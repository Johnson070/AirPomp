#pragma once

#include <LittleFS.h>
#include "local.h"

extern struct Settings_struct cred;
extern struct MEL_data cond_data[MAX_MEL_DEVICES];

bool SaveCondsToFS() {
  File settings = LittleFS.open("conds.json", "w");

  if (settings) {
    for (int i = 0; i < MAX_MEL_DEVICES; i++) {
      settings.print(cond_data[i].mac);
      settings.print('#');
      settings.print(cond_data[i].name);
      settings.print('#');
      settings.print(cond_data[i].pin);
      //settings.print('#');
      settings.print('&');
    }

    settings.close();

    return true;
  }
  else {
    settings.close();
    return false;
  }
}

bool ReadCondsFromFS() {
  File settings = LittleFS.open("conds.json", "r");
  if (settings) {
    bool noWifi = true;

    for (int i = 0; i < MAX_MEL_DEVICES; i++)
      if (cond_data[i].mac != "") {
        noWifi = false;
        break;
      }

    int melIdx = 0;
    String data;
    String s;
    while (settings.available()) {
      s += (char)settings.read();
    }

    int startIdx = 0;
    int endIdx = 0;

    while (s.indexOf('&',startIdx) != -1) {
      endIdx = s.indexOf('&',startIdx);
      String data = s.substring(startIdx, endIdx);

      // обработка

      String mac = data.substring(0,17);
      String name = data.substring(18,data.length()-2);
      int pin = data.substring(data.length()-1,data.length()).toInt();

//      Serial.println(mac + " " + name + " " + String(pin));
//      Serial.println();

      if (noWifi) {
        cond_data[melIdx].mac = mac;
        cond_data[melIdx].name = name;
        cond_data[melIdx].pin = pin;
        cond_data[melIdx].temp = "#";
        melIdx++;
      }
      else {
        bool finded = false;
        for (int i = 0; i < MAX_MEL_DEVICES; i++)
          if (cond_data[i].mac == mac) {
            cond_data[i].pin = pin;
            finded = true;
            break;
          }
        if (!finded)
           for (int i = 0; i < MAX_MEL_DEVICES; i++)
              if (cond_data[i].mac == "") {
                cond_data[i].mac = mac;
                cond_data[i].name = name;
                cond_data[i].pin = pin;
              }
      }
      
      //

      startIdx = endIdx+1;
    }

    settings.close();
    return true;
  }
  else
  {
    settings.close();
  }
  return false;
}

bool SaveSettingsToFS() {
  File settings = LittleFS.open("settings.json", "w");

  if (settings) {
    settings.print(cred.SSID);
    settings.print('#');
    settings.print(cred.SSIDPass);
    settings.print('#');
    settings.print(cred.email);
    settings.print('#');
    settings.print(cred.emailPass);
    settings.print('#');
    settings.print(cred.enabTime);
    settings.print('#');
    settings.print(cred.stbTime);
    settings.print('#');
    settings.close();

    return true;
  }
  else {
    settings.close();
    return false;
  }
}

void ReadSettingFromFS() {
  File settings = LittleFS.open("settings.json", "r");
  if (settings) {
    String data;
    int idxCred = 0;
    while (settings.available()) {
      char charr = (char)settings.read();
      if (charr != '#') data += charr; //&& charr != 0x0D
      else {
        //Serial.println(data);
        switch (idxCred) {
          case 0:
            cred.SSID = c_str(data);
            break;
          case 1:
            cred.SSIDPass = c_str(data);
            break;
          case 2:
            cred.email = data;
            break;
          case 3:
            cred.emailPass = data;
            break;
          case 4:
            cred.enabTime = data.toInt();
            break;
          case 5:
            cred.stbTime = data.toInt();
            break;
        }
        idxCred++;
        data = "";
      }
    }
    settings.close();
  }
  else
  {
    settings.close();
  }

}
