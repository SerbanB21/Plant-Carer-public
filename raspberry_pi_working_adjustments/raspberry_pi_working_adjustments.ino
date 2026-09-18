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

const int sensorPin = 27; 
const int AirValue = 500;   
const int WaterValue = 400; 

const int redPin = 14;    
const int greenPin = 15;  
const int speakerPin = 13; 
#define PUMP_PIN 17 // <-- NEW: Pin for the Relay/Transistor controlling the pump

// --- BUTTON PINS ---
#define BTN_LEFT 10
#define BTN_MID 11
#define BTN_RIGHT 12

// Global Variables
int currentScreen = 0; 
float currentT = 0;
float currentH = 0;
int currentSoil = 0;

// --- DYNAMIC THRESHOLDS ---
// Temperature (C)
int tempGoodMin = 18;
int tempGoodMax = 27;
int tempAvgMin = 10;
int tempAvgMax = 32;

// Humidity (%)
int humGoodMin = 40;
int humGoodMax = 70;
int humAvgMin = 30;
int humAvgMax = 80;

// Soil Moisture (%)
int soilGoodMin = 30;
int soilGoodMax = 85;
int soilAvgMin = 15;
int soilAvgMax = 90;

// Pump Trigger Threshold (%)
int pumpThreshold = 25; // <-- NEW: Configurable pump threshold

// --- SETTINGS MENU STATE ---
int currentSettingPage = 0; // 0-12 to cover all 13 parameters
bool inEditMode = false;
unsigned long lastButtonPress = 0; 

// --- GRAPH DATA ---
#define GRAPH_POINTS 100
uint8_t tempQueue[GRAPH_POINTS];
uint8_t humQueue[GRAPH_POINTS];
uint8_t soilQueue[GRAPH_POINTS];

unsigned long lastReadTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);     // Set pump pin as output
  digitalWrite(PUMP_PIN, HIGH);   // Make sure pump is off at boot

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_MID, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  
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
  // --- BUTTON LOGIC (For Settings Menu) ---
  if (currentScreen == 4 && millis() - lastButtonPress > 200) { 
    bool leftPressed = (digitalRead(BTN_LEFT) == LOW);
    bool midPressed = (digitalRead(BTN_MID) == LOW);
    bool rightPressed = (digitalRead(BTN_RIGHT) == LOW);

    if (midPressed) {
      inEditMode = !inEditMode; 
      lastButtonPress = millis();
      refreshScreen();
    } 
    else if (leftPressed) {
      if (inEditMode) {
        if (currentSettingPage == 0) tempGoodMin--;
        else if (currentSettingPage == 1) tempGoodMax--;
        else if (currentSettingPage == 2) tempAvgMin--;
        else if (currentSettingPage == 3) tempAvgMax--;
        else if (currentSettingPage == 4) humGoodMin--;
        else if (currentSettingPage == 5) humGoodMax--;
        else if (currentSettingPage == 6) humAvgMin--;
        else if (currentSettingPage == 7) humAvgMax--;
        else if (currentSettingPage == 8) soilGoodMin--;
        else if (currentSettingPage == 9) soilGoodMax--;
        else if (currentSettingPage == 10) soilAvgMin--;
        else if (currentSettingPage == 11) soilAvgMax--;
        else if (currentSettingPage == 12) pumpThreshold--; // <-- NEW
      } else {
        currentSettingPage--;
        if (currentSettingPage < 0) currentSettingPage = 12; // Wrap to end
      }
      lastButtonPress = millis();
      refreshScreen();
    }
    else if (rightPressed) {
      if (inEditMode) {
        if (currentSettingPage == 0) tempGoodMin++;
        else if (currentSettingPage == 1) tempGoodMax++;
        else if (currentSettingPage == 2) tempAvgMin++;
        else if (currentSettingPage == 3) tempAvgMax++;
        else if (currentSettingPage == 4) humGoodMin++;
        else if (currentSettingPage == 5) humGoodMax++;
        else if (currentSettingPage == 6) humAvgMin++;
        else if (currentSettingPage == 7) humAvgMax++;
        else if (currentSettingPage == 8) soilGoodMin++;
        else if (currentSettingPage == 9) soilGoodMax++;
        else if (currentSettingPage == 10) soilAvgMin++;
        else if (currentSettingPage == 11) soilAvgMax++;
        else if (currentSettingPage == 12) pumpThreshold++; // <-- NEW
      } else {
        currentSettingPage++;
        if (currentSettingPage > 12) currentSettingPage = 0; // Wrap to start
      }
      lastButtonPress = millis();
      refreshScreen();
    }
  }

  // --- GESTURE LOGIC ---
  uint8_t gesture = apds.readGesture();
  
  if (gesture == APDS9960_RIGHT) {
    if (currentScreen < 4) { 
      int nextScreen = currentScreen + 1;
      if (nextScreen > 3) nextScreen = 1; 
      animateScroll(currentScreen, nextScreen, 1);
      currentScreen = nextScreen;
      updateLED(); 
    }
  } 
  else if (gesture == APDS9960_LEFT) {
    if (currentScreen < 4) {
      int nextScreen = currentScreen - 1;
      if (nextScreen < 1) nextScreen = 3; 
      animateScroll(currentScreen, nextScreen, -1); 
      currentScreen = nextScreen;
      updateLED(); 
    }
  }
  else if (gesture == APDS9960_UP) {
    if (currentScreen != 4) {
      inEditMode = false; 
      animateScroll(currentScreen, 4, 1); 
      currentScreen = 4;
      updateLED();
    }
  }
  else if (gesture == APDS9960_DOWN) {
    if (currentScreen == 4) {
      inEditMode = false;
      animateScroll(4, 1, -1); 
      currentScreen = 1;
      updateLED();
    }
  }

  // --- READ SENSORS EVERY 1 SECOND ---
  if (millis() - lastReadTime >= 1000) {
    lastReadTime = millis();

    float newH = dht.readHumidity();
    float newT = dht.readTemperature();
    if (!isnan(newH)) currentH = newH;
    if (!isnan(newT)) currentT = newT;
    
    readSoilMoisture();

    // --- AUTOMATIC WATER PUMP LOGIC ---
    if (currentSoil < pumpThreshold) {
      digitalWrite(PUMP_PIN,LOW); // Turn pump on!
    } else {
      digitalWrite(PUMP_PIN, HIGH);  // Turn pump off!
    }

    for(int i = 0; i < GRAPH_POINTS - 1; i++) {
      tempQueue[i] = tempQueue[i+1];
      humQueue[i] = humQueue[i+1];
      soilQueue[i] = soilQueue[i+1];
    }
    
    tempQueue[GRAPH_POINTS - 1] = (uint8_t)currentT;
    humQueue[GRAPH_POINTS - 1] = (uint8_t)currentH;
    soilQueue[GRAPH_POINTS - 1] = (uint8_t)currentSoil;

    if (currentScreen != 4) {
      refreshScreen();
    }
    updateLED(); 
  }
}

void refreshScreen() {
  display.clearDisplay();
  drawFrame(currentScreen, 0, (currentScreen > 0 && currentScreen < 4));
  display.display();
}

void readSoilMoisture() {
  int sensorValue = analogRead(sensorPin);
  currentSoil = map(sensorValue, AirValue, WaterValue, 0, 100);
  if(currentSoil > 100) currentSoil = 100; 
  if(currentSoil < 0) currentSoil = 0; 
}

int checkStatus(int mode) {
  if (mode == 1) { // TEMP
    if (currentT >= tempGoodMin && currentT <= tempGoodMax) return 2; 
    if ((currentT >= tempAvgMin && currentT < tempGoodMin) || (currentT > tempGoodMax && currentT <= tempAvgMax)) return 1; 
    return 0; 
  } 
  else if (mode == 2) { // HUMIDITY
    if (currentH >= humGoodMin && currentH <= humGoodMax) return 2; 
    if ((currentH >= humAvgMin && currentH < humGoodMin) || (currentH > humGoodMax && currentH <= humAvgMax)) return 1; 
    return 0; 
  } 
  else if (mode == 3 || mode == 0) { // SOIL MOISTURE
    if (currentSoil >= soilGoodMin && currentSoil <= soilGoodMax) return 2; 
    if ((currentSoil >= soilAvgMin && currentSoil < soilGoodMin) || (currentSoil > soilGoodMax && currentSoil <= soilAvgMax)) return 1; 
    return 0; 
  }
  return 2; 
}

void updateLED() {
  if (currentScreen == 4) {
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 0);
    return;
  }

  int status = checkStatus(currentScreen == 0 ? 3 : currentScreen);

  if (status == 0) { 
    digitalWrite(redPin, LOW); 
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 85); 
  } 
  else if (status == 1) {  
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 0);
  } 
  else { 
    digitalWrite(redPin, HIGH);
    digitalWrite(greenPin, LOW);
    analogWrite(speakerPin, 0);
  }
}

void drawFace(int x, int y, int status) {
  display.drawCircle(x, y, 5, SSD1306_WHITE);
  display.drawPixel(x - 2, y - 1, SSD1306_WHITE);
  display.drawPixel(x + 2, y - 1, SSD1306_WHITE);
  
  if (status == 2) { 
    display.drawPixel(x - 2, y + 1, SSD1306_WHITE);
    display.drawPixel(x + 2, y + 1, SSD1306_WHITE);
    display.drawLine(x - 1, y + 2, x + 1, y + 2, SSD1306_WHITE);
  } 
  else if (status == 1) { 
    display.drawLine(x - 2, y + 2, x + 2, y + 2, SSD1306_WHITE);
  } 
  else { 
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

  // --- SETTINGS UI MULTI-PAGE ---
  if (mode == 4) {
    display.setTextSize(1);
    display.fillRect(x_offset, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    
    // Check which category we are in based on page number
    if (currentSettingPage < 4) {
      display.setCursor(x_offset + 15, 2);
      display.print(F("TEMP THRESHOLDS"));
    } else if (currentSettingPage < 8) {
      display.setCursor(x_offset + 15, 2);
      display.print(F("HUMIDITY THRESH"));
    } else if (currentSettingPage < 12) {
      display.setCursor(x_offset + 15, 2);
      display.print(F("SOIL THRESHOLDS"));
    } else {
      display.setCursor(x_offset + 15, 2);
      display.print(F("PUMP CONTROL")); // <-- NEW CATEGORY
    }
    
    display.setTextColor(SSD1306_WHITE);

    String settingName = "";
    int settingVal = 0;
    
    // Page logic for finding the right string and value
    int subPage = currentSettingPage % 4;
    
    // Only map the names if we aren't on the pump page
    if (currentSettingPage < 12) {
      if (subPage == 0) settingName = "Good Min (Green)";
      else if (subPage == 1) settingName = "Good Max (Green)";
      else if (subPage == 2) settingName = "Avg Min (Yellow)";
      else if (subPage == 3) settingName = "Avg Max (Yellow)";
    }

    if (currentSettingPage == 0) settingVal = tempGoodMin;
    else if (currentSettingPage == 1) settingVal = tempGoodMax;
    else if (currentSettingPage == 2) settingVal = tempAvgMin;
    else if (currentSettingPage == 3) settingVal = tempAvgMax;
    else if (currentSettingPage == 4) settingVal = humGoodMin;
    else if (currentSettingPage == 5) settingVal = humGoodMax;
    else if (currentSettingPage == 6) settingVal = humAvgMin;
    else if (currentSettingPage == 7) settingVal = humAvgMax;
    else if (currentSettingPage == 8) settingVal = soilGoodMin;
    else if (currentSettingPage == 9) settingVal = soilGoodMax;
    else if (currentSettingPage == 10) settingVal = soilAvgMin;
    else if (currentSettingPage == 11) settingVal = soilAvgMax;
    else if (currentSettingPage == 12) { // <-- NEW PUMP PAGE LOGIC
      settingName = "Pump Trigger %";
      settingVal = pumpThreshold;
    }

    display.setCursor(x_offset + 5, 25);
    display.print(settingName);

    if (inEditMode) {
      display.fillRect(x_offset + 48, 42, 32, 18, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    
    display.setTextSize(2);
    display.setCursor(x_offset + 52, 44);
    display.print(settingVal);
    display.setTextColor(SSD1306_WHITE); 
    return;
  }

  display.setCursor(x_offset, 0);
  display.setTextSize(1);
  if (mode == 1) {
    display.print(F("Temp: ")); display.print(currentT, 1); display.print(F(" C"));
  } else if (mode == 2) {
    display.print(F("Humidity: ")); display.print(currentH, 0); display.print(F(" %"));
  } else if (mode == 3) {
    display.print(F("Soil Moist: ")); display.print(currentSoil); display.print(F(" %"));
  }

  drawFace(x_offset + 115, 5, checkStatus(mode));

  if (!showGraph) return;

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