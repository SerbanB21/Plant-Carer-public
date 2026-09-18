#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_APDS9960.h>
#include <DHT.h>

// --- PICO 2 W PIN DEFINITIONS ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_CLK    18  
#define OLED_MOSI   19  
#define OLED_RESET  20  
#define OLED_DC     21  
#define OLED_CS     22  

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);

Adafruit_APDS9960 apds;

#define DHTPIN 16     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

const int sensorPin = 26; 
const int AirValue = 500;   
const int WaterValue = 260; 

const int redPin = 14;    
const int greenPin = 15;  
const int speakerPin = 13; 

// Global Variables
int currentScreen = 0; 
float currentT = 0;
float currentH = 0;
int currentSoil = 0;

// --- UPGRADED DATA QUEUE (100 Points) ---
#define GRAPH_POINTS 100
uint8_t tempQueue[GRAPH_POINTS];
uint8_t humQueue[GRAPH_POINTS];
uint8_t soilQueue[GRAPH_POINTS];

unsigned long lastReadTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  
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

  delay(2000); 
  dht.begin();

  // Force I2C pins for Pico
  Wire.setSDA(4);
  Wire.setSCL(5);
  Wire.begin();

  if(!apds.begin()){
    Serial.println(F("APDS-9960 failed"));
    for(;;); 
  }
  apds.enableProximity(true);
  apds.enableGesture(true);

  currentT = dht.readTemperature();
  currentH = dht.readHumidity();
  if(isnan(currentT)) currentT = 20.0; 
  if(isnan(currentH)) currentH = 50.0; 
  readSoilMoisture();

  // Pre-fill the larger queue
  for(int i = 0; i < GRAPH_POINTS; i++) {
    tempQueue[i] = (uint8_t)currentT;   
    humQueue[i] = (uint8_t)currentH;
    soilQueue[i] = (uint8_t)currentSoil;
  }

  display.clearDisplay();
  drawFrame(0, 0, true); 
  display.display();
  updateLED(); 
}

void loop() {
  // 1. WATCH FOR GESTURES
  uint8_t gesture = apds.readGesture();
  
  if (gesture == APDS9960_LEFT || gesture == APDS9960_UP) {
    int nextScreen = currentScreen + 1;
    if (nextScreen > 3) nextScreen = 1; 
    animateScroll(currentScreen, nextScreen, 1);
    currentScreen = nextScreen;
    updateLED(); // Update LED immediately on screen change
  } 
  else if (gesture == APDS9960_RIGHT || gesture == APDS9960_DOWN) {
    int nextScreen = currentScreen - 1;
    if (nextScreen < 1) nextScreen = 3; 
    animateScroll(currentScreen, nextScreen, -1); 
    currentScreen = nextScreen;
    updateLED(); // Update LED immediately on screen change
  }

  // 2. READ SENSORS EVERY 1 SECOND
  if (millis() - lastReadTime >= 1000) {
    lastReadTime = millis();

    float newH = dht.readHumidity();
    float newT = dht.readTemperature();
    if (!isnan(newH)) currentH = newH;
    if (!isnan(newT)) currentT = newT;
    
    readSoilMoisture();

    // Shift the queue to the left
    for(int i = 0; i < GRAPH_POINTS - 1; i++) {
      tempQueue[i] = tempQueue[i+1];
      humQueue[i] = humQueue[i+1];
      soilQueue[i] = soilQueue[i+1];
    }
    
    // Insert the newest reading at the end
    tempQueue[GRAPH_POINTS - 1] = (uint8_t)currentT;
    humQueue[GRAPH_POINTS - 1] = (uint8_t)currentH;
    soilQueue[GRAPH_POINTS - 1] = (uint8_t)currentSoil;

    display.clearDisplay();
    drawFrame(currentScreen, 0, true);
    display.display();
    
    updateLED(); 
  }
}

void readSoilMoisture() {
  int sensorValue = analogRead(sensorPin);
  currentSoil = map(sensorValue, AirValue, WaterValue, 0, 100);
  if(currentSoil > 100) currentSoil = 100; 
  if(currentSoil < 0) currentSoil = 0; 
}

// --- NEW: Health Logic Evaluator ---
// Returns 0 = Bad, 1 = Average, 2 = Good
int checkStatus(int mode) {
  if (mode == 1) { // TEMPERATURE
    if (currentT >= 18 && currentT <= 27) return 2; // Good (18-27C)
    if ((currentT >= 10 && currentT < 18) || (currentT > 27 && currentT <= 32)) return 1; // Avg
    return 0; // Bad (<10C or >32C)
  } 
  else if (mode == 2) { // HUMIDITY
    if (currentH >= 40 && currentH <= 70) return 2; // Good (40-70%)
    if ((currentH >= 30 && currentH < 40) || (currentH > 70 && currentH <= 80)) return 1; // Avg
    return 0; // Bad (<30% or >80%)
  } 
  else { // SOIL MOISTURE (Mode 3, or Mode 0 default)
    if (currentSoil > 30 && currentSoil <= 85) return 2; // Good (31-85%)
    if (currentSoil >= 15 && currentSoil <= 30) return 1; // Avg (15-30%)
    return 0; // Bad (<15% or >85%)
  }
}

// Update LED Color based on the CURRENT screen being viewed
void updateLED() {
  // If on intro screen (0), default to looking at soil moisture health (3)
  int status = checkStatus(currentScreen == 0 ? 3 : currentScreen);

  if (status == 0) { // BAD (Red LED + Buzz)
    digitalWrite(redPin, LOW); 
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 85); 
  } 
  else if (status == 1) { // AVERAGE (Yellow LED)
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 0);
  } 
  else { // GOOD (Green LED)
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    analogWrite(speakerPin, 0);
  }
}

// --- NEW: Draw the Emoji Face ---
void drawFace(int x, int y, int status) {
  // Face Outline
  display.drawCircle(x, y, 5, SSD1306_WHITE);
  
  // Eyes
  display.drawPixel(x - 2, y - 1, SSD1306_WHITE);
  display.drawPixel(x + 2, y - 1, SSD1306_WHITE);
  
  if (status == 2) { // Smiley Face (Good)
    display.drawPixel(x - 2, y + 1, SSD1306_WHITE);
    display.drawPixel(x + 2, y + 1, SSD1306_WHITE);
    display.drawLine(x - 1, y + 2, x + 1, y + 2, SSD1306_WHITE);
  } 
  else if (status == 1) { // Neutral Face (Average)
    display.drawLine(x - 2, y + 2, x + 2, y + 2, SSD1306_WHITE);
  } 
  else { // Sad Face (Bad)
    display.drawPixel(x - 2, y + 2, SSD1306_WHITE);
    display.drawPixel(x + 2, y + 2, SSD1306_WHITE);
    display.drawLine(x - 1, y + 1, x + 1, y + 1, SSD1306_WHITE);
  }
}

void drawFrame(int mode, int x_offset, bool showGraph) {
  display.setTextColor(SSD1306_WHITE);
  
  if (mode == 0) {
    display.setTextSize(2);
    display.setCursor(x_offset + 15, 10);
    display.print(F("PLANT"));
    display.setCursor(x_offset + 15, 30);
    display.print(F("MONITOR"));
    
    display.setTextSize(1);
    display.setCursor(x_offset + 10, 52);
    display.print(F("SWIPE > TO UNLOCK"));
    return; 
  }

  // Draw Header Text
  display.setCursor(x_offset, 0);
  display.setTextSize(1);
  if (mode == 1) {
    display.print(F("Temp: ")); display.print(currentT, 1); display.print(F(" C"));
  } else if (mode == 2) {
    display.print(F("Humidity: ")); display.print(currentH, 0); display.print(F(" %"));
  } else if (mode == 3) {
    display.print(F("Soil Moist: ")); display.print(currentSoil); display.print(F(" %"));
  }

  // DRAW THE EMOJI FACE IN TOP RIGHT CORNER
  drawFace(x_offset + 115, 5, checkStatus(mode));

  if (!showGraph) return;

  // Draw axes
  display.drawLine(x_offset + 22, 14, x_offset + 22, 63, SSD1306_WHITE); 
  display.drawLine(x_offset + 22, 63, x_offset + 127, 63, SSD1306_WHITE); 

  display.setTextSize(1);
  if (mode == 1) {
    display.setCursor(x_offset, 14); display.print(F("30"));  
    display.setCursor(x_offset + 6, 56); display.print(F("15"));
  } else {
    display.setCursor(x_offset, 14); display.print(F("100")); 
    display.setCursor(x_offset + 12, 56); display.print(F("0"));
  }

  // PLOT THE CONTINUOUS LINE GRAPH
  for (int i = 0; i < GRAPH_POINTS - 1; i++) {
    int y1, y2;
    
    if (mode == 1) {
      y1 = map(tempQueue[i], 15, 30, 62, 14);
      y2 = map(tempQueue[i+1], 15, 30, 62, 14);
    } else if (mode == 2) {
      y1 = map(humQueue[i], 0, 100, 62, 14);
      y2 = map(humQueue[i+1], 0, 100, 62, 14);
    } else if (mode == 3) {
      y1 = map(soilQueue[i], 0, 100, 62, 14);
      y2 = map(soilQueue[i+1], 0, 100, 62, 14);
    }

    y1 = constrain(y1, 14, 62);
    y2 = constrain(y2, 14, 62);

    int x1 = x_offset + 26 + i; 
    int x2 = x_offset + 26 + (i + 1); 

    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
}

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