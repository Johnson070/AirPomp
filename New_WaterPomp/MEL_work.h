#pragma once

#include "local.h"

extern struct MEL_data cond_data[MAX_MEL_DEVICES];
extern struct Settings_struct cred;

const String MELCloud = "https://app.melcloud.com/Mitsubishi.Wifi.Client";

bool UpdateCond(String id){
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClientSecure client;
    client.setInsecure();
    client.connect(MELCloud + "/Device/RequestRefresh?id=" + id, 433);
    HTTPClient http;

    http.begin(client, MELCloud + "/Device/RequestRefresh?id=" + id);
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
      //Get the request response payload
      String payload = http.getString();
      //Print the response payload
      //Serial.println(payload);
      http.end();
      
      return payload == "true" ? true : false;
    }
    else {
      Serial.println("Error connect to MELCloud!");
      Serial.print(httpResponseCode);
      http.end();
      return false;
    }
  }
  else {
    Serial.println("WiFi Disconnected");
    //ConnectWIFI();
    return false;
  }
}

void SplitString(char delim, String str){
  int elements = 1;
  for (int i = 0; i < str.length(); i++)
    if (str[i] == delim) elements++;

  String outStr[elements];

  int idxStart = 0;
  int idxStr = 0;
  for (int i = 0; i < str.length(); i++)
    if (str[i] == delim) {
       outStr[idxStr++] = str.substring(idxStart, i);
       idxStart = i+1; 
    }
  outStr[idxStr++] = str.substring(idxStart, str.length()-1);

  for (int i = 0; i < elements; i++){
    if (i % 6 == 0) cond_data[i/6].id = outStr[i];
    else if (i % 6 == 1) cond_data[i/6].name = outStr[i];
    else if (i % 6 == 2) cred.buildId = outStr[i];
    else if (i % 6 == 3) cond_data[i/6].state = outStr[i] == "true" ? true : false;
    else if (i % 6 == 4) cond_data[i/6].temp = outStr[i];
    else if (i % 6 == 5) cond_data[i/6].mac = outStr[i];
  }
}

bool ExtractBigDataMEL(String json, bool showCont = false)
{
  SplitString('$',json);

  if (showCont)
    for (int i = 0; i < MAX_MEL_DEVICES; i++)
    {
      if (cond_data[i].id != "") {
        Serial.println(cond_data[i].id);
        Serial.println(cond_data[i].name);
        Serial.println(cond_data[i].state);
        Serial.println(cond_data[i].temp);
        Serial.println(cond_data[i].mac);
        Serial.println();
      }
    }
  
  return true;
}

bool GetDataFromMELCloud() {
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
      uint8_t buff[512] = { 0 };

      // get tcp stream
      WiFiClient* stream = http.getStreamPtr();

      // read all data from server
      String buffBlock = "";
      int lenBlock = 512;
      uint8_t maxLenWord = 40;
      uint8_t countWords = 8;
      String searchWords[countWords];
      int srcIdx = 0;
      searchWords[srcIdx] = "Devices";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ":[{";

      searchWords[srcIdx] = "DeviceID";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';
      
      searchWords[srcIdx] = "DeviceName";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx] += ':';
      searchWords[srcIdx++] += '"';
      
      searchWords[srcIdx] = "BuildingID";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';
      
      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "Device";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ":{";
      
      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "Power";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';
      
      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "RoomTemperature";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';
      
      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "MacAddress";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx] += ':';
      searchWords[srcIdx++] += '"';

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

              if (i == 1) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 2) out += k.substring(idx + searchWords[i].length(), idx + lenData - 1) + "$";
              else if (i == 3) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 5) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 6) out += k.substring(idx + searchWords[i].length(), idx + lenData) + "$";
              else if (i == 7) out += k.substring(idx + searchWords[i].length(), idx + lenData - 1) + "$";
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
      
      //Serial.println(out);
      out[out.length()-1] = '\0';
      ExtractBigDataMEL(out, false);

      return true;
    }
    else return false;
  }
  else {
    Serial.println("WiFi Disconnected");
    //ConnectWIFI();
  }
  

  return false;
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
                             "{\"Email\":\"" + cred.email + "\", \"Password\":\"" + cred.emailPass + "\",\"Language\":16,\"AppVersion\":\"1.19.1.1\",\"Persist\":true, \"CaptchaResponse\":null}");

    if (httpResponseCode == 200) {
      //Get the request response payload
      String payload = http.getString();
      //Print the response payload
      //Serial.println(payload);
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
      
//        Serial.println(cred.token);
//        Serial.println(cred.user);
        
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

bool UpdateCondData(String id) {
    if (WiFi.status() == WL_CONNECTED && cred.buildId != "") {
    BearSSL::WiFiClientSecure client;
    client.setInsecure();
    client.connect(MELCloud + "/Device/Get?id="+id+"&buildingID=" + cred.buildId, 433);
    HTTPClient http;

    http.begin(client, MELCloud + "/Device/Get?id="+id+"&buildingID=" + cred.buildId);

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
      uint8_t buff[512] = { 0 };

      // get tcp stream
      WiFiClient* stream = http.getStreamPtr();

      int condIdx;

      for (int i = 0; i < MAX_MEL_DEVICES; i++)
        if (cond_data[i].id == id)
        {
          condIdx = i;
          break;
        } 

      // read all data from server
      String buffBlock = "";
      int lenBlock = 512;
      uint8_t maxLenWord = 40;
      uint8_t countWords = 8;
      String searchWords[countWords];
      int srcIdx = 0;
      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "RoomTemperature";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';

      searchWords[srcIdx] = '"';
      searchWords[srcIdx] += "Power";
      searchWords[srcIdx] += '"';
      searchWords[srcIdx++] += ':';

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

              if (i == 0) cond_data[condIdx].temp = k.substring(idx + searchWords[i].length(), idx + lenData);
              else if (i == 1) cond_data[condIdx].state = k.substring(idx + searchWords[i].length(), idx + lenData) == "true" ? true : false;
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

      return true;
    }
    else return false;
  }
  else {
    Serial.println("WiFi Disconnected");
    //ConnectWIFI();
  }
  

  return false;
}
