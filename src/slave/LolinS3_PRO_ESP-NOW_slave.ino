/*
  Rui Santos & Sara Santos - Random Nerd Tutorials
  Complete project details at https://RandomNerdTutorials.com/esp-now-two-way-communication-esp32/
  Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files.
  The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*/
/*
   Changes by Paulus Schulinck (@PaulskPt Github)
   Arduino sketch: LolinS3_ESP-NOW_slave.ino

   Hardware connected to the Lolin S3 PRO device:
   a) a small DS3231 external RTC board marked "DS3231 for Pi";
   b) a Lolin 2.13inch e-Paper 3-color display, 250x122px. Needs SSD1680 driver module.

   PURPOSE: 
   Test ESP-NOW communication between two microcontroller boards, each with an Esprssif ESP32-S3 processor.
   This board is set to function as the "Slave" device.
   The other board is set to function as the "Master" device.

   SHORT DESCRIPTION:
   At startup this device will connect to the internet via WiFi.
   When WiFi is established, this sketch will get a datetime stamp from a NTP-server.
   After reception of the NTP datetime stamp, this sketch will set the external RTC with the received datetimestamp.
   Immediately after this sketch will send the datetime stamp to the master device.
   Next, with an interval of one hour the sending of a updated datetimestamp (this time from the external RTC) will be sent to the master device.
   The master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to this slave device.
   At the moment of a successful reception, the builtin RGB led will blink GREEN.
   After reception this sketch will display the received data on the connected e-Paper display.
   Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).

   WiFi connect parts copied from: <drive>:\Dropbox\<user>\Hardware\M5Stack\M5Stack_Dial\Arduino\M5Dial_Timezones\2024-10-05_23h28_backup_(working_OK)
*/
#include <esp_now.h>
#include <WiFi.h>
#include <WiFiType.h>
#include "DS3231.h"
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#include <LOLIN_EPD.h>
#include <Adafruit_GFX.h> // Core graphics library
#include "NTPClient.h" // Fork by Taranais. See: https://github.com/taranais/NTPClient
#include <WiFiUdp.h>
#include "secret.h"
// #include <Adafruit_AHTX0.h>
#define NTP_OFFSET  0 // In seconds for Europe/Lisbon time zone
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  SECRET_NTP_SERVER_1

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

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

enum myWiFiStatus {
  notSet,
  usingWiFi,
  usingSTA
};

// See: https://stackoverflow.com/questions/55059105/different-wifi-modes-in-arduino-for-esp32
wifi_mode_t currWiFiMode;

myWiFiStatus currWiFiStatus = notSet;

String formattedDate;
String dayStamp;
String timeStamp;
char rcvdNTPdatetime[40];

#define SDA   9
#define SCL  10
#define MOSI 11
#define SCK  12
#define MISO 13
// #define EPD_LED 14
#define EPD_BUSY 14 // can set to -1 to not use a pin (will wait a fixed delay)
#define EPD_RST 21  // can set to -1 and share with microcontroller Reset!
#define TS_CS 45  // Touch
#define TF_CS 46  // SD-card
#define EPD_DC 47
#define EPD_CS 48
#define RGB_LED 38
//#define EPD_SPI &SPI // primary SPI

#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, RGB_LED, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

uint8_t EPD_Width = 250;
uint8_t EPD_Height = 122;
LOLIN_SSD1680 EPD(EPD_Width, EPD_Height, EPD_DC, EPD_RST, EPD_CS, EPD_BUSY); //hardware SPI

bool my_debug = true;

unsigned long start_t = millis();

// THE MAC Address of your receiver 
// You need to enter the MAC address of the other board (the board you’re sending data to).
uint8_t broadcastAddress[] = {0xEC, 0xDA, 0x3B,0x55,0x38,0x40}; // Lolin S3
// uint8_t broadcastAddress[] = {0x30, 0x30, 0xF9, 0x6D, 0xC5, 0xF0}; // Lolin S3 PRO

DS3231 RTC;  // Create the RTC object
bool Century=false;
bool h12;
bool PM;
byte ADay, AHour, AMinute, ASecond, ABits;
bool ADy, A12h, Apm;

byte year, month, date, DoW, hour, minute, second;

#define WIFI_SSID     SECRET_SSID // "YOUR WIFI SSID NAME"
#define WIFI_PASSWORD SECRET_PASS //"YOUR WIFI PASSWORD"
#define NTP_TIMEZONE  SECRET_NTP_TIMEZONE0 // for example: "Europe/Lisbon"
#define NTP_TIMEZONE_CODE  SECRET_NTP_TIMEZONE0_CODE // for example: "WET0WEST,M3.5.0/1,M10.5.0"
#define NTP_SERVER1   SECRET_NTP_SERVER_1 // for example: "0.pool.ntp.org"
#define NTP_SERVER2   "1.pool.ntp.org"
#define NTP_SERVER3   "2.pool.ntp.org"

#ifdef USE_AHT
#undef USE_AHT
#endif

#ifdef USE_AHT
Adafruit_AHTX0 aht;
float temperature;
float humidity;
Adafruit_Sensor *aht_humidity, *aht_temperature;
sensors_event_t aht20_humidity;
sensors_event_t aht20_temperature;
#endif

// Define variables to store incoming readings
float incomingTemp;
float incomingHum;
float incomingTempOld;
float incomingHumOld;
bool new_data_rcvd = false;
uint8_t bytes_recvd = 0;

// Variable to store if sending data was successful
String success;

//Structure example to send data
//Must match the receiver structure

uint8_t RX_PacketNr = 0;

typedef struct struct_message {
    float temp;
    float hum;
    uint8_t packetNr;
} struct_message;

struct_message AHT20Readings;

typedef struct datetime_message {
  char datetime[21];
} datetime_message;

datetime_message NTPmessage;
datetime_message RTC_DT_message;

// Create a struct_message to hold incoming sensor readings
struct_message incomingReadings;

esp_now_peer_info_t peerInfo;

void neoPixShow(int r=0, int g=0, int b=0) {
  for(int i=0; i<NUMPIXELS; i++) {
    pixels.setPixelColor(i, pixels.Color(r, g, b));
    pixels.show();
    delay(DELAYVAL);
  }
}

bool disconWiFi() {
  static constexpr const char txt0[] PROGMEM = "disconWiFi(): ";
  bool retval = false;
  wl_status_t WStatus;
  if (currWiFiStatus == notSet) {
    Serial.print(txt0);
    Serial.println(F("currWiFiStatus = notSet"));
    retval = true;
  }
  else if (currWiFiStatus == usingWiFi || currWiFiStatus == usingSTA ) {
    while (IsWiFiConnected()) {
      WiFi.disconnect(true);  // disconnect a live WiFi connection
      WiFi.mode(WIFI_OFF); // really switch off
      Serial.print(txt0);
      Serial.print(F("WiFi status: "));
      WStatus = WiFi.status();
      Serial.println(wl_status_to_string(WStatus));
      if (WStatus == WL_STOPPED || WStatus == WL_CONNECTION_LOST) {
        currWiFiStatus = notSet; // update WiFi use status
        retval = true;
        break;
      }
      delay(100);
    }
  }
  return retval;
}

bool setupESPNOW(void) {
  static constexpr const char txt0[] PROGMEM = "setupESPNOW(): ";
  bool retval = false;
  if (currWiFiStatus != notSet) {
    if (!disconWiFi()) { // Try to disconnect WiFi
      Serial.print(txt0);
      Serial.print(F("unable to disconnect active WiFi"));
      return retval;
    }
  }
  // Check again
  if (currWiFiStatus == notSet) {
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);
    Serial.print(txt0);
    Serial.println(F("WiFi mode set to: WIFI_STA"));
    currWiFiStatus = usingSTA; // update the current WiFi use status
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
  }
  return retval;
}

bool DataSentFlag = false;
esp_now_send_status_t LastSentStatus;

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
  }
  else{
    success = "Delivery Failed :(";
  }
}

#else
// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  DataSentFlag = true;
  LastSentStatus = status;
  /*
  // Serial.print("\r\nLast Packet Send Status:\t");
  // Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Failed");
  if (status == 0){
    success = "Delivery Success :)";
  }
  else{
    success = "Delivery Failed :(";
  }
  */
}
#endif 

// Callback when data is received
// Put as few as possible print statements in this function
// because it happened that text got not printed
#ifdef USE_UPDATE
void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  Serial.println("Data received!");

  if (recv_info) {
    Serial.print("From MAC: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", recv_info->src_addr[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
    neoPixShow(60,0,0);
    memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
    bytes_recvd = len; // copy to global var
    incomingTemp = incomingReadings.temp;
    incomingHum = incomingReadings.hum;
    RX_PacketNr = incomingReadings.packetNr;
    new_data_rcvd = true;
    delay(500);                      // wait for a second
    //digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
    neoPixShow(0,0,0);
	// delay(1000);
  }

  Serial.print("Data length: ");
  Serial.println(len);

  Serial.print("Payload: ");
  for (int i = 0; i < len; i++) {
    Serial.printf("%02X ", incomingData[i]);
  }
  Serial.println();
}
#else 
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  //digitalWrite(LED_BUILTIN, HIGH);  // turn the LED on (HIGH is the voltage level)
  neoPixShow(60,0,0);
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
  bytes_recvd = len; // copy to global var
  incomingTemp = incomingReadings.temp;
  incomingHum = incomingReadings.hum;
  RX_PacketNr = incomingReadings.packetNr;
  new_data_rcvd = true;
  delay(500);                      // wait for a second
  //digitalWrite(LED_BUILTIN, LOW);   // turn the LED off by making the voltage LOW
  neoPixShow(0,0,0);
  //delay(1000);
}
#endif

void testdrawtext(char *text, uint16_t color)
{
  EPD.clearBuffer();
  EPD.setTextSize(2);
  EPD.setCursor(0, 0);
  EPD.setTextColor(color);
  EPD.setTextWrap(true);
  EPD.print(text);
  EPD.display();
}

void drawTempHum(void) {
  // Display only if values have changed
  // This makes the e-Paper display update more quiet
  if ((incomingTempOld != incomingTemp) || (incomingHumOld != incomingHum)) {
    if (incomingTempOld != incomingTemp)
      incomingTempOld = incomingTemp;
    if (incomingHumOld != incomingHum)
      incomingHumOld = incomingHum;
    EPD.clearBuffer();
    EPD.setTextSize(2);
    EPD.setTextColor(EPD_RED);
    EPD.setTextWrap(false);
    EPD.setCursor(10, 10);
    EPD.print("INCOMING TEMP & HUM");
    EPD.setCursor(10, 40);
    EPD.printf("Temp: %5.2f deg C\n", incomingTemp);
    EPD.setCursor(10, 60);
    EPD.printf("Hum:  %5.2f % rH\n", incomingHum);
    EPD.setCursor(10, 90);
    EPD.printf("RX PacketNr: %d", RX_PacketNr);
    EPD.display();
  }
}

#ifdef USE_AHT
void getReadings(){
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
  Serial.println(" deg C");
  Serial.print("Humidity: "); 
  Serial.print(aht20_humidity.relative_humidity); 
  Serial.println(" % rH");

}
#endif

void updateDisplay(){

  drawTempHum();
  
  Serial.print(F("\nRX PacketNr: "));
  Serial.printf("%d\n", RX_PacketNr);
  Serial.print(F("Bytes received: "));
  Serial.printf("%d\n", bytes_recvd);
  // Display Readings in Serial Monitor
  Serial.println(F("INCOMING READINGS"));
  Serial.print(F("Temperature: "));
  if (incomingReadings.temp != 0.00) {
    Serial.print(incomingReadings.temp);
    Serial.println(F(" ºC"));
  }
  else {
    Serial.println(F("not available"));
  }
  Serial.print(F("Humidity: "));
  if (incomingReadings.hum != 0.00) {
    Serial.print(incomingReadings.hum);
    Serial.println(" %");
  }
  else {
    Serial.println(F("not available"));
  }
  Serial.println();
}


const char monthsOfTheYear[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
const char daysOfTheWeek[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

void read_RTC() {
  int second,minute,hour,dow,date,month,year;
  float rtc_temperature;

  second=RTC.getSecond();
  minute=RTC.getMinute();
  hour=RTC.getHour(h12, PM);
  dow=RTC.getDoW();
  date=RTC.getDate();
  month=RTC.getMonth(Century);
  year=RTC.getYear();
  
  rtc_temperature=RTC.getTemperature();
  
  //Serial.printf("%4d-%02d-%02d %02d:%02d:%02d\n", year+2000, month, date, hour, minute, second);
  Serial.printf("\n%4d-%3s-%02d %02d:%02d:%02d\n", year+2000, monthsOfTheYear[month-1], date, hour, minute, second);
  Serial.printf("Day of the week: %s\n", daysOfTheWeek[dow]);
  /*
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
  Serial.println();
  */
  Serial.print(F("RTC Temperature: "));
  Serial.printf("%5.2f ºC\n", rtc_temperature);
}

bool IsWiFiConnected(void) {
  return (WiFi.status() == WL_CONNECTED);
}

bool connect_WiFi(void)
{
  char TAG[] = "connect_WiFi(): ";
  bool ret = false;
  if (my_debug)
  {
    Serial.print(F("WiFi: "));
  }
  WiFi.begin( WIFI_SSID, WIFI_PASSWORD );

  for (int i = 20; i && WiFi.status() != WL_CONNECTED; --i)
  {
    if (my_debug)
      Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) 
  {
    currWiFiStatus = usingWiFi; // change status
    ret = true;
    if (my_debug) {
      Serial.print(F("\r\nWiFi Connected to: "));
      Serial.printf("%s\n",WIFI_SSID);
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
    }
    else {
      Serial.print(F("\r\nWiFi Connected"));
    }
  }
  else
  {
    Serial.print(F("\r\nWiFi connection failed."));
  }
  return ret;
}

void prepSendTimeClientDt() {
  // In this function we want to imitate the resultStr returned by NTPClient::getFormattedDate() 
  // String resultStr = String(year) + "-" + monthStr + "-" + dayStr + "T" + this->getFormattedTime(secs ? secs : 0) + "Z";
  bool retval = false;
  timeClient.update(); // refresh
  String formattedTime = timeClient.getFormattedTime();
  String fmtDate = timeClient.getFormattedDate();
  int splitT = fmtDate.indexOf("T");
  
  String dateStamp      = fmtDate.substring(0, splitT);
  String dayStamp       = fmtDate.substring(splitT-2, splitT);
  String monthStamp     = fmtDate.substring(splitT-5, splitT-3);
  String yearStamp      = fmtDate.substring(0, 4);
  String yearStampSmall = fmtDate.substring(2, 4);
  String timeStamp      = fmtDate.substring(splitT+1, fmtDate.length()-1);
  String hourStamp      = timeStamp.substring(0,2);
  String minuteStamp    = timeStamp.substring(3,5);
  String secondStamp    = timeStamp.substring(6,8);

  String resultStr = yearStamp + "-" + monthStamp + "-" + dayStamp + "T" + hourStamp + ":" + minuteStamp + ":" + secondStamp + "Z";
  // Copy resultStr to the struct RTC_DT_message
  uint8_t le = resultStr.length();
  uint8_t i = 0;
  for (i = 0; i < le; i++) {
    RTC_DT_message.datetime[i] = resultStr[i];
  }
  RTC_DT_message.datetime[i] = '\0';
}

void pollTimeClient() {
  timeClient.update();
  String formattedTime = timeClient.getFormattedTime();
  formattedDate = timeClient.getFormattedDate();
  int splitT = formattedDate.indexOf("T");
  
  String dateStamp      = formattedDate.substring(0, splitT);
  String dayStamp       = formattedDate.substring(splitT-2, splitT);
  String monthStamp     = formattedDate.substring(splitT-5, splitT-3);
  String yearStamp      = formattedDate.substring(0, 4);
  String yearStampSmall = formattedDate.substring(2, 4);
  String timeStamp      = formattedDate.substring(splitT+1, formattedDate.length()-1);
  String hourStamp      = timeStamp.substring(0,2);
  String minuteStamp    = timeStamp.substring(3,5);
  String secondStamp    = timeStamp.substring(6,8);

  String resultStr = yearStamp + "-" + monthStamp + "-" + dayStamp + "T" + hourStamp + ":" + minuteStamp + ":" + secondStamp + "Z";

  int year   = yearStamp.toInt();
  int month  = monthStamp.toInt();
  int day    = dayStamp.toInt();
  int hour   = hourStamp.toInt();
  int minute = minuteStamp.toInt();
  int second = secondStamp.toInt();

  boolean isPm = false;
  
  int hourNoMilitary;
  
  if(hour > 12)
  {
    hourNoMilitary = hour - 12;
    isPm = true;
  }
  else
  {
    isPm = false;
    hourNoMilitary = hour;
  }
  if(hour == 0)
  {
    hour = 0;
    hourNoMilitary = 12;
  }
  String hourStampNoMilitary;
  if(hourNoMilitary < 10)
  {
    hourStampNoMilitary = "0"+(String)hourNoMilitary;
  }
  else
  {
    hourStampNoMilitary = (String)hourNoMilitary;
  }

  int daysLeft = 0;
  int monthsLeft = 0;

  String timeStampNoMilitary = hourStampNoMilitary + ":" + minuteStamp + ":" + secondStamp;    //hh:mm:ss
  String dateStampConstructed = monthStamp + "/" + dayStamp + "/" + yearStampSmall;  //mm/dd/yy
  
  EPD.setTextSize(6);
  EPD.fillScreen(EPD_BLACK);
  EPD.setCursor(0, 10);
  EPD.print(dateStampConstructed);
  EPD.println(timeStampNoMilitary);
  EPD.setTextSize(2);
  if(isPm)
  {
    EPD.setCursor(EPD_Width-10, 80);
    EPD.println("PM");
  }
  elseNTPdatetime
  {
    EPD.setCursor(EPD_Width-10, 60);
    EPD.println("AM");
  }
}

void setExtRTC(void) {
  timeClient.update();
  Serial.print(F("Datetime from NTP-server: "));
  String fmtDt = timeClient.getFormattedDate();  // set global var
   // Copy the String to the char buffer
  fmtDt.toCharArray(rcvdNTPdatetime, sizeof(rcvdNTPdatetime));

  Serial.println(rcvdNTPdatetime);
  int ss = timeClient.getSeconds();
  int mm = timeClient.getMinutes();
  int hh = timeClient.getHours();
  int wd = timeClient.getDay();
  int dd = timeClient.getDate();
  int mo = timeClient.getMonth();
  int yy = timeClient.getYear() -2000;
  RTC.setSecond(ss);//Set the second 
  RTC.setMinute(mm);//Set the minute 
  RTC.setHour(hh);  //Set the hour 
  RTC.setDoW(wd);    //Set the day of the week, Sunday = 0
  RTC.setDate(dd);  //Set the date of the month
  RTC.setMonth(mo);  //Set the month of the year
  RTC.setYear(yy);  //Set the year (Last two digits of the year)
  Serial.printf("RTC set to: %s %4d-%02d-%02d %02d:%02d:%02d\n", daysOfTheWeek[wd], yy, mo, dd, hh, mm, ss);
}

const char* wl_status_to_string(wl_status_t status) {
  switch (status) {
    case WL_NO_SHIELD: return "WL_NO_SHIELD";
    case WL_STOPPED: return "WL_STOPPED";
    case WL_IDLE_STATUS: return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL: return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED: return "WL_SCAN_COMPLETED";
    case WL_CONNECTED: return "WL_CONNECTED";
    case WL_CONNECT_FAILED: return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST: return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED: return "WL_DISCONNECTED";
  }
}

void setup() {
    static constexpr const char txt0[] PROGMEM = "setup(): ";
    static constexpr const char txt1[] PROGMEM = "LOLIN S3 PRO";
    static constexpr const char txt2[] PROGMEM = "ESP-NOW TEST";
  // Init Serial Monitor
  Serial.begin(115200);

  Wire.begin();

#ifdef RTC_SET_MANUAL
  RTC.setSecond(0);//Set the second 
  RTC.setMinute(30);//Set the minute 
  RTC.setHour(12);  //Set the hour 
  RTC.setDoW(2);    //Set the day of the week
  RTC.setDate(12);  //Set the date of the month
  RTC.setMonth(3);  //Set the month of the year
  RTC.setYear(25);  //Set the year (Last two digits of the year)
#endif

  pixels.begin();

  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(115200);

#ifdef USE_AHT
  // Init AHT20 sensor
  if (! aht.begin()) {
    Serial.print(txt0);
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  Serial.println("AHT20 found");
#endif
  Serial.printf("\n\n%s %s\n", txt1, txt2);
  EPD.begin();
  Serial.print(txt0);
  Serial.println("EPD initialized)");

  // Large block of text
  EPD.clearBuffer();
  EPD.fillScreen(EPD_WHITE);

  // Define buffer sizes
  char buffer1[20];
  char buffer2[20];
  char finalString[50];

  // Read values from PROGMEM into buffers
  strcpy_P(buffer1, txt1);
  strcpy_P(buffer2, txt2);

  // Combine strings with the desired format
  snprintf(finalString, sizeof(finalString), "   %s\n\n   %s", buffer1, buffer2);

  // Output the combined string
  //Serial.println(finalString);

  // Call testdrawtext with the final string
  testdrawtext(finalString, EPD_RED);

  //testdrawtext("   LOLIN S3 PRO\n\n   ESP-NOW TEST", EPD_RED);

  /* Try to establish WiFi connection. If so, Initialize NTP, */
  if (connect_WiFi())
  {
    Serial.print(txt0);
    Serial.println(F("Starting NTP timeClient"));
    timeClient.begin();
    delay(1000);
    setExtRTC();
    if (!disconWiFi()) {
      Serial.print(txt0);
      Serial.print(F("unable to disconnect active WiFi"));
      return;
    } else {
      Serial.print(txt0);
      Serial.println(F("WiFi disconnected and mode switched to OFF"));
      
      Serial.print(F("WiFi status: "));
      Serial.println(wl_status_to_string(WiFi.status()));
    }
  }
  bool dummy = setupESPNOW();
}

void loop() {
  static constexpr const char txt0[] PROGMEM = "loop(): ";
  unsigned long current_t = 0;
  unsigned long interval_t = 10000; // 10 second interval. (not to rapid refreshes on the e-Paper display!)
  unsigned long rtc_start_t = start_t;
  unsigned long rtc_current_t = 0;
  unsigned long rtc_interval_t = 1000;  // 1 second interval 
  unsigned long dt_tx_start_t = start_t;
  unsigned long dt_tx_current_t = 0;
  unsigned long dt_tx_interval_t = 60 * 60000; // 1 hour interval for datetime send.
 
  bool lStart = true;

  if (lStart) {
    lStart = false;
    Serial.print(F("rtc interval set to: "));
    Serial.print(rtc_interval_t / 1000);
    Serial.println(F(" second(s)."));
    Serial.print(F("e-Paper display refresh interval set to: "));
    Serial.print(interval_t / 1000);
    Serial.println(F(" seconds."));
    Serial.print(F("Datetime send interval set to: "));
    Serial.print(dt_tx_interval_t / 1000);
    Serial.println(F(" seconds."));
    esp_err_t result = esp_now_send(broadcastAddress, reinterpret_cast<const uint8_t*>(&rcvdNTPdatetime), sizeof(rcvdNTPdatetime));
    if (result == ESP_OK) {
      Serial.println(F("Sent NTP datetime with success"));
    }
    else {
      Serial.println(F("Sending NTP datetime failed"));
    }
  }
  while (true) {
    dt_tx_current_t = millis();
    if (dt_tx_current_t - dt_tx_start_t >= dt_tx_interval_t) {
      dt_tx_start_t = dt_tx_current_t;
      // Send message via ESP-NOW
      prepSendTimeClientDt();
      Serial.print(txt0);
      Serial.printf("going to send RTC datetime: \"%s\"\n", RTC_DT_message.datetime);
      esp_err_t result = esp_now_send(broadcastAddress, reinterpret_cast<const uint8_t*>(&RTC_DT_message.datetime), sizeof(RTC_DT_message.datetime));
      Serial.printf("Datetime send result (esp_err_t) = %d\n", result);
      if (result == ESP_OK) {
        Serial.println(F("Datetime sent with success"));
      }
      else {
        Serial.println(F("Datetime send failed"));
      }
    }
    if (DataSentFlag) {
      DataSentFlag = false; // reset flag
      Serial.print(F("\r\nLast Packet Send Status:\t"));
      Serial.println(LastSentStatus == ESP_NOW_SEND_SUCCESS ? "Delivery Success :)" : "Delivery Failed (:");
      Serial.printf("LastSentStatus = %d\n", LastSentStatus);
      Serial.printf("ESP_NOW_SEND_SUCCESS = %d\n", ESP_NOW_SEND_SUCCESS);
      /*
      if (LastSentStatus == 0){
        success = "Delivery Success :)";
      }
      else{
        success = "Delivery Failed :(";
      }
      */
    }
    rtc_current_t = millis();
    if (current_t - rtc_start_t >= rtc_interval_t) {
      rtc_start_t = rtc_current_t;
      read_RTC();
      // pollTimeClient();
    }
    current_t = millis();
    if (current_t - start_t >= interval_t) {
        start_t = current_t;

#ifdef USE_AHT
      // Get readings from sensor
      getReadings();
      // Set values to send
      AHT20Readings.temp = temperature;
      AHT20Readings.hum = humidity;

      // Send message via ESP-NOW
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &AHT20Readings, sizeof(AHT20Readings));
      if (result == ESP_OK) {
        Serial.println(F("Data sent with success"));
      }
      else {
        Serial.println(F("Sending data failed"));
      }
#endif
      if (new_data_rcvd) {
        updateDisplay();
        new_data_rcvd = false; // reset flag
      }
    }
  } // end-of-while
}
