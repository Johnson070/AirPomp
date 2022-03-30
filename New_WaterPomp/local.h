#pragma once

#define MAX_MEL_DEVICES 5

struct Settings_struct {
  const char* SSID = "";
  const char* SSIDPass = "";
  String email;
  String emailPass;
  String token;
  String user;
  String buildId;
  bool autoPowerPomp = true;
  int enabTime = 10000;
  int stbTime = 5000;
};

struct MEL_data {
  String id;
  String name;
  bool state = false;
  String temp;
  String mac;
  int pin;
};

struct dataForm {
  String ssid;
  String ssidPass;
  String email;
  String emailPass;
  int enabTime;
  int stbTime;
};

const char* c_str(String str) {
  unsigned char* buf = new unsigned char[str.length()];
  str.getBytes(buf, str.length()+1, 0);
  return (const char*) buf;
}

void outBTN(String &s, String name, String text, String a, String b) {
  s += "<input type=\"button\" value=\"";
  s += text;
  s += "\" name=\"";
  s += name;
  s += "\"";
  s += " id=\"";
  s += name;
  s += "\" ";
  s += " onclick=\"(function(){ GP_clickid('" + a + "','" + a + "'); setTimeout(GP_clickid('" + b + "','" + b + "'),500); setTimeout(GP_click(document.getElementById('" + name + "')),1000);})();\">\n";
}

Settings_struct cred;
MEL_data cond_data[MAX_MEL_DEVICES];
dataForm data_form;
