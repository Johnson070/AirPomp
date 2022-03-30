
#include <GyverPortal.h>
#include <WiFiClientSecure.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPClient.h>
#include <PCF8574.h>
#include "MEL_work.h"
#include "local.h"
#include "files_work.h"
#include "PortalAttach.h"

GyverPortal portal;
PCF8574 expander;

bool APMode;

unsigned long lastTime = 0;
//unsigned long timerDelay = 15000;

extern struct MEL_data cond_data[MAX_MEL_DEVICES];
extern struct Settings_struct cred;

void ConnectWIFI() {
  // ---------------This was the magic WiFi reconnect fix for me
  WiFi.hostname("WaterPomp_ESP-01");
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);   // this is a temporary line, to be removed after SDK update to 1.5.4
  WiFi.mode(WIFI_STA);
  // ---------------END - WiFi reconnect fix

  WiFi.begin(cred.SSID, cred.SSIDPass);
  Serial.println("Connecting");
  //expander.digitalWrite(7,LOW);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    //expander.blink(5,1,300);
    count++;

    if (count == 16)
      goto APModeGoto;
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());



  if (false) {
APModeGoto:
    APMode = true;
    //expander.digitalWrite(7,HIGH);
    WiFi.mode(WIFI_AP);
    WiFi.softAP((const char*)"WaterPomp", (const char*)"12345678");
  }

  if (!MDNS.begin("waterpomp")) {  // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  else {
    MDNS.addService("http", "tcp", 80);

    Serial.println("mDNS responder started");
  }
}

unsigned long enabTimeMillis = 0;
unsigned long stbTimeMillis = 0;

void setup() {
  Serial.begin(115200);
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  pinMode(2,OUTPUT);

  //expander.begin(0x20);

  for (uint8_t i = 0; i < 8; i++){
    expander.pinMode(i, OUTPUT);
    expander.digitalWrite(i,LOW);
  }

  expander.blink(5,3,300);
  expander.blink(6,3,300);
  expander.blink(7,3,300);

  if (LittleFS.begin()) {
    Serial.println("LittleFS Initialize....ok");
  } else {
    Serial.println("LittleFS Initialization...failed");
  }

  ReadSettingFromFS();

  ConnectWIFI();

  if (!APMode) {
    for (int i = 0; i < 5; i++)
      if (LoginMELCloud()) break;

    for (int i = 0; i < MAX_MEL_DEVICES; i++)
      if (cond_data[i].id != "")
        for (int j = 0; j < 5; j++)
          if (UpdateCond(cond_data[i].id)) break;

    for (int i = 0; i < 5; i++)
      if (GetDataFromMELCloud()) {
        bool notFull = false;
        for (int j = 0; j < MAX_MEL_DEVICES; j++)
          if (cond_data[j].id == "" && cond_data[j].mac != ""){
            notFull = true;
            break;
          }
        if (!notFull) break;
      }
  }
  ReadCondsFromFS();

  // подключаем билдер и запускаем
  portal.attachBuild(build);
  portal.attachClick(myClick);
  portal.attachUpdate(myUpdate);
  portal.attachForm(myForm);
  portal.start();

  lastTime = millis();
  enabTimeMillis = millis();
}

bool enabState = true;
bool stdState = false;

bool enabStateEnd = false;
bool stdStateEnd = false;



void loop() {
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && !APMode) APMode = true;
    
    portal.tick();
    MDNS.update();
    ESP.wdtFeed();

    if (!stdState && (millis() - enabTimeMillis) <= cred.enabTime && enabState && !enabStateEnd) {
      for (int i = 0; i < MAX_MEL_DEVICES; i++)
        if (cond_data[i].state)
          digitalWrite(2,HIGH);//Serial.println("enable " + cond_data[i].name + " ENABLE");
      enabState = false;
      //Serial.println();
    }
    if (!enabStateEnd && (millis() - enabTimeMillis) > cred.enabTime && !enabState && !enabStateEnd) {
      stbTimeMillis = millis();
      enabStateEnd = true;
      stdState = true;
      stdStateEnd = false;
      //Serial.println("NEXT STB");
    }

    if (!enabState && (millis() - stbTimeMillis) <= cred.stbTime && stdState && !stdStateEnd) {
      for (int i = 0; i < MAX_MEL_DEVICES; i++)
        if (cond_data[i].state)
          digitalWrite(2,LOW);//Serial.println("standby " + cond_data[i].name + " \r\rDISABLE");
      stdState = false;
      //Serial.println();
    }
    if (!enabState && (millis() - stbTimeMillis) > cred.stbTime && !stdState && !stdStateEnd) {
      enabTimeMillis = millis();
      stdStateEnd = true;
      enabState = true;
      enabStateEnd = false;
      //Serial.println("NEXT ENAB");
    }

    if (millis() - lastTime >= 15000) {
      if (!APMode) {
        if (cred.autoPowerPomp) {
          expander.toggle(6);
          for (int i = 0; i < MAX_MEL_DEVICES; i++)
            if (cond_data[i].id != "")
              UpdateCond(cond_data[i].id);
  
          delay(500);
          //portal.tick();
          ESP.wdtFeed();
  
          for (int i = 0; i < MAX_MEL_DEVICES; i++)
            if (cond_data[i].id != "")
              UpdateCondData(cond_data[i].id);

          expander.toggle(6);
        }
      }
      else {
        int n = WiFi.scanNetworks();
        for (int i = 0; i < n; i++)
        {
          Serial.println(String(i) + ") " + WiFi.SSID(i));
          if (WiFi.SSID(i) == cred.SSID) {
            APMode = false;
            ConnectWIFI();
          }
        }
        Serial.println();
      }

      lastTime = millis();
    }

    

    if (millis() / 1000 / 60 / 60 >= 24) ESP.restart();
  }
}
