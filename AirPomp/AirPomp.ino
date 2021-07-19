/*
  Rui Santos
  Complete project details at Complete project details at https://RandomNerdTutorials.com/esp8266-nodemcu-http-get-post-arduino/

  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

  Code compatible with ESP8266 Boards Version 3.0.0 or above
  (see in Tools > Boards > Boards Manager > ESP8266)
*/

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <FS.h>

const char* ssid = "WHome";
const char* password = "adrenalin";

//Your Domain name with URL path or IP address with path
String serverName = "https://app.melcloud.com/Mitsubishi.Wifi.Client";

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
// Timer set to 10 minutes (600000)
//unsigned long timerDelay = 600000;
// Set timer to 5 seconds (5000)
unsigned long timerDelay = 5000;

String token;

struct {
  char email[50];
  char password[50];
  char user[20];
  char token[30];
  char ssid[30];
  char pass_wifi[30];
} data;

void setup() {
  Serial.begin(115200);

  if (SPIFFS.begin())
  {
    Serial.println("SPIFFS Initialize....ok");
  }
  else
  {
    Serial.println("SPIFFS Initialization...failed");
  }


  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());

  if (!MDNS.begin("airpump")) {             // Start the mDNS responder for esp8266.local
    Serial.println("Error setting up MDNS responder!");
  }
  Serial.println("mDNS responder started");

  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.connect((serverName + "/Login/ClientLogin").c_str(), 433);
    HTTPClient http;

    http.begin(client, (serverName + "/Login/ClientLogin").c_str());

    // If you need an HTTP request with a content type: application/json, use the following:
    http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:73.0) Gecko/20100101 Firefox/73.0");
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(
                             "{\"Email\":\"anton.shokin@gmail.com\", \"Password\":\"Qawsed123\",\"Language\":0,\"AppVersion\":\"1.19.1.1\",\"Persist\":true, \"CaptchaResponse\":null}"
                           );

    if (httpResponseCode > 0) {
      //Get the request response payload
      String payload = http.getString();
      //Print the response payload
      Serial.println(payload);
      DynamicJsonDocument jsonBuffer(2048);
      DeserializationError error = deserializeJson(jsonBuffer, payload.c_str());

      if (error) {
        Serial.println(error.c_str());
        return;
      }
      Serial.println(jsonBuffer["ErrorId"].as<int>());
      Serial.println(jsonBuffer["LoginData"]["ContextKey"].as<String>());
      Serial.println(jsonBuffer["LoginData"]["Name"].as<String>());
      token = jsonBuffer["LoginData"]["ContextKey"].as<String>();
    }

    // If you need an HTTP request with a content type: text/plain
    //http.addHeader("Content-Type", "text/plain");
    //int httpResponseCode = http.POST("Hello, World!");

    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);

    // Free resources
    http.end();
  }
}

void loop() {
  //Send an HTTP POST request every 10 minutes
  if ((millis() - lastTime) > timerDelay) {
    //Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED) {
      BearSSL::WiFiClientSecure client;
      client.setInsecure();
      client.connect((serverName + "/User/ListDevices").c_str(), 433);
      HTTPClient http;

      // Your Domain name with URL path or IP address with path
      http.begin(client, (serverName + "/User/ListDevices").c_str());

      // Specify content-type header
      //http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      // Data to send with HTTP POST
      //String httpRequestData = "api_key=tPmAT5Ab3j7F9&sensor=BME280&value1=24.25&value2=49.54&value3=1005.14";
      // Send HTTP POST request
      //int httpResponseCode = http.POST(httpRequestData);

      // If you need an HTTP request with a content type: application/json, use the following:
      http.addHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:73.0) Gecko/20100101 Firefox/73.0");
      http.addHeader("Content-Type", "application/json");
      http.addHeader("Accept", "application/json, text/javascript, */*; q=0.01");
      http.addHeader("Accept-Language", "en-US,en;q=0.5");
      http.addHeader("Accept-Encoding", "gzip, deflate, br");
      http.addHeader("X-MitsContextKey", token);
      http.addHeader("X-Requested-With", "XMLHttpRequest");
      http.addHeader("Cookie", "policyaccepted=true");
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        int countDevice;
        int len = http.getSize();

        // create buffer for read
        uint8_t buff[128] = { 0 };

        // get tcp stream
        WiFiClient * stream = http.getStreamPtr();

        // read all data from server
        String SearchWord;
        bool findDevice;
        bool findPower;
        String DeviceWord = "Device";
        DeviceWord += '"';
        DeviceWord += ":{";
        Serial.println(DeviceWord);

        while (http.connected() && (len > 0 || len == -1)) {
          // get available data size
          size_t size = stream->available();

          if (size) {
            // read up to 128 byte
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));

            Serial.write(buff, c);
//            // write it to Serial
//            if (strstr((const char*)buff, DeviceWord.c_str()) && !findDevice) {
//              countDevice++;
//              findDevice = true;
//            }
//            else if (findDevice && strstr((const char*)buff, ("Power").c_str())){
//              findPower = true;
//            }
//            else if (findDevice && findPower && strstr((const char*)buff, ("true").c_str()) || strstr((const char*)buff, ("false").c_str()))

            if (len > 0) {
              len -= c;
            }
          }
          delay(1);

        }
        Serial.println();
        if (countDevice > 0)
          Serial.println("Finded Device: " + String(countDevice));
      }



      // If you need an HTTP request with a content type: text/plain
      //http.addHeader("Content-Type", "text/plain");
      //int httpResponseCode = http.POST("Hello, World!");

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // Free resources
      http.end();
    }
    else {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}
