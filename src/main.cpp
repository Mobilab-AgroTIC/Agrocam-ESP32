#include "Arduino.h"
#include "time.h"
#include <chrono>
#include <ESP32Servo.h>

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "./const_dev.h"

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  10
#define TIME_LIGHT_SLEEP 5
#define PIN_FT90B GPIO_NUM_13
#define PIN_TRANSISTOR GPIO_NUM_2
#define PIN_BATTERY_LEVEL GPIO_NUM_15
#define PIN_TRANSISTOR_BC550 GPIO_NUM_12

#define ADC2_CHAN_BATTERY  ADC2_CHANNEL_3
#define ADC_ATTEN   ADC_ATTEN_DB_11
static esp_adc_cal_characteristics_t adc2_chars;

RTC_DATA_ATTR int bootCount = 0;

#if defined(SERIAL_DEBUG)
    #define DBG(x) Serial.println(x)
#else
    #define DBG(...)
#endif

#include "./connect.h"
#include "./ftp.h"
#include "./cam.h"

Servo ft90b;

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector
    //Initalisation debug
    #ifdef SERIAL_DEBUG
        Serial.begin(115200);
        Serial.setDebugOutput(true);
    #endif
    gpio_hold_dis(PIN_TRANSISTOR);
    gpio_hold_dis(PIN_TRANSISTOR_BC550);
    //pinMode(PIN_BATTERY_LEVEL, INPUT);
    
    pinMode(PIN_TRANSISTOR, OUTPUT);
    pinMode(PIN_TRANSISTOR_BC550, OUTPUT);
    pinMode(PIN_FT90B, OUTPUT);

    uint32_t voltage = 0;
    esp_adc_cal_characterize(ADC_UNIT_2, ADC_ATTEN, ADC_WIDTH_BIT_12, 1100, &adc2_chars);
    adc2_config_channel_atten(ADC2_CHAN_BATTERY, ADC_ATTEN);

    for (int i =0; i<100; i++){
        //int rawBattery = adc2_get_raw(ADC2_GPIO15_CHANNEL);
        //float batteryLevel = 2*rawBattery/4095.0*3.3;
        int adc_raw;
        adc2_get_raw(ADC2_CHAN_BATTERY, ADC_WIDTH_BIT_12, &adc_raw);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_raw, &adc2_chars);
        uint32_t voltage2 = adc_raw *2450/4095;
        DBG("niveau de batterie 12 bits : "+String(adc_raw)+" -- Batterie en volt : "+String(voltage2));
        delay(1000);
    }

    gpio_hold_en(PIN_TRANSISTOR);
    esp_sleep_enable_timer_wakeup(TIME_LIGHT_SLEEP*uS_TO_S_FACTOR); //120 seconds
    int ret = esp_light_sleep_start(); 
    DBG("end of light sleep");
    digitalWrite(PIN_TRANSISTOR_BC550, HIGH);
    delay(1000);
    ft90b.setPeriodHertz(50); // Fréquence PWM pour le FT90B
    ft90b.attach(PIN_FT90B, 500, 2500);
    ft90b.write(0);

//Mesure tension
    int rawBattery = analogRead(PIN_BATTERY_LEVEL);
    float batteryLevel = 2*rawBattery/4095.0*3.3;

//Heure de réveil
    struct tm time_reveil = {0};
    time_reveil.tm_hour = 12;       // Heures = 12
    time_reveil.tm_min = 0;         // Minutes = 0
    time_reveil.tm_sec = 0;         // Secondes = 0

//Boot Count
    ++bootCount;
    DBG("Boot number : " + String(bootCount));
    long int duree_sommeil = TIME_TO_SLEEP;

//Prise de photo
    ft90b.write(90);
    
    wifiInit();
    wifiConnect();
    unsigned long current_millis = millis();
    while (!wifi_connected) {
        current_millis = millis();
        if (current_millis - connect_timer >= WIFI_CONNECT_INTERVAL) {
            connect_timer = current_millis;
            wifiConnect();
        }
    }
    if (wifi_connected){
        if (!cam_init_ok) {
                cam_init_ok = cameraInit();
            }

            if (cam_init_ok) {
                //Récupérer heure réseau
                configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
                struct tm timeinfo;
                getLocalTime(&timeinfo);
                time_reveil.tm_year = timeinfo.tm_year;
                time_reveil.tm_mon = timeinfo.tm_mon;
                time_reveil.tm_mday = timeinfo.tm_mday + 1;
                time_t reveil = mktime(&time_reveil);
                time_t actuel = mktime(&timeinfo);
                DBG("date actuelle : ");
                DBG(&timeinfo);
                DBG("date de réveil : ");
                DBG(&time_reveil);
                duree_sommeil = reveil-actuel;
                DBG("duree sommeil : " + String(duree_sommeil));
                DBG("time : " + String(timeinfo.tm_hour) + ":" + String(timeinfo.tm_min) + ":" + String(timeinfo.tm_sec));
                //Prendre photo
                //takePicture(); //uncomment this line to send photo
            }
    }
    else{
        DBG("connection lost");
    }
    
    
    ft90b.write(0);
    ft90b.detach();
    delay(100);
    gpio_hold_dis(PIN_TRANSISTOR);
    digitalWrite(PIN_TRANSISTOR, LOW);
    digitalWrite(PIN_TRANSISTOR_BC550, LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR);
    gpio_hold_en(PIN_TRANSISTOR);
    gpio_hold_en(PIN_TRANSISTOR_BC550);
    gpio_deep_sleep_hold_en();
    DBG("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +  " Seconds");
    DBG("Going to sleep now");
    esp_deep_sleep_start();

}

void loop() {

}
