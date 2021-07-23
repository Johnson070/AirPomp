//{"SSID":"Home","SSIDPass":"89052299312","email":"anton.shokin@gmail.com","pass":"Qawsed123"}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <LittleFS.h>
#include <ESPAsyncTCP.h>
#include "ESPAsyncWebServer.h"
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>    // Required for I2C communication
#include <PCF8574.h>

#define MaxMELDevices 8

PCF8574 expander;
String MELCloud = "https://app.melcloud.com/Mitsubishi.Wifi.Client";


unsigned long lastTime = 0;
unsigned long timerDelay = 5000;
AsyncWebServer server(80);
String pomps[MaxMELDevices];
String rooms[MaxMELDevices];
uint8_t pins[MaxMELDevices];
bool pompsState[MaxMELDevices];
bool APMode;
bool autoMode = true;

struct {
  String SSID;
  String SSIDPass;
  String email;
  String pass;
  String token;
  String user;
  String devices;
} cred;

String processor(const String& var) {
  if (var == "SSID") return cred.SSID;
  else if (var == "SSIDPASS") return cred.SSIDPass;
  else if (var == "USER") return cred.user;
  else if (var == "EMAIL") return cred.email;
  else if (var == "PASS") return cred.pass;
  else if (var == "DEVICES") return "-----";
  else if (var == "TOKEN") return cred.token;
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
      DynamicJsonDocument jsonBuffer(2048);
      DeserializationError error = deserializeJson(jsonBuffer, payload.c_str());

      if (error) {
        Serial.println(error.c_str());
        http.end();
        return false;
      }

      if (jsonBuffer["ErrorId"].as<int>() == 0) {
        cred.token = jsonBuffer["LoginData"]["ContextKey"].as<String>();
        cred.user = jsonBuffer["LoginData"]["Name"].as<String>();
        Serial.println(cred.token);
        Serial.println(cred.user);
        http.end();
        return true;
      }
      else {
        Serial.println("Error connect to MELCloud!");
        Serial.print(jsonBuffer["ErrorId"].as<int>());
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
      uint8_t buff[256] = { 0 };

      // get tcp stream
      WiFiClient* stream = http.getStreamPtr();

      // read all data from server
      String buffBlock = "";
      int lenBlock = 256;
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
        }
        delay(1);
      }

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
        http.end();

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
    StaticJsonDocument<256> docSettings;
    docSettings["SSID"] = cred.SSID;
    docSettings["SSIDPass"] = cred.SSIDPass;
    docSettings["email"] = cred.email;
    docSettings["pass"] = cred.pass;

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

  int count = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");

    count++;

    if (count == 25)
      goto APModeGoto;
  }

  LoginMELCloud();

  if (false) {
APModeGoto:
    APMode = true;
    WiFi.mode(WIFI_AP);
    WiFi.softAP((const char*)"WaterPump", (const char*)"Qawsed123");
  }
}

void SetPompMode(String mac, bool state) {
  for (int i = 0; i < MaxMELDevices; i++) {
    if (pomps[i] != "") {
      for (int y = 1; y < 9; y++) {
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

void setup() {
  Serial.begin(115200);
  delay(500);

  //expander.begin(0x20);

  if (LittleFS.begin()) {
    Serial.println("LittleFS Initialize....ok");
  } else {
    Serial.println("LittleFS Initialization...failed");
  }


  //
  //
  //  expander.pinMode(0, OUTPUT);
  //  expander.pinMode(1, OUTPUT);
  //  expander.pinMode(2, OUTPUT);
  //  expander.pinMode(3, OUTPUT);
  //  expander.pinMode(4, OUTPUT);

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

    for (uint8_t i = count; i < 8; i++)
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
    request->send(LittleFS, "/index.html", String(), false, processor);
  });
  server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/style.css", "text/css");
  });
  server.on("/getpomp", HTTP_GET, [](AsyncWebServerRequest * request) {
    for (uint8_t i = 0; i < MaxMELDevices; i++)
      if (pomps[i] == (request->getParam(0))->value())
        request->send_P(200, "text/plain", pompsState[i] == true ? "1" : "0");

  });
  server.on("/autoupdate", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", autoMode == true ? "1" : "0");
  });
  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", (String(ESP.getFreeHeap())).c_str());
  });
  server.on("/autoupdate", HTTP_POST, [](AsyncWebServerRequest * request) {
    autoMode = ((request->getParam(0))->value()) == "1" ? true : false;
    request->send_P(200, "text/plain", "");
  });
  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest * request) {
    if (APMode) request->send_P(200, "text/plain", "");
    else request->send_P(200, "text/plain", cred.devices.c_str());
  });
  server.on("/getpins", HTTP_GET, [](AsyncWebServerRequest * request) {
    String out = "[";
    for (uint8_t i = 0; i < MaxMELDevices; i++) {
      out += (pompsState[i] == true ? "1" : "0");
      if (i < 7) out += ",";
      else out += "]";
    }
    request->send_P(200, "text/plain", out.c_str());
  });
  server.on("/restart", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send_P(200, "text/plain", "");
    ESP.restart();
  });
  server.on("/setpomp", HTTP_GET, [](AsyncWebServerRequest * request) {
    SetPompMode((request->getParam(0))->value(), ((((request->getParam(1))->value())).toInt() == 1 ? true : false));

    request->send_P(200, "text/plain", "");
  });
  server.on("/pomps", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(LittleFS, "/pomps.json", "text/plain");
  });
  server.on("/saveDevices", HTTP_GET, [](AsyncWebServerRequest * request) {
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
    int paramsNr = request->params();
    for (int i = 0; i < paramsNr; i++) {

      AsyncWebParameter* p = request->getParam(i);

      Serial.print("Param name: ");
      Serial.println(p->name());

      Serial.print("Param value: ");
      Serial.println(p->value());

      Serial.println("------");
    }

    cred.SSID = ((request->getParam(0))->value());
    cred.SSIDPass = ((request->getParam(1))->value());

    SaveSettingsToFS();

    request->send_P(200, "text/plain", "");
  });
  server.on("/saveMEL", HTTP_GET, [](AsyncWebServerRequest * request) {
    int paramsNr = request->params();
    for (int i = 0; i < paramsNr; i++) {

      AsyncWebParameter* p = request->getParam(i);

      Serial.print("Param name: ");
      Serial.println(p->name());

      Serial.print("Param value: ");
      Serial.println(p->value());

      Serial.println("------");
    }

    cred.email = ((request->getParam(0))->value());
    cred.pass = ((request->getParam(1))->value());

    SaveSettingsToFS();
    LoginMELCloud();

    request->send_P(200, "text/plain", "");
  });

  server.begin();

  ArduinoOTA.setHostname("waterpump");
  //ArduinoOTA.setPassword((const char *)"Qawsed123");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");  //  "Начало OTA-апдейта"

  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");  //  "Завершение OTA-апдейта"
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    //  "Ошибка при аутентификации"
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    //  "Ошибка при начале OTA-апдейта"
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    //  "Ошибка при подключении"
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    //  "Ошибка при получении данных"
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    //  "Ошибка при завершении OTA-апдейта"
  });
  ArduinoOTA.begin();

  for (uint8_t i = 0; i < 5; i++)
    if (LoginMELCloud()) break;
}


void loop() {
  MDNS.update();
  ArduinoOTA.handle();
  ESP.wdtFeed();

  if ((millis() - lastTime) > timerDelay) {
    if (ESP.getFreeHeap() < 3000) {
      Serial.println("ESP REBOOT HEAP IS SMALL!!!");
      Serial.print("HEAP: ");
      Serial.print(ESP.getFreeHeap());
      Serial.print(" kb");
      ESP.restart();
    }

    if (!APMode) cred.devices = GetDataFromMELCloud();
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


  }
}
