#include "Arduino.h"
#include "time.h"
#include <chrono>
#include <ESP32Servo.h>
// brownout
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "./const_dev.h"

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  5
#define TIME_LIGHT_SLEEP 60
#define PIN_FT90B GPIO_NUM_13
#define PIN_TRANSISTOR GPIO_NUM_2
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
    gpio_hold_dis(PIN_TRANSISTOR);
    pinMode(PIN_TRANSISTOR, OUTPUT);
    digitalWrite(PIN_TRANSISTOR, HIGH);
    delay(1000);
    digitalWrite(PIN_TRANSISTOR, LOW);
    delay(500);
    digitalWrite(PIN_TRANSISTOR, HIGH);

    gpio_hold_en(PIN_TRANSISTOR);
    esp_sleep_enable_timer_wakeup(TIME_LIGHT_SLEEP*uS_TO_S_FACTOR); //120 seconds
    int ret = esp_light_sleep_start(); // 
    DBG("end of light sleep");
    gpio_hold_dis(PIN_TRANSISTOR);
    ft90b.setPeriodHertz(50); // Fréquence PWM pour le FT90B
    ft90b.attach(PIN_FT90B, 500, 2500);
    ft90b.write(0);

//Heure de réveil
    struct tm time_reveil = {0};
    time_reveil.tm_hour = 12;       // Heures = 12
    time_reveil.tm_min = 0;         // Minutes = 0
    time_reveil.tm_sec = 0;         // Secondes = 0
//Initalisation debug
#ifdef SERIAL_DEBUG
    Serial.begin(115200);
    Serial.setDebugOutput(true);
#endif

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
                takePicture(); //uncomment this line to send photo
            }
    }
    else{
        DBG("connection lost");
    }
    
    ft90b.write(0);
    delay(100);
    digitalWrite(PIN_TRANSISTOR, LOW);
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR);
    gpio_hold_en(PIN_TRANSISTOR);
    gpio_deep_sleep_hold_en();
    DBG("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +  " Seconds");
    DBG("Going to sleep now");
    esp_deep_sleep_start();

}

void loop() {

}
