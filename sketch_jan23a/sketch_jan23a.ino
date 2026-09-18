#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// SPI pin mapping (Arduino Uno / Nano)
#define OLED_MOSI   18
#define OLED_CLK    19
#define OLED_DC     9
#define OLED_CS     10
#define OLED_RESET  8

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,
                         OLED_MOSI,
                         OLED_CLK,
                         OLED_DC,
                         OLED_RESET,
                         OLED_CS);



void setup() {
  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    while (1); // stop if OLED not detected
  }

 display.clearDisplay();       // clear old pixels
display.setCursor(0, 0);      // set top-left
display.setTextSize(2);       // choose size
display.setTextColor(SSD1306_WHITE);
display.println("Hello world");
display.println("");
display.display();   
}

void loop() {
  // nothing needed
}