/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/get-change-esp32-esp8266-mac-address-arduino/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.  
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
#include <WiFi.h>
#include <esp_wifi.h>

uint8_t baseMac[6];
bool gotMac = false;

void dispMacAddress() {
  Serial.printf("%02x:%02x:%02x:%02x:%02x:%02x\n",
      baseMac[0], baseMac[1], baseMac[2],
      baseMac[3], baseMac[4], baseMac[5]);
}

void readMacAddress(){
  // uint8_t baseMac[6];
  esp_err_t ret = esp_wifi_get_mac(WIFI_IF_STA, baseMac);
  if (ret == ESP_OK) {
    gotMac = true;
    dispMacAddress();
  } else {
    Serial.println("Failed to read MAC address");
  }
}

void setup(){
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.STA.begin();

  Serial.print("[DEFAULT] ESP32 Board MAC Address: ");
  readMacAddress();
  Serial.printf("gotMac = %d\n", gotMac);
}
 
void loop(){
  digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  delay(1000);                      // wait for a second
  if (gotMac)
    dispMacAddress();
  digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  delay(1000);   
}