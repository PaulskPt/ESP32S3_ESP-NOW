# ESP32S3 ESP-NOW test

## PURPOSE: 
  Test ESP-NOW communication between two microcontroller boards, each with an Espressif ESP32-S3 processor.
  One device is set to function as the "Master" device.
  The other device is set to function as the "Slave" device.

## Hardware used

    1) Lolin ESP32-S3 (master);
    2) Lolin ESP32-S3 PRO (slave);
    3) 2.13inch 3-color e-paper display;
    4) temperature and humidity sensor;
    5) external RTC module.


## Software
The software consists of two parts: 

  a) for the master device;
  
  b) for the slave device.
  
  In the "src" folder is a subfolder with a sketch to determine the MAC-address for each device [here](https://github.com/PaulskPt/ESP32S3_ESP-NOW/tree/main/src/get_MacAddress).

## MASTER DEVICE

Hardware connected to the Lolin S3 PRO (slave) device:

a) a temperature and humidity sensor.

DESCRIPTION

  At startup this device will wait to receive an NTP datetime stamp from the slave device (which has an external RTC connected).
  After reception of the datetime stamp, this sketch will set the internal RTC with the received NTP datetime stamp.
  Next, with an interval of one hour, the slave device will send an updated datetimestamp (this time from it's external RTC).
  Every time this sketch will then update the internal RTC of this master device.
  This master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to the slave device.
  At the moment of a successful transmission, the builtin RGB led will blink GREEN. If transmission fails, the led will blink RED.
  Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).

## SLAVE DEVICE

Hardware connected to the Lolin S3 PRO (slave) device:

  a) a small DS3231 external RTC board marked "DS3231 for Pi";
  
  b) a Lolin 2.13inch e-Paper 3-color display, 250x122px. Needs EPD library. See [repo](https://github.com/wemos/LOLIN_EPD_Library)


DESCRIPTION

  At startup this device will connect to the internet via WiFi.
  When WiFi is established, this sketch will get a datetime stamp from a NTP-server.
  After reception of the NTP datetime stamp, this sketch will set the external RTC with the received datetime stamp.
  Immediately after this sketch will send the datetime stamp to the master device.
  Next, with an interval of one hour, sending an updated datetime stamp to the master device (this time from the external RTC).
  The master device will sent, at intervals of (initially) 10 seconds, temperature, humidity and packet number to this slave device.
  At the moment of a successful reception, the builtin RGB led will blink GREEN.
  After reception this sketch will display the received data on the connected e-Paper display.
  Information in text of the actions taken place, will be printed to the Serial Monitor window of the Arduino IDE (or other serial modem app).

## File secret.h (used with the slave device):

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
## Docs

Monitor_output.txt.


## Images

Images, mostly edited screenshots, are in the folder: "images".


## Links to product pages of the hardware used:

- Lolin S3 [info](https://pt.aliexpress.com/item/1005004643475363.html?gatewayAdapt=glo2bra);
- Lolin S3 PRO [info](https://pt.aliexpress.com/item/1005004931357085.html?gatewayAdapt=glo2bra);
- Waveshare 2.13in 3-color e-Paper display [info](https://www.waveshare.com/wiki/Main_Page#Display-e-Paper);
- External RTC DS3231 for Pi [info](https://www.aliexpress.com/p/tesla-landing/index.html?scenario=c_ppc_item_bridge&productId=1005008622717984&_immersiveMode=true&withMainCard=true&src=google&aff_platform=true&isdl=y&src=google&albch=shopping&acnt=615-992-9880&isdl=y&slnk=&plac=&mtctp=&albbt=Google_7_shopping&aff_platform=google&aff_short_key=_oFgTQeV&gclsrc=aw.ds&&albagn=888888&&ds_e_adid=&ds_e_matchtype=&ds_e_device=c&ds_e_network=x&ds_e_product_group_id=&ds_e_product_id=pt1005008622717984&ds_e_product_merchant_id=761104415&ds_e_product_country=PT&ds_e_product_language=pt&ds_e_product_channel=online&ds_e_product_store_id=&ds_url_v=2&albcp=22568097293&albag=&isSmbAutoCall=false&needSmbHouyi=false&gad_source=1&gad_campaignid=22561749573&gbraid=0AAAAA_TvRHpgQaBmOOx9v5-Ms7Xf8kTNE&gclid=Cj0KCQjwrJTGBhCbARIsANFBfgvvqc1KjWdRe_i8AZGMX5ut5M7Z6T4P3YPPCLKOHh16SmM8tELUXNIaAtCzEALw_wcB);
- Adafruit AHT20 temperature and humidity sensor [info](https://www.adafruit.com/product/4566);


## Update 2025-09-13

The sketches of the master and slave have been updated because the ESP-NOW API has changed in ESP-IDF v5.0+.

## Known Issues

None.


