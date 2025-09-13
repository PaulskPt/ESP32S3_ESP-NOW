/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp-now-two-way-communication-esp32/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
/*
  Changes by Paulus Schulinck (@PaulskPt Github)
  Arduino sketch: LolinS3_ESP-NOW_master.ino

   Hardware connected to the Lolin S3 device:
   a) an Adafruit AHT20 temperature and humidity sensor.
  
   PURPOSE: 
   Test ESP-NOW communication between two microcontroller boards, each with an Esprssif ESP32-S3 processor.
   This board is set to function as the "Master" device.
   The other board is set to function as the "Slave" device.

   SHORT DESCRIPTION:
   At startup this device will wait to receive a datetime stamp from the slave device.
   After reception of the datetime stamp, this sketch will set the internal RTC with the received datetime stamp.
   Next, with an interval of one hour, the slave device will send an updated datetimestamp (this time from it's external RTC).
   Every time this sketch will then update the internal RTC of this device.
   This master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to the slave device.
   At the moment of a successful transmission, the builtin RGB led will blink GREEN. If transmission fails, the led will blink RED.

   Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).
*/
#include <esp_now.h>
#include <WiFi.h>

#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_AHTX0.h>
#include <ESP32Time.h>

/*
   The ESP-NOW API changed in ESP-IDF v5.0+, and the Arduino core for ESP32 has now adopted that newer version.
   Espressif refactored ESP-NOW to unify it with their Wi-Fi stack. 
   Instead of passing just the MAC address, the new wifi_tx_info_t struct includes richer metadata 
   about the transmission — including destination MAC, rate, and status flags.
   For this the #define USE_UPDATE below. You can view the difference between the two versions of OnDataSent and OnDataRecv.
*/
#ifndef USE_UPDATE
#define USE_UPDATE
#endif

#ifdef USE_RTC
#undef USE_RTC
#endif

#ifdef USE_RTC
#include "DS3231.h"
#endif

#ifndef USE_AHT
#define USE_AHT (1)
#endif

#define RGB_LED 38
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, RGB_LED, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

// THE MAC Address of your receiver 
// You need to enter the MAC address of the other board (the board you’re sending data to).
uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x6D, 0xC5, 0xF0}; // Lolin S3 PRO (ESP-NOW slave)
//uint8_t broadcastAddress[] = {0xEC, 0xDA, 0x3B,0x55,0x38,0x40};  // Lolin S3

#ifdef USE_RTC
DS3231 RTC;  // Create the RTC object
bool Century=false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;

byte year, month, date, DoW, hour, minute, second;
#endif

//ESP32Time rtc;
ESP32Time rtc(0);  // offset in seconds GMT+0  (if +1 set rtc(3600))
bool internalRTCset = false;

unsigned long start_t = millis();

#ifdef USE_AHT
  Adafruit_AHTX0 aht;
  float temperature;
  float humidity;
  Adafruit_Sensor *aht_humidity, *aht_temperature;
  sensors_event_t aht20_humidity;
  sensors_event_t aht20_temperature;
#endif

// Define variables to store incoming readings
//float incomingTemp;
//float incomingHum;
char rcvdNTPdatetime[21];
String NTPdatetime = "";
int incomingDataLen = 0;
bool new_data_rcvd = false;

// Variable to store if sending data was successful
String success;

uint8_t TX_PacketNr = 0;

//Structure example to send data
//Must match the receiver structure
#ifdef USE_AHT
    typedef struct struct_message {
      float temp;
      float hum;
      uint8_t packetNr;
  } struct_message;

  struct_message AHT20Readings;
#endif

typedef struct datetime_message {
  char datetime[40];
} datetime_message;

datetime_message NTPmessage;

// Create a struct_message to hold incoming sensor readings
// struct_message incomingReadings;
datetime_message incomingDatetime;

esp_now_peer_info_t peerInfo;

void neoPixShow(int r=0, int g=0, int b=0) {
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
    pixels.show();
    delay(DELAYVAL);
  }
}

#ifdef USE_UPDATE
/* See: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html */
void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (info) {
    Serial.print("Destination MAC: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", info->des_addr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
  }
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Failed");
  if (status ==0){
    success = "Delivery Success :)";
    neoPixShow(0,60,0); // GREEN
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(500);                         // wait for a half second
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    neoPixShow(0,0,0);
  }
  else{
    success = "Delivery Failed :(";
    neoPixShow(60,0,0); // RED
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(500);                         // wait for a half second
    neoPixShow(0,0,0);
    //digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  }
}

#else
// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  //Serial.print(F("\r\nLast Packet Send Status:\t"));
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Failed");
  if (status == 0){
    success = "Delivery Success :)";
    neoPixShow(0,60,0); // GREEN
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(500);                      // wait for a half second
    neoPixShow(0,0,0);
    //digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    // delay(1000);  
  }
  else{
    success = "Delivery Failed :(";
    neoPixShow(60,0,0); // RED
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    delay(500);                      // wait for a half second
    neoPixShow(0,0,0);
  }
}
#endif

#ifdef USE_UPDATE
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {

  if (recv_info) {
    memcpy(&rcvdNTPdatetime, incomingData, len); // was: , sizeof(rcvdNTPdatetime));
    incomingDataLen = len;
    if (incomingDataLen > 0)
      new_data_rcvd = true;
    }
  }
}
#else
// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&rcvdNTPdatetime, incomingData, len); // was: , sizeof(rcvdNTPdatetime));
  incomingDataLen = len;
  if (incomingDataLen > 0)
    new_data_rcvd = true;
}
#endif

void setInternalRTC() {
  static constexpr const char txt0[] PROGMEM = "setInternalRTC(): ";
  //          ss  mm  hh  dd mo  yy
  rtc.setTime(30, 24, 15, 17, 1, 2021);  // 17th Jan 2021 15:24:30
  // Example received datetime:  2025-03-13T12:28:59Z
  uint8_t NTPdtLe = NTPdatetime.length();
  if (NTPdtLe > 0) {
    int splitT = NTPdatetime.indexOf("T");
  
    String dateStamp      = NTPdatetime.substring(0, splitT);
    String dayStamp       = NTPdatetime.substring(splitT-2, splitT);
    String monthStamp     = NTPdatetime.substring(splitT-5, splitT-3);
    String yearStamp      = NTPdatetime.substring(0, 4);
    String yearStampSmall = NTPdatetime.substring(2, 4);
    String timeStamp      = NTPdatetime.substring(splitT+1, NTPdatetime.length()-1);
    String hourStamp      = timeStamp.substring(0,2);
    String minuteStamp    = timeStamp.substring(3,5);
    String secondStamp    = timeStamp.substring(6,8);

    int year   = yearStamp.toInt();
    int month  = monthStamp.toInt();
    int day    = dayStamp.toInt();
    int hour   = hourStamp.toInt();
    int minute = minuteStamp.toInt();
    int second = secondStamp.toInt();
    Serial.print(txt0);
    Serial.printf("Setting internal rtc to (ss: %d, mm; %d, hh: %d, dd: %d, mo: %d, yy: %d)\n", 
        second, minute, hour, day, month, year);
    rtc.setTime(second, minute, hour, day, month, year);
    Serial.print(txt0);
    Serial.print(F("Internal RTC datetime now: "));
    Serial.println(rtc.getDateTime(true));
    internalRTCset = true;
  }
}

#ifdef USE_RTC
void ReadDS3231()
{
  int second,minute,hour,date,month,year,temperature; 
  second=RTC.getSecond();
  minute=RTC.getMinute();
  hour=RTC.getHour(h12, PM);
  date=RTC.getDate();
  month=RTC.getMonth(Century);
  year=RTC.getYear();
  
  temperature=RTC.getTemperature();
  
  Serial.print("20");
  Serial.print(year,DEC);
  Serial.print('-');
  Serial.print(month,DEC);
  Serial.print('-');
  Serial.print(date,DEC);
  Serial.print(' ');
  Serial.print(hour,DEC);
  Serial.print(':');
  Serial.print(minute,DEC);
  Serial.print(':');
  Serial.print(second,DEC);
  Serial.print('\n');
  Serial.print("Temperature=");
  Serial.print(temperature); 
  Serial.print('\n');
}
#endif

bool IsWiFiConnected(void) {
  return (WiFi.status() == WL_CONNECTED);
}

bool setupESPNOW(void) {
  static constexpr const char txt0[] PROGMEM = "setupESPNOW(): ";
  bool retval = false;

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.print(txt0);
    Serial.println(F("Error initializing ESP-NOW"));
    return retval;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  esp_now_register_send_cb(OnDataSent);

  // Register peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.print(txt0);
    Serial.println(F("Failed to add peer"));
    return retval;
  }
  // Register for a callback function that will be called when data is received
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));

  Serial.print(txt0);
  Serial.println(F("successfull"));
  retval = true;

  return retval;
}

void getReadings(){
  #ifdef USE_AHT
    /* Get a new normalized sensor event */
    //sensors_event_t humidity;
    //sensors_event_t temp;
    aht.getEvent(&aht20_humidity, &aht20_temperature);
    
    // populate temp and humidity objects with fresh data
    //aht_humidity->getEvent(&aht20_humidity);
    //aht_temperature->getEvent(&aht20_temperature);

    // Copy to global variables
    temperature = aht20_temperature.temperature;
    humidity = aht20_humidity.relative_humidity;
  
    Serial.print("Temperature: "); 
    Serial.print(aht20_temperature.temperature); 
    Serial.println(" ºC");
    Serial.print("Humidity: "); 
    Serial.print(aht20_humidity.relative_humidity); 
    Serial.println(" % rH");
  #endif
}

void updateDisplay(){
  // Display Readings on OLED Display
  #ifdef USE_OLED
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0, 0);
    display.println("INCOMING READINGS");
    if (incomingTemp != 0.00) {
    display.setCursor(0, 15);
      display.print("Temperature: ");
      display.print(incomingTemp);
      display.cp437(true);
      display.write(248);
      display.print("C");
    }
    else {
      display.print(F("not available"));
    }
    if (incomingHum != 0.00) {
      display.setCursor(0, 25);
      display.print("Humidity: ");
      display.print(incomingHum);
      display.print("%");
    }
    else {
      display.print(F("not available"));
    }
    #ifdef USE_BME
      display.setCursor(0, 35);
      display.print("Pressure: ");
      display.print(incomingPres);
      display.print("hPa");
    #endif
    display.setCursor(0, 56);
    display.print(success);
    display.display();
  #endif
  
  // Display Readings in Serial Monitor
  /*
  Serial.println("INCOMING READINGS");
  Serial.print("Temperature: ");
  if (incomingReadings.temp != 0.00) {
    Serial.print(incomingReadings.temp);
    Serial.println(" ºC");
  }
  else {
    Serial.println(F("not available"));
  }
  Serial.print("Humidity: ");
  if (incomingReadings.hum != 0.00) {
    Serial.print(incomingReadings.hum);
    Serial.println(" %");
  }
  else {
    Serial.println(F("not available"));
  }
  Serial.println();
  */
}

void handleIncomingData() {
  static constexpr const char txt0[] PROGMEM = "handleIncomingData(): ";
  // Clear NTPdatetime:
  NTPdatetime = "";
  if (incomingDataLen > 0) {
    for (uint8_t i = 0; i < incomingDataLen; i++) {
      if (rcvdNTPdatetime[i] == 0)
        break;
      NTPdatetime += rcvdNTPdatetime[i];
    }
    //NTPdatetime += '\0';
  }

  uint8_t NTPdtLe = NTPdatetime.length();
  if (NTPdtLe > 0) {
    Serial.print(txt0);
    Serial.print(F("\nNTP datetime received from slave: "));
    Serial.println(NTPdatetime);
    setInternalRTC();
    if (internalRTCset) {
      NTPdatetime[0] = '\0'; // Cleanup NTPdatetime for a next round
    }
  } else {
    Serial.print(txt0);
    Serial.print(F("length NTP datetime received from slave: "));
    Serial.printf("%d\n", NTPdtLe);
  }
  new_data_rcvd = false; // reset flag
}

void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

#ifdef USE_RTC
  Wire.begin();
  RTC.setSecond(0);//Set the second 
  RTC.setMinute(30);//Set the minute 
  RTC.setHour(12);  //Set the hour 
  RTC.setDoW(2);    //Set the day of the week
  RTC.setDate(12);  //Set the date of the month
  RTC.setMonth(3);  //Set the month of the year
  RTC.setYear(25);  //Set the year (Last two digits of the year)
#endif

#ifdef USE_AHT
    // Init AHT20 sensor
    if (! aht.begin()) {
      Serial.println("Could not find AHT? Check wiring");
      while (1) delay(10);
    }
    Serial.println("AHT20 found");
#endif
 
  int try_cnt = 0;

  while (!setupESPNOW()) {
    delay(1000);
    try_cnt++;
    if (try_cnt >= 5)
      Serial.println(F("setup(): unable to setup ESP-NOW.\nRestarting in 5 seconds..."));
      delay(5000);
      esp_restart();
  }
}
 
void loop() {
  static constexpr const char txt0[] PROGMEM = "loop(): ";
  unsigned long current_t = 0;
  unsigned long interval_t = 10000; // 10 second interval.
  int cnt = 0;
  Serial.print(txt0);
  Serial.println(F("waiting for NTP datetime from slave"));
  while (incomingDataLen == 0) {  // Wait for NTPdatetime from slave
    //Serial.print(F(".."));
    cnt++;
    if (cnt >= 100)
      break;
    delay(100);
  }
  Serial.print(txt0);
  Serial.print(F("Bytes received from slave: "));
  Serial.println(incomingDataLen);

  if (new_data_rcvd) {
    handleIncomingData();
  }
  while (true) {
#ifdef USE_RTC
    ReadDS3231();
#endif
    if (new_data_rcvd) {
      handleIncomingData();
    }
    current_t = millis();
    if (current_t - start_t >= interval_t) {
        start_t = current_t;
      getReadings();
      // Set values to send

#ifdef USE_AHT
      AHT20Readings.temp = temperature;
      AHT20Readings.hum = humidity;
      AHT20Readings.packetNr = TX_PacketNr;
      // Send message via ESP-NOW

      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &AHT20Readings, sizeof(AHT20Readings));
#endif
      if (internalRTCset) {
        Serial.print(rtc.getDateTime());
        Serial.print(". ");
      }
      Serial.print(F("Packet nr: "));
      Serial.printf("%d ", TX_PacketNr);
      //Serial.print(F("send result: "));
      Serial.println(success);
      if (result == ESP_OK) {
        //Serial.println(F("successful"));
        TX_PacketNr++;
        if (TX_PacketNr > 255)
          TX_PacketNr = 0;
      }
      else {
        //Serial.println(F("failed"));
        ;
      }
      //updateDisplay();
      delay(10000); // Wait 10 seconds
    }
  }
}
