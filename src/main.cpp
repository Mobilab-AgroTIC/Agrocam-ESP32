#include "Arduino.h"
#include "time.h"
#include <chrono>
// brownout
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#include "./const_dev.h"

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  120
RTC_DATA_ATTR int bootCount = 0;

#if defined(SERIAL_DEBUG)
    #define DBG(x) Serial.println(x)
#else
    #define DBG(...)
#endif

#include "./connect.h"
#include "./ftp.h"
#include "./cam.h"

void setup() {
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

    pinMode(4, OUTPUT);
    digitalWrite(4, HIGH);

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
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP*uS_TO_S_FACTOR);

//Prise de photo
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
                long int duree_sommeil = reveil-actuel;
                DBG("duree sommeil : " + String(duree_sommeil));
                DBG("heures : " + String(timeinfo.tm_hour) + " minutes : " + String(timeinfo.tm_min) + " secondes : " + String(timeinfo.tm_sec));
                //Prendre photo
                takePicture(); //uncomment this line to send photo
            }
    }
    else{
        DBG("connection lost");
    }

    digitalWrite(4, LOW);
    gpio_hold_en(GPIO_NUM_4);
    gpio_deep_sleep_hold_en();
    DBG("Setup ESP32 to sleep for " + String(TIME_TO_SLEEP) +  " Seconds");
    DBG("Going to sleep now");
    esp_deep_sleep_start();

}

void loop() {

}
