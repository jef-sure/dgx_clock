# This is my test project to learn ESP32

## Achivements

### How to program TFT SPI displays

I successfully created a generalized graphics library for wide spectre TFT displays. Unfortunately, I have only one based on ST7735 chip. 
It supports 12, 16 and 18 bits per color pixel. There should be little effort to add support for more displays.

### How to program I2C OLED displays

The "dgx" component was also used for I2C SSD1306 OLED display. It wasn't actually used in this project though.

### Own grapics library with some extra features

The "dgx" graphics library supports virtual screens, bitmaps, fonts, some usual geometric primitives and also some 
not so usual geometric primitives like bit-masked circles.

## Explored features of ESP-IDF

* SPI
* I2C
* DMAS
* Heap memory classes
* HTTP client
* cJSON
* WiFi
* SNTP

## What the project does?

1. It draws backround using bit-masked lines
2. Starts to get IP from WiFi router
3. Starts SNTP time synchronization
3. Draws clockface and clock hands over background
4. Determines it's location using [ip-api.com](http://ip-api.com/json) service
5. Sets its timezone according to determined location
6. Obtains current weather for determined location and draws it over background but under clock hands
7. Draws clock hands every second
8. Periodically refreshes location and weather

## What does it look like

![video](https://github.com/jef-sure/dgx_clock/video_2021-05-16_16-08-26.gif)