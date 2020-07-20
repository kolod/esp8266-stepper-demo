# esp8266-stepper-demo
Demo project for the ESP8266 with stepper motor and ULN2003 driver.

## Dependencies:

### Libs:
* [CheapStepper](https://github.com/kolod/CheapStepper) - Library for the ULN2003 stepper motor driver.
* [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) - Async HTTP and WebSocket Server for ESP8266 Arduino
* [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) - Async TCP Library for ESP8266 Arduino.
* [uTimerLib](https://github.com/Naguissa/uTimerLib) - Hardware timer library.
* [ArduinoJson](https://arduinojson.org/) - JSON library.

### Tools:
* [ESP8266LittleFS](https://github.com/earlephilhower/arduino-esp8266littlefs-plugin) - LittleFS uploading tool which integrates into the Arduino IDE.
    
### Configuration
* Copy ```data/config.in``` to ```data/config```.
* Change WiFi SSID & password in ```data/config```.
