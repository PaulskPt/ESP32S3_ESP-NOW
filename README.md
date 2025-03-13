ESP32S3 ESP-NOW test

PURPOSE: 
  Test ESP-NOW communication between two microcontroller boards, each with an Esprssif ESP32-S3 processor.
  One device is set to function as the "Master" device.
  The other device is set to function as the "Device" device.

Hardware used:

    1) Lolin ESP32-S3 (master);
    2) Lolin ESP32-S3 PRO (slave);
    3) 2.13inch 3-color e-paper display;
    4) temperature and humidity sensor;
    5) external RTC module.

The software consists of two parts: 
a) for the master device; 
b) for the slave device.
In the "src" folder is also a subfolder with a sketch you can use to determine the MAC-address for each of the two devices used.

THE MASTER DEVICE

Hardware connected to the Lolin S3 PRO (slave) device:
a) a temperature and humidity sensor.

SHORT DESCRIPTION:
  At startup this device will wait to receive a datetime stamp from the slave device (which has an external RTC connected).
  After reception of the datetime stamp, this sketch will set the internal RTC with the received datetime stamp.
  Next, with an interval of one hour, the slave device will send an updated datetimestamp (this time from it's external RTC).
  Every time this sketch will then update the internal RTC of this device.
  This master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to the slave device.
  At the moment of a successful transmission, the builtin RGB led will blink GREEN. If transmission fails, the led will blink RED.
  Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).

THE SLAVE DEVICE

Hardware connected to the Lolin S3 PRO (slave) device:
  a) a small DS3231 external RTC board marked "DS3231 for Pi";
  b) a Lolin 2.13inch e-Paper 3-color display, 250x122px. Needs SSD1680 driver module.


SHORT DESCRIPTION:
  At startup this device will connect to the internet via WiFi.
  When WiFi is established, this sketch will get a datetime stamp from a NTP-server.
  After reception of the NTP datetime stamp, this sketch will set the external RTC with the received datetimestamp.
  Immediately after this sketch will send the datetime stamp to the master device.
  Next, with an interval of one hour, sending an updated datetimestampto the master device (this time from the external RTC).
  The master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to this slave device.
  At the moment of a successful reception, the builtin RGB led will blink GREEN.
  After reception this sketch will display the received data on the connected e-Paper display.
  Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).

File secret.h (used with the slave device):

Update the file secret.h as far as needed:
```
 a) your WiFi SSID in SECRET_SSID;
 b) your WiFi PASSWORD in SECRET_PASS;
 c) the name of the NTP server of your choice in SECRET_NTP_SERVER_1, for example: 2.pt.pool.ntp.org;
 d) the SECRET_NTP_NR_OF_ZONES as a string, default: "1";
 e) the TIMEZONE and TIMEZONE_CODE texts for the timezone you want to use.

 At this moment file secret.h has the following timezone and timezone_code defined:
    #define SECRET_NTP_TIMEZONE3 "Europe/Lisbon"
    #define SECRET_NTP_TIMEZONE3_CODE "WET0WEST,M3.5.0/1,M10.5.0"

```

Docs:

```
Monitor_output.txt.


Images: 

Images, mostly edited screenshots, are in the folder: ```images```.


Links to product pages of the hardware used:

- Lolin S3 [info](https://pt.aliexpress.com/item/1005004643475363.html?gatewayAdapt=glo2bra);
- Lolin S3 PRO [info](https://pt.aliexpress.com/item/1005004931357085.html?gatewayAdapt=glo2bra);
- Waveshare 2.13in 3-color e-Paper display [info](https://www.waveshare.com/wiki/Main_Page#Display-e-Paper);
- External RTC DS3231 for Pi [info](https://www.miravia.es). Aliexpress also sells this model of RTC;
- Adafruit AHT20 temperature and humidity sensor [info](https://www.adafruit.com/product/4566);


Known Issues:
None.


