//{"SSID":"Home","SSIDPass":"89052299312","email":"anton.shokin@gmail.com","pass":"Qawsed123"}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <ESPAsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <Wire.h>    // Required for I2C communication
#include <PCF8574.h>
#include "Ticker.h"
#define MaxMELDevices 5

PCF8574 expander;
Ticker PowerPomps;
//Ticker PowerOffPomps;

String MELCloud = "https://app.melcloud.com/Mitsubishi.Wifi.Client";
unsigned long lastTime = 0;
unsigned long timerDelay = 15000;

AsyncWebServer server(80);
String pomps[MaxMELDevices+3];
String rooms[MaxMELDevices+3];
uint8_t pins[MaxMELDevices+3];
bool pompsState[MaxMELDevices+3];
bool APMode;
bool autoMode = true;
bool waitMode = false;
int countReboot = 0;

bool SDTPomp = false;
bool EnabledPomp = false;


struct {
  String SSID;
  String SSIDPass;
  String email;
  String pass;
  String token;
  String user;
  String devices;
  int enabTime = 10000;
  int perTime = 5000;
} cred;

bool deleteWater = false;
bool deleteWater1 = false;
unsigned long lastTimeDelWater = 0;
unsigned long timerDelayDelWater = 10000;

unsigned long lastTimeDelWater1 = 0;
unsigned long timerDelayDelWater1 = 5000;

unsigned long lastSTDPomp = 0;
unsigned long lastEnablePomp = 0;


void notFound(AsyncWebServerRequest *request) {
  Serial.println(request->url());
  request->send(404, "text/plain", "Not found");
}

String processor(const String& var) {
  if (var == "SSID") return cred.SSID;
  else if (var == "SSIDPASS") return cred.SSIDPass;
  else if (var == "USER") return cred.user;
  else if (var == "EMAIL") return cred.email;
  else if (var == "PASS") return cred.pass;
  else if (var == "DEVICES") return "-----";
  else if (var == "TOKEN") return cred.token;
  else if (var == "enabTime") return String(timerDelayDelWater);
  else if (var == "perTime") return String(timerDelayDelWater1);
  else if (var == "UPTIME") return String(millis()/1000/60);
  else return "NODATA";
}

bool LoginMELCloud() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.connect(MELCloud + "/Login/ClientLogin", 433);
    HTTPClient http;

    http.begin(client, MELCloud + "/Login/ClientLogin");

    // If you need an HTTP request with a content type: application/json, use the following:
    http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:73.0) Gecko/20100101 Firefox/73.0");
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(
                             "{\"Email\":\"" + cred.email + "\", \"Password\":\"" + cred.pass + "\",\"Language\":16,\"AppVersion\":\"1.19.1.1\",\"Persist\":true, \"CaptchaResponse\":null}");

    if (httpResponseCode == 200) {
      //Get the request response payload
      String payload = http.getString();
      //Print the response payload
      Serial.println(payload);
      http.end();
      
      String searchWords[3];
      searchWords[0] = '"';
      searchWords[0] += "ErrorId";
      searchWords[0] += '"';
      searchWords[0] += ':';
      searchWords[1] = '"';
      searchWords[1] += "ContextKey";
      searchWords[1] += '"';
      searchWords[1] += ':';
      searchWords[2] = '"';
      searchWords[2] += "Name";
      searchWords[2] += '"';
      searchWords[2] += ':';
      searchWords[2] += '"';
       

      int idxWord = payload.indexOf(searchWords[0]);
      int lenData = 0;
      for (int i = idxWord; i < payload.length();i++)
        if (payload[i] != ',') lenData++;
        else break;
      String ErrorID = payload.substring(idxWord+searchWords[0].length(),idxWord+lenData);
      
      if (ErrorID == "null") {
        idxWord = payload.indexOf(searchWords[1]);
        lenData = 0;
        for (int i = idxWord; i < payload.length();i++)
          if (payload[i] != ',') lenData++;
          else break;
        cred.token = payload.substring(idxWord+searchWords[1].length()+1,idxWord+lenData-1);
  
        idxWord = payload.indexOf(searchWords[2]);
        lenData = 0;
        for (int i = idxWord; i < payload.length();i++)
          if (payload[i] != ',') lenData++;
          else break;
        cred.user = payload.substring(idxWord+searchWords[2].length(),idxWord+lenData-1);
      
        Serial.println(cred.token);
        Serial.println(cred.user);
        
        return true;
      }
      else {
        Serial.println("Error connect to MELCloud!");
        Serial.print(ErrorID);
        http.end();
        return false;
      }
    }
    else {
      Serial.println("Error connect to MELCloud!");
      Serial.print(httpResponseCode);
      http.end();
      return false;
    }
  }
  else {
    return false;
  }
}

String GetDataFromMELCloud() {
  //Check WiFi connection status
  if (WiFi.status() == WL_CONNECTED) {
    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.connect(MELCloud + "/User/ListDevices", 433);
    HTTPClient http;

    http.begin(client, MELCloud + "/User/ListDevices");

    http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:73.0) Gecko/20100101 Firefox/73.0");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Accept", "application/json, text/javascript, */*; q=0.01");
    http.addHeader("Accept-Language", "en-US,en;q=0.5");
    http.addHeader("Accept-Encoding", "gzip, deflate, br");
    http.addHeader("X-MitsContextKey", cred.token);
    http.addHeader("X-Requested-With", "XMLHttpRequest");
    http.addHeader("Cookie", "policyaccepted=true");
    int httpResponseCode = http.GET();

    if (httpResponseCode == 200) {
      int countDevice;
      int len = http.getSize();

      // create buffer for read
      uint8_t buff[128] = { 0 };

      // get tcp stream
      WiFiClient* stream = http.getStreamPtr();

      // read all data from server
      String buffBlock = "";
      int lenBlock = 128;
      uint8_t maxLenWord = 50;
      uint8_t countWords = 6;
      String searchWords[countWords] = { "", "", "", "", "" };
      searchWords[0] = "Devices";
      searchWords[0] += '"';
      searchWords[0] += ":[{";
      searchWords[1] = "DeviceName";
      searchWords[1] += '"';
      searchWords[1] += ':';
      searchWords[2] = '"';
      searchWords[2] += "Device";
      searchWords[2] += '"';
      searchWords[2] += ":{";
      searchWords[3] = '"';
      searchWords[3] += "Power";
      searchWords[3] += '"';
      searchWords[3] += ':';
      searchWords[4] = '"';
      searchWords[4] += "RoomTemperature";
      searchWords[4] += '"';
      searchWords[4] += ':';
      searchWords[5] = '"';
      searchWords[5] += "MacAddress";
      searchWords[5] += '"';
      searchWords[5] += ':';

      uint8_t searchIdx = 0;
      String out;
      while (http.connected() && (len > 0 || len == -1)) {
        // get available data size
        ESP.wdtFeed();
        size_t size = stream->available();

        if (size) {
          // read up to 128 byte
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

          String k = "";

          for (int i = 0; i < c; i++)
            k += (char)buff[i];


          if (buffBlock == "") buffBlock = k;
          else {
            String buffK = k;
            k = buffBlock + buffK;
            buffBlock = buffK;
          }

          if (searchIdx > countWords - 1) searchIdx = 0;

          for (int i = searchIdx; i < countWords; i++) {
            int idx = k.indexOf(searchWords[i]);

            if (idx != -1) {
              //                              Serial.print(idx);
              //              Serial.print("  ");
              //              Serial.print(i);
              //              Serial.print("  ");
              //              Serial.print(searchIdx);float css
              //              Serial.print("  ");
              //              Serial.print(searchWords[i]);
              //              Serial.print("  ");
              //              Serial.println(k);
              searchIdx += 1;
              int lenData = 0;

              if (k.length() - idx < maxLenWord) {
                searchIdx--;
                break;
              }

              for (int y = idx; y < k.length(); y++) {
                if (k[y] != ',') lenData += 1;
                else break;

                ESP.wdtFeed();
              }

              //if (i == 1 || i == 3 || i == 4) Serial.println(k.substring(idx + searchWords[i].length(), idx + lenData));
              //                if (i == 0) doc_device = doc.createNestedObject();
              //                if (i==1) doc_device["name"] = k.substring(idx + searchWords[i].length() + 1, idx + lenData - 1);
              //                if (i==3) doc_device["state"] = k.substring(idx + searchWords[i].length(), idx + lenData);
              //                if (i==4) doc_device["temp"] = k.substring(idx + searchWords[i].length(), idx + lenData);
              //                if (i==5) doc_device["mac"] = k.substring(idx + searchWords[i].length() + 1, idx + lenData - 1);
              //if (i == 0) doc_device = doc.createNestedObject();
              if (i == 1) out += k.substring(idx + searchWords[i].length() + 1, idx + lenData - 1) + "$";
              else if (i == 3) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 4) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 5) out += k.substring(idx + searchWords[i].length() + 1, idx + lenData - 1) + "$";
              else break;
            }
            else break;
          }

          if (len > 0) {
            len -= c;
          }
          delay(5); 
        }
      }
      http.end();
      StaticJsonDocument<1024> docJson;
      JsonObject doc_device;


      uint8_t codeWord = 0;
      String wordInStr;
      uint8_t roomIdx = 0;
      for (int i = 0; i < out.length(); i++) {
        if (out[i] == '$') {
          if (codeWord == 0) {
            doc_device = docJson.createNestedObject();
            doc_device["name"] = wordInStr;
            rooms[roomIdx] = wordInStr;
            roomIdx++;
          }
          else if (codeWord == 1) doc_device["state"] = wordInStr;
          else if (codeWord == 2) doc_device["temp"] = wordInStr;
          else if (codeWord == 3) doc_device["mac"] = wordInStr;

          if (codeWord < 3) codeWord++;
          else codeWord = 0;
          wordInStr = "";
        } else wordInStr += out[i];
      }

      if (autoMode)
        for (JsonObject elem : docJson.as<JsonArray>()) {
          SetPompMode(elem["mac"].as<String>(), elem["state"].as<String>() == "true" ? true : false);
        }

      String jsonOut;
      serializeJson(docJson, jsonOut);

      if (jsonOut != "") {
        //Serial.print("HTTP Response code: ");
        //Serial.println(httpResponseCode);

        // Free resources
        

        return jsonOut;
      }

      return "";
    }

    //Serial.print("HTTP Response code: ");
    // Serial.println(httpResponseCode);

    // Free resources
    http.end();
  } else {
    Serial.println("WiFi Disconnected");
    ConnectWIFI();
  }

  return "";
}

void SaveSettingsToFS() {
  File settings = LittleFS.open("settings.json", "w");
  if (settings) {
    StaticJsonDocument<512> docSettings;
    docSettings["SSID"] = cred.SSID;
    docSettings["SSIDPass"] = cred.SSIDPass;
    docSettings["email"] = cred.email;
    docSettings["pass"] = cred.pass;
    docSettings["tm1"] = timerDelayDelWater;
    docSettings["tm2"] = timerDelayDelWater1;

    String jsonSettings;
    serializeJson(docSettings, jsonSettings);

    settings.print(jsonSettings);
    settings.close();
  }
}

void SavePins(String pomps_save[], uint8_t pins_save[], String names[], uint8_t count) {
  StaticJsonDocument<1024> doc;
  String json;


  File settings = LittleFS.open("/pomps.json", "r");
  if (settings) {
    String json;
    while (settings.available()) {
      json += (char)settings.read();
    }
  }

  settings = LittleFS.open("/pomps.json", "w");

  if (settings) {
    DeserializationError error = deserializeJson(doc, json);
    JsonObject doc_device;

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
      doc.clear();

      for (uint8_t i = 0; i < count; i++) {
        doc_device = doc.createNestedObject();
        doc_device["mac"] = pomps_save[i];
        doc_device["pin"] = pins_save[i];
        doc_device["name"] = names[i];
      }
    }
    else {
      for (uint8_t i = 0; i < count; i++) {
        bool match;
        for (JsonObject elem : doc.as<JsonArray>()) {
          if (doc["mac"] == pomps_save[i]) {
            doc["pin"] = pins_save[i];
            match = true;
          }
        }
        if (!match) {
          doc_device = doc.createNestedObject();
          doc_device["mac"] = pomps_save[i];
          doc_device["pin"] = pins_save[i];
          doc_device["name"] = names[i];
        }
      }
    }



    String out;
    serializeJson(doc, out);
    Serial.println(out);
    settings.print(out);

    settings.close();
  }
}

void ConnectWIFI() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(cred.SSID.c_str(), cred.SSIDPass.c_str());
  Serial.println("Connecting");
  expander.digitalWrite(7,LOW);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    expander.blink(5,1,300);
    count++;

    if (count == 25)
      goto APModeGoto;
  }

  LoginMELCloud();

  if (false) {
APModeGoto:
    APMode = true;
    expander.digitalWrite(7,HIGH);
    WiFi.mode(WIFI_AP);
    WiFi.softAP((const char*)"WaterPump", (const char*)"Qawsed123");
  }
}

void SetPompMode(String mac, bool state) {
  for (int i = 0; i < MaxMELDevices; i++) {
    if (pomps[i] != "") {
      for (int y = 1; y < MaxMELDevices+1; y++) {
        if (pomps[i] == mac && pins[i] == y) {
          pompsState[pins[i]-1] = state;
//          Serial.print(pomps[i]);
//            Serial.print(" ");
//            Serial.print(mac);
//            Serial.print(" ");
//            Serial.print(pins[i]);
//            Serial.print(" ");
//            Serial.println(i);
        }
      }
    }
  }
}

void SDTTimer(){
  SDTPomp = true;
}

void PompTimer() {
  EnabledPomp = true;
}

//void PompTicker(){
////  bool pompsStateOLD[MaxMELDevices+3];
////  for (int i = 0; i < MaxMELDevices+3;i++)
////    pompsStateOLD[i] = 
//  
//  for (int i = 0; i < MaxMELDevices;i++)
//      if (pompsState[i]) {
//        expander.digitalWrite(i,HIGH);
//        delay(pwmT2-pwmT1);
//        expander.digitalWrite(i,LOW);
//        delay(pwmT1);
//      }
//}

void setup() {
  Serial.begin(115200);
  delay(500);
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);

  expander.begin(0x20);

  if (LittleFS.begin()) {
    Serial.println("LittleFS Initialize....ok");
  } else {
    Serial.println("LittleFS Initialization...failed");
  }

  for (uint8_t i = 0; i < MaxMELDevices+3; i++)
    expander.pinMode(i, OUTPUT);

  expander.blink(5,3,300);
  expander.blink(6,3,300);
  expander.blink(7,3,300);

  File settings = LittleFS.open("/settings.json", "r");
  if (settings) {
    String json;
    while (settings.available()) {
      json += (char)settings.read();
    }
    Serial.println(json);
    settings.close();
    StaticJsonDocument<512> jsonSettings;

    DeserializationError error = deserializeJson(jsonSettings, json);

    if (error) {
      Serial.println(error.c_str());
    }

    cred.SSID = jsonSettings["SSID"].as<String>();
    cred.SSIDPass = jsonSettings["SSIDPass"].as<String>();
    cred.email = jsonSettings["email"].as<String>();
    cred.pass = jsonSettings["pass"].as<String>();
    cred.enabTime = jsonSettings["tm1"].as<int>();
    cred.perTime = jsonSettings["tm2"].as<int>();
    timerDelayDelWater = cred.enabTime;
    timerDelayDelWater1 = cred.perTime;
  }

  File pompsFile = LittleFS.open("/pomps.json", "r");
  if (pompsFile) {
    String json;
    while (pompsFile.available()) {
      json += (char)pompsFile.read();
    }
    Serial.println(json);
    pompsFile.close();
    StaticJsonDocument<768> jsonPomps;

    DeserializationError error = deserializeJson(jsonPomps, json);

    if (error) {
      Serial.println(error.c_str());
    }

    uint8_t count = 0;
    for (JsonObject elem : jsonPomps.as<JsonArray>()) {

      pomps[count] = elem["mac"].as<String>(); // "323423423423423", "323423423423423", "323423423423423", ...
      pins[count] = elem["pin"].as<uint8_t>(); // 8, 8, 8, 8, 8, 8, 8, 8
      count++;
    }

    for (uint8_t i = count; i < MaxMELDevices; i++)
    {
      pomps[count] = "";
      pins[count] = 255;
    }
  }

  for (uint8_t i = 0; i < MaxMELDevices; i++) {
    Serial.print(pomps[i]);
    Serial.print(" ");
    Serial.print(pins[i]);
    Serial.println();
  }

  ConnectWIFI();


  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("waterpump")) {  // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  MDNS.addService("http", "tcp", 80);

  Serial.println("mDNS responder started");

  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send(LittleFS, "/index.html", String(), false, processor);
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/getpomp", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    for (uint8_t i = 0; i < MaxMELDevices; i++)
      if (pomps[i] == (request->getParam(0))->value())
        request->send_P(200, "text/plain", pompsState[i] == true ? "1" : "0");

  });
  server.on("/autoupdate", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send_P(200, "text/plain", autoMode == true ? "1" : "0");
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send_P(200, "text/plain", (String(ESP.getFreeHeap())).c_str());
  });
  server.on("/autoupdate", HTTP_POST, [](AsyncWebServerRequest * request) {
    while (waitMode);
    autoMode = ((request->getParam(0))->value()) == "1" ? true : false;
    request->send_P(200, "text/plain", "");
  });
  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    if (APMode) request->send_P(200, "text/plain", "");
    else request->send_P(200, "text/plain", cred.devices.c_str());
  });
  server.on("/getpins", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
      String out = "[";
      for (uint8_t i = 0; i < MaxMELDevices; i++) {
        out += (pompsState[i] == true ? "1" : "0");
        if (i < MaxMELDevices-1) out += ",";
        else out += "]";
      }
      request->send_P(200, "text/plain", out.c_str());
    
  });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send_P(200, "text/plain", "");
    ESP.restart();
  });
  server.on("/settimer", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    timerDelayDelWater = (request->getParam(0))->value().toInt();
    timerDelayDelWater1 = (request->getParam(1))->value().toInt();

    SaveSettingsToFS();
  
    request->send_P(200, "text/plain", "");
  });
  server.on("/setpomp", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    SetPompMode((request->getParam(0))->value(), ((((request->getParam(1))->value())).toInt() == 1 ? true : false));

    request->send_P(200, "text/plain", "");
  });
  server.on("/pomps", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    request->send(LittleFS, "/pomps.json", "text/plain");
  });
  server.on("/saveDevices", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
    int paramsNr = request->params();


    for (int i = 0; i < paramsNr; i++) {

      AsyncWebParameter* p = request->getParam(i);

      Serial.print("Param name: ");
      Serial.println(p->name());

      Serial.print("Param value: ");
      Serial.println(p->value());

      Serial.println("------");

      pomps[i] = p->name();
      pins[i] = (p->value()).toInt();
    }

    SavePins(pomps, pins, rooms, paramsNr);

    request->send_P(200, "text/plain", "");
  });
  server.on("/saveWifi", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
//    int paramsNr = request->params();
////    for (int i = 0; i < paramsNr; i++) {
////
////      AsyncWebParameter* p = request->getParam(i);
//////
//////      Serial.print("Param name: ");
//////      Serial.println(p->name());
//////
//////      Serial.print("Param value: ");
//////      Serial.println(p->value());
//////
//////      Serial.println("------");
////    }

    cred.SSID = ((request->getParam(0))->value());
    cred.SSIDPass = ((request->getParam(1))->value());

    SaveSettingsToFS();

    request->send_P(200, "text/plain", "");
  });
  server.on("/saveMEL", HTTP_GET, [](AsyncWebServerRequest * request) {
    while (waitMode);
//    int paramsNr = request->params();
//    for (int i = 0; i < paramsNr; i++) {
//
//      AsyncWebParameter* p = request->getParam(i);
//
////      Serial.print("Param name: ");
////      Serial.println(p->name());
////
////      Serial.print("Param value: ");
////      Serial.println(p->value());
////
////      Serial.println("------");
//    }

    cred.email = ((request->getParam(0))->value());
    cred.pass = ((request->getParam(1))->value());

    SaveSettingsToFS();

    request->send_P(200, "text/plain", "");
  });
  server.onNotFound(notFound);
  
  server.begin();

  for (uint8_t i = 0; i < 5; i++)
    if ((WiFi.status() != WL_CONNECTED || cred.token == "") && LoginMELCloud()) break;

  //PowerPomps.attach_ms((pwmT2-pwmT1)+pwmT1, PompTicker);
}



void loop() {
  delay(10); 
  MDNS.update();
  ESP.wdtFeed();

  if (millis() - lastSTDPomp >= 20*60*1000) {
    SDTTimer();
    lastSTDPomp = millis();
  }

  if (millis() - lastEnablePomp >= timerDelayDelWater) {
    PompTimer();
    lastEnablePomp = millis();
  }

  if (SDTPomp && !deleteWater) {
    for (int i = 0; i < MaxMELDevices; i++)
      if (pompsState[i] == false)
        expander.digitalWrite(i,HIGH);
    
    lastTimeDelWater = millis();
    deleteWater = true;
    SDTPomp = false;
  }
  else if ((millis() - lastTimeDelWater) > timerDelayDelWater && deleteWater) {
    for (int i = 0; i < MaxMELDevices; i++)
      if (pompsState[i] == false)
        expander.digitalWrite(i,LOW);

    deleteWater = false;
  }

  if (EnabledPomp && !deleteWater1) {
    for (int i = 0; i < MaxMELDevices; i++)
      if (pompsState[i] == true)
        expander.digitalWrite(i,HIGH);
    
    lastTimeDelWater1 = millis();
    deleteWater1 = true;
    EnabledPomp = false;
  }
  else if ((millis() - lastTimeDelWater1) > timerDelayDelWater1 && deleteWater1) {
    for (int i = 0; i < MaxMELDevices; i++)
        expander.digitalWrite(i,LOW);

    deleteWater1 = false;
  }
  
  if ((millis() - lastTime) > timerDelay) {
    countReboot++;
//    for (int i = 0; i < MaxMELDevices; i++)
//      expander.digitalWrite(i,pompsState[i]);
    
    if (ESP.getFreeHeap() < 3000) {
      Serial.println("ESP REBOOT HEAP IS SMALL!!!");
      Serial.print("HEAP: ");
      Serial.print(ESP.getFreeHeap());
      Serial.print(" kb");
      ESP.restart();
    }

    if (!APMode) {
      waitMode = true;
      expander.toggle(6);
      delay(100);
      cred.devices = GetDataFromMELCloud();
      //server.begin();
      delay(100);
      expander.toggle(6);
      waitMode = false;
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
    //Serial.println(cred.devices);
    lastTime = millis();

    if (countReboot == 2160) ESP.restart();
  }
}
