#pragma once

extern struct dataForm data_form;

void myClick(GyverPortal* p) {
  // имеем доступ к объекту портала, который отправил вызов
  for (int i = 0; i < MAX_MEL_DEVICES; i++) {
    const char* switchName = c_str("sw"+String(i));
    if (p -> click(switchName)) {
      if (!cred.autoPowerPomp && cond_data[i].name != "")
        cond_data[i].state = p -> getCheck(switchName);
    }
  }

  for (int i = 0; i < MAX_MEL_DEVICES; i++) {
    const char* selectName = c_str("sel"+String(i));
    if (p -> click(selectName)) {
      //Serial.println(p -> getCheck(selectName));
      if (cond_data[i].name != "")
        cond_data[i].pin = p -> getSelected(selectName, "1,2,3,4,5");
    }
  }

  if (p->click("ssid")) data_form.ssid = p->getString("ssid");
  if (p->click("ssidPass")) data_form.ssidPass = p->getString("ssidPass");
  if (p->click("email")) data_form.email = p->getString("email");
  if (p->click("emailPass")) data_form.emailPass = p->getString("emailPass");
  if (p->click("enabTime")) data_form.enabTime = p->getInt("enabTime");
  if (p->click("stbTime")) data_form.stbTime = p->getInt("stbTime");

  if (p->click("saveWifiBtn")){
    cred.SSID = c_str(data_form.ssid);
    cred.SSIDPass = c_str(data_form.ssidPass);
    SaveSettingsToFS();

    //wait ans 
  }
  if (p->click("saveMelBtn")) {
    cred.email = data_form.email;
    cred.emailPass = data_form.emailPass;
    SaveSettingsToFS();
  }
  if (p->click("saveTimeBtn")) {
    cred.enabTime = data_form.enabTime;
    cred.stbTime = data_form.stbTime;
    SaveSettingsToFS();
  }

  if (p->click("swAuto")) cred.autoPowerPomp = p->getCheck("swAuto");

  if (p -> click("rebootBtn")) ESP.restart();

  if (p -> click("btnSavePins")) {
    SaveCondsToFS();
  }
}

void myUpdate(GyverPortal* p) {
  if (p -> update("uptime")) p -> answer(String(millis() / 1000 / 60 / 60 / 24) + " d " + String(millis() / 1000 / 60 / 60 % 24) + " h " + String(millis() / 1000 / 60 % 60) + " m " + String(millis() / 1000 % 60) + " s");
  if (p -> update("heap")) p -> answer(String(ESP.getFreeHeap() / 1024) + " kB");

  for (int i = 0; i < MAX_MEL_DEVICES; i++) {
    if (p -> update(("led" + String(i)).c_str()))
    {
      p -> answer(cond_data[i].state);
    }
    if (p -> update(("sw" + String(i)).c_str()))
    {
      p -> answer(cond_data[i].state);
    }
  }
}

void myForm(GyverPortal* p) {
//  // проверяем, была ли это форма "/update"
//  if (p -> form("/wifi")) {
//    Serial.println("WIFI SAVED!");
//    cred.SSID = c_str(data_form.ssid);
//    cred.SSIDPass = c_str(data_form.ssidPass);
//    SaveSettingsToFS(&cred);
//  }
//  if (p -> form("/mel")) {
//    Serial.println("MEL SAVED!");
//    cred.email = data_form.email;
//    cred.emailPass = data_form.emailPass;
//    SaveSettingsToFS(&cred);
//  }
}

void build() {
  String s;
  BUILD_BEGIN(s);
  add.THEME(GP_DARK);

  String ajax_update = "";
  for (byte i = 0; i < MAX_MEL_DEVICES; i++)
    if (cond_data[i].name != "")
      ajax_update += ("led" + String(i) + ",sw" + String(i) + ",");
  ajax_update[ajax_update.length()-1] = '\0';

  add.AJAX_UPDATE(ajax_update.c_str(), 1000);
  add.AJAX_UPDATE(PSTR("uptime,heap"), 1000);

  add.TITLE("ESP-01 Mel Cloud");
  add.LABEL("Hello,");
  add.LABEL(cred.user.c_str());
  add.LABEL("!"); add.BREAK();

  add.BLOCK_BEGIN();
    add.LABEL("Wi-Fi Settings"); add.HR();
    add.LABEL("SSID: ");
    add.TEXT("ssid", "SSID", cred.SSID); add.BREAK();
    add.LABEL("Password: ");
    add.TEXT("ssidPass", "pass", cred.SSIDPass); add.BREAK();
    outBTN(s,"saveWifiBtn","Save","ssid","ssidPass");
    //add.BUTTON("saveWifiBtn","Save");
  add.BLOCK_END();
    
  add.BLOCK_BEGIN();
    add.LABEL("MEL Cloud Settings"); add.HR();
    add.LABEL("Email: ");
    add.TEXT("email", "email", cred.email); add.BREAK();
    add.LABEL("Password: ");
    add.TEXT("emailPass", "pass", cred.emailPass); add.BREAK();
    outBTN(s,"saveMelBtn","Save","email","emailPass");
    //add.BUTTON("saveMelBtn","Save");
  add.BLOCK_END();


  add.BLOCK_BEGIN();
    add.LABEL("Devices"); add.HR();
    for (byte i = 0; i < MAX_MEL_DEVICES; i++) {
      if (cond_data[i].mac != "") {
        add.LABEL((cond_data[i].name + " |").c_str());
        //add.LABEL((cond_data[i].mac).c_str());
        add.LABEL((cond_data[i].temp + 'C').c_str());
        add.SELECT(("sel" + String(i)).c_str(), "1,2,3,4,5", cond_data[i].pin);
        add.LED_GREEN(("led" + String(i)).c_str(), cond_data[i].state);
        add.SWITCH(("sw" + String(i)).c_str(), cond_data[i].state); add.BREAK();
      }
    }
    add.LABEL("Auto update power pomp");
    add.SWITCH("swAuto",cred.autoPowerPomp); add.BREAK();
    add.BUTTON("btnSavePins", "Save");
  add.BLOCK_END();

  add.BLOCK_BEGIN();
    add.LABEL("Timer settings"); add.HR();
    add.LABEL("Enable time:");
    add.NUMBER("enabTime", "period(ms)", cred.enabTime); add.BREAK();
    add.LABEL("STB time:");
    add.NUMBER("stbTime", "period(ms)", cred.stbTime); add.BREAK();
    outBTN(s,"saveTimeBtn","Save","enabTime","stbTime");
    //add.BUTTON("saveMelBtn","Save");
  add.BLOCK_END();


  add.BLOCK_BEGIN();
  add.LABEL("Uptime:");
  add.LABEL("NAN", "uptime"); add.BREAK();
  add.LABEL("Token:");
  add.LABEL(cred.token.c_str()); add.BREAK();
  add.LABEL("Free heap:");
  add.LABEL("NAN", "heap"); add.BREAK();
  add.BUTTON("rebootBtn", "Reboot");
  add.BLOCK_END();
  add.FORM_END();

  BUILD_END();
}

//
