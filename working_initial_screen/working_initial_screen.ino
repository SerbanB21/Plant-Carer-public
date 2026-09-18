#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_APDS9960.h>
#include <DHT.h>

// --- OLED Software SPI Settings ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_CLK    8   
#define OLED_MOSI   9   
#define OLED_RESET  10  
#define OLED_DC     11  
#define OLED_CS     12  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

// --- Gesture Sensor ---
Adafruit_APDS9960 apds;

// --- DHT11 Settings ---
#define DHTPIN 4     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// --- Soil Moisture Settings ---
const int sensorPin = A0;
const int AirValue = 500;   
const int WaterValue = 260; 

// --- LED Settings ---
const int redPin = 5;
const int greenPin = 6;

// --- Global Variables ---
int currentScreen = 0; 
float currentT = 0;
float currentH = 0;
int currentSoil = 0;

// --- Data Queue (20 Points) ---
uint8_t tempQueue[20];
uint8_t humQueue[20];
uint8_t soilQueue[20];

unsigned long lastReadTime = 0;

void setup() {
  Serial.begin(115200);

  // Initialize LED Pins
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  
  // Initialize Screen
  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("OLED failed"));
    for(;;); 
  }
  display.setTextWrap(false); 
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.print(F("Booting..."));
  display.display();

  // Warm up DHT11 and initialize
  delay(2000); 
  dht.begin();

  // Initialize Gesture Sensor
  if(!apds.begin()){
    Serial.println(F("APDS-9960 failed"));
    for(;;); 
  }
  apds.enableProximity(true);
  apds.enableGesture(true);

  // Initial Sensor Read
  currentT = dht.readTemperature();
  currentH = dht.readHumidity();
  if(isnan(currentT)) currentT = 20.0; 
  if(isnan(currentH)) currentH = 50.0; 
  readSoilMoisture();

  // Pre-fill the queue
  for(int i = 0; i < 20; i++) {
    tempQueue[i] = (uint8_t)currentT;
    humQueue[i] = (uint8_t)currentH;
    soilQueue[i] = (uint8_t)currentSoil;
  }

  // Draw the very first frame and sync LED
  display.clearDisplay();
  drawFrame(currentScreen, 0, true); 
  display.display();
  updateLED(); // Set the initial LED color exactly as the screen draws
}

void loop() {
  // 1. WATCH FOR GESTURES (Runs constantly)
  uint8_t gesture = apds.readGesture();
  
  if (gesture == APDS9960_LEFT || gesture == APDS9960_UP) {
    int nextScreen = currentScreen + 1;
    if (nextScreen > 2) nextScreen = 0; 
    animateScroll(currentScreen, nextScreen, 1);
    currentScreen = nextScreen;
  } 
  else if (gesture == APDS9960_RIGHT || gesture == APDS9960_DOWN) {
    int nextScreen = currentScreen - 1;
    if (nextScreen < 0) nextScreen = 2; 
    animateScroll(currentScreen, nextScreen, -1); 
    currentScreen = nextScreen;
  }

  // 2. READ SENSORS EVERY 5 SECONDS
  if (millis() - lastReadTime >= 5000) {
    lastReadTime = millis();

    // Read new values
    float newH = dht.readHumidity();
    float newT = dht.readTemperature();
    if (!isnan(newH)) currentH = newH;
    if (!isnan(newT)) currentT = newT;
    
    readSoilMoisture();

    // Shift the queue to the left
    for(int i = 0; i < 19; i++) {
      tempQueue[i] = tempQueue[i+1];
      humQueue[i] = humQueue[i+1];
      soilQueue[i] = soilQueue[i+1];
    }
    
    // Insert the newest reading at the end (Index 19)
    tempQueue[19] = (uint8_t)currentT;
    humQueue[19] = (uint8_t)currentH;
    soilQueue[19] = (uint8_t)currentSoil;

    // Refresh the screen quietly
    display.clearDisplay();
    drawFrame(currentScreen, 0, true);
    display.display();
    
    // Sync the physical LED immediately after the screen draws the new point
    updateLED(); 
  }
}

// --- HELPER: Read Soil Moisture ---
void readSoilMoisture() {
  int sensorValue = analogRead(sensorPin);
  currentSoil = map(sensorValue, AirValue, WaterValue, 0, 100);
  if(currentSoil > 100) currentSoil = 100; 
  if(currentSoil < 0) currentSoil = 0; 
}

// --- HELPER: Update LED Color based on Soil ---
//put the values very low to test LEDs
void updateLED() {
  if (currentSoil < 5) {
    // Yellow (Too Dry)
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
  } 
  else if (currentSoil <= 10) {
    // Green (Optimal)
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
  } 
  else {
    // Red (Too Wet - Over 60%)
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, HIGH);
  }
}

// --- HELPER: Draw Graphing Interface ---
void drawFrame(int mode, int x_offset, bool showGraph) {
  display.setTextColor(SSD1306_WHITE);
  
  // Draw Top Text
  display.setCursor(x_offset, 0);
  display.setTextSize(1);
  if (mode == 0) {
    display.print(F("Temp: ")); display.print(currentT, 1); display.print(F(" C"));
  } else if (mode == 1) {
    display.print(F("Humidity: ")); display.print(currentH, 0); display.print(F(" %"));
  } else if (mode == 2) {
    display.print(F("Soil Moist: ")); display.print(currentSoil); display.print(F(" %"));
  }

  // Skip plotting the dots during swipe animations
  if (!showGraph) return;

  // Draw simple axes
  display.drawLine(x_offset + 22, 14, x_offset + 22, 63, SSD1306_WHITE); 
  display.drawLine(x_offset + 22, 63, x_offset + 127, 63, SSD1306_WHITE); 

  // Draw axis labels
  display.setTextSize(1);
  if (mode == 0) {
    display.setCursor(x_offset, 14); display.print(F("30"));  
    display.setCursor(x_offset + 6, 56); display.print(F("15"));
  } else {
    display.setCursor(x_offset, 14); display.print(F("100")); 
    display.setCursor(x_offset + 12, 56); display.print(F("0"));
  }

  // PLOT THE DOTS
  for (int i = 0; i < 20; i++) {
    int y;
    
    if (mode == 0) {
      y = map(tempQueue[i], 15, 30, 62, 14);
    } else if (mode == 1) {
      y = map(humQueue[i], 0, 100, 62, 14);
    } else if (mode == 2) {
      y = map(soilQueue[i], 0, 100, 62, 14);
    }

    y = constrain(y, 14, 62);
    int x_pos = x_offset + 26 + (i * 5); 

    // Draw a 2x2 pixel square
    display.fillRect(x_pos, y, 2, 2, SSD1306_WHITE);
  }
}

// --- HELPER: Slide animation between screens ---
void animateScroll(int prevMode, int nextMode, int direction) {
  int step = 16; 
  for (int i = 0; i <= SCREEN_WIDTH; i += step) {
    display.clearDisplay();
    if (direction == 1) { 
      drawFrame(prevMode, -i, false); 
      drawFrame(nextMode, SCREEN_WIDTH - i, false);
    } else { 
      drawFrame(prevMode, i, false);
      drawFrame(nextMode, -SCREEN_WIDTH + i, false);
    }
    display.display();
  }
  
  display.clearDisplay();
  drawFrame(nextMode, 0, true);
  display.display();
}