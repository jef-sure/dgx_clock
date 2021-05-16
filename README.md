# This is my test project to learn ESP32

## Archivements

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
* DMA
* Heap memory classes
* HTTP client
* cJSON
* WiFi
* SNTP
