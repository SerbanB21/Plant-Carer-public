
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_APDS9960.h>
#include <DHT.h>
#include <Fonts/FreeSans9pt7b.h> 

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

const int greenPin = 14;    
const int redPin = 15;  
const int speakerPin = 13; 
#define PUMP_PIN 17 

// --- BUTTON PINS ---
#define BTN_LEFT 10
#define BTN_MID 11
#define BTN_RIGHT 12

// Global Variables
int currentScreen = 0; 
int prevActiveScreen = 1;
float currentT = 0;
float currentH = 0;
int currentSoil = 0;

// --- DYNAMIC THRESHOLDS ---
int tempGoodMin = 18, tempGoodMax = 27;
int tempAvgMin = 10,  tempAvgMax = 32;

int humGoodMin = 40, humGoodMax = 70;
int humAvgMin = 30,  humAvgMax = 80;

int soilGoodMin = 30, soilGoodMax = 85;
int soilAvgMin = 15,  soilAvgMax = 90;

int pumpThreshold = 25; 

// --- SETTINGS MENU STATE ---
int currentSettingPage = 0; 
bool inEditMode = false;
unsigned long lastInteractionTime = 0; 
unsigned long lastGestureTime = 0; 

// --- BUTTON STATE MACHINE ---
bool lastLeftState = false;
bool lastRightState = false;
bool lastMidState = false;
unsigned long midPressStart = 0;
bool midLongPressHandled = false;

// Variables for rapid continuous holding
unsigned long leftActionTime = 0;
unsigned long rightActionTime = 0;
const int HOLD_DELAY = 400; 
const int HOLD_RATE = 150;  

// --- GRAPH DATA ---
#define GRAPH_POINTS 88
uint8_t tempQueue[GRAPH_POINTS];
uint8_t humQueue[GRAPH_POINTS];
uint8_t soilQueue[GRAPH_POINTS];

unsigned long lastReadTime = 0;

void setup() {
  Serial.begin(115200);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);     
  digitalWrite(PUMP_PIN, HIGH);   

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
  display.setFont(&FreeSans9pt7b);
  display.setCursor(10, 30);
  display.print(F("Booting..."));
  display.display();
  display.setFont(); 

  delay(2000); 
  dht.begin();
  Wire.setSDA(4); Wire.setSCL(5); Wire.begin();

  if(apds.begin()){
    apds.enableProximity(true);
    apds.enableGesture(true);
  }

  readSensors();

  for(int i = 0; i < GRAPH_POINTS; i++) {
    tempQueue[i] = (uint8_t)currentT;   
    humQueue[i] = (uint8_t)currentH;
    soilQueue[i] = (uint8_t)currentSoil;
  }

  lastInteractionTime = millis();
  display.clearDisplay();
  drawFrame(0, 0, false); 
  display.display();
  updateLED(); 
}

void loop() {
  unsigned long now = millis();

  // --- SCREENSAVER LOGIC ---
  if (now - lastInteractionTime > 20000 && currentScreen != 5 && currentScreen != 0) {
    if (currentScreen != 4) prevActiveScreen = currentScreen;
    inEditMode = false;
    currentScreen = 5;
    refreshScreen();
    updateLED();
  }

  // --- BUTTON LOGIC ---
  bool leftState = (digitalRead(BTN_LEFT) == LOW);
  bool midState = (digitalRead(BTN_MID) == LOW);
  bool rightState = (digitalRead(BTN_RIGHT) == LOW);

  if (midState && !lastMidState) {
    midPressStart = now;
    midLongPressHandled = false;
    wakeUp(); 
  }
  
  if (midState && !midLongPressHandled && (now - midPressStart >= 2000)) {
    midLongPressHandled = true;
    display.invertDisplay(true); delay(30); display.invertDisplay(false);

    if (currentScreen == 4) {
      inEditMode = false;
      currentScreen = prevActiveScreen;
    } else {
      prevActiveScreen = currentScreen;
      inEditMode = false;
      currentScreen = 4;
    }
    refreshScreen();
    updateLED();
  }

  if (!midState && lastMidState) {
    if (!midLongPressHandled && (now - midPressStart >= 50)) {
      if (currentScreen == 4) {
        display.invertDisplay(true); delay(30); display.invertDisplay(false);
        inEditMode = !inEditMode;
        refreshScreen();
      }
    }
  }
  lastMidState = midState;

  if (leftState) {
    if (!lastLeftState) { 
      leftActionTime = now + HOLD_DELAY; 
      handleLeftPress();
    } else if (now >= leftActionTime) {
      leftActionTime = now + HOLD_RATE; 
      handleLeftPress();
    }
  }
  lastLeftState = leftState;

  if (rightState) {
    if (!lastRightState) { 
      rightActionTime = now + HOLD_DELAY; 
      handleRightPress();
    } else if (now >= rightActionTime) {
      rightActionTime = now + HOLD_RATE; 
      handleRightPress();
    }
  }
  lastRightState = rightState;

  // --- GESTURE LOGIC ---
  if (apds.gestureValid()) {
    uint8_t gesture = apds.readGesture(); 
    
    if (now - lastGestureTime > 800 && currentScreen != 4) { 
      if (gesture == APDS9960_RIGHT || gesture == APDS9960_LEFT) {
        wakeUp();
        lastGestureTime = now; 
        
        if (gesture == APDS9960_RIGHT) {
          if (currentScreen < 4 && currentScreen != 0) { 
            int nextScreen = currentScreen + 1;
            if (nextScreen > 3) nextScreen = 1; 
            animateScroll(currentScreen, nextScreen, 1);
            currentScreen = nextScreen; 
            refreshScreen(); 
            updateLED(); 
          } else if (currentScreen == 0) {
            animateScroll(0, 1, 1);
            currentScreen = 1;
            refreshScreen();
            updateLED();
          }
        } 
        else if (gesture == APDS9960_LEFT) {
          if (currentScreen < 4 && currentScreen != 0) {
            int nextScreen = currentScreen - 1;
            if (nextScreen < 1) nextScreen = 3; 
            animateScroll(currentScreen, nextScreen, -1); 
            currentScreen = nextScreen; 
            refreshScreen(); 
            updateLED(); 
          }
        }
      }
    }
  }

  // --- SENSOR READING ---
  if (now - lastReadTime >= 1000) {
    lastReadTime = now;
    readSensors();

    if (currentSoil < pumpThreshold) digitalWrite(PUMP_PIN, LOW); 
    else digitalWrite(PUMP_PIN, HIGH); 

    for(int i = 0; i < GRAPH_POINTS - 1; i++) {
      tempQueue[i] = tempQueue[i+1];
      humQueue[i] = humQueue[i+1];
      soilQueue[i] = soilQueue[i+1];
    }
    tempQueue[GRAPH_POINTS - 1] = (uint8_t)currentT;
    humQueue[GRAPH_POINTS - 1] = (uint8_t)currentH;
    soilQueue[GRAPH_POINTS - 1] = (uint8_t)currentSoil;

    if (currentScreen != 4 && currentScreen != 0) refreshScreen();
    updateLED(); 
  }
}

// --- BUTTON HELPER FUNCTIONS ---
void handleLeftPress() {
  wakeUp();
  if (currentScreen == 4) {
    display.invertDisplay(true); delay(15); display.invertDisplay(false);
    if (inEditMode) adjustSetting(-1);
    else { currentSettingPage--; if (currentSettingPage < 0) currentSettingPage = 12; }
    refreshScreen();
  }
}

void handleRightPress() {
  wakeUp();
  if (currentScreen == 4) {
    display.invertDisplay(true); delay(15); display.invertDisplay(false);
    if (inEditMode) adjustSetting(1);
    else { currentSettingPage++; if (currentSettingPage > 12) currentSettingPage = 0; }
    refreshScreen();
  }
}

// --- SYSTEM FUNCTIONS ---
void wakeUp() {
  lastInteractionTime = millis();
  if (currentScreen == 5) {
    currentScreen = prevActiveScreen; 
    refreshScreen();
  }
}

void readSensors() {
  float newH = dht.readHumidity();
  float newT = dht.readTemperature();
  if (!isnan(newH)) currentH = newH;
  if (!isnan(newT)) currentT = newT;
  
  int sensorValue = analogRead(sensorPin);
  currentSoil = map(sensorValue, AirValue, WaterValue, 0, 100);
  currentSoil = constrain(currentSoil, 0, 100);
}

bool trySet(int &var, int delta, int minVal, int maxVal) {
  int test = var + delta;
  if (test < minVal || test > maxVal) return false;
  var = test;
  return true;
}

void shakeError() {
  for (int i = 0; i < 4; i++) {
    int x_off = (i % 2 == 0) ? 4 : -4;
    display.clearDisplay();
    drawFrame(4, x_off, false);
    display.display();
    delay(40);
  }
}

void adjustSetting(int d) {
  bool ok = true;
  switch (currentSettingPage) {
    case 0: ok = trySet(tempGoodMin, d, tempAvgMin + 1, tempGoodMax - 1); break;
    case 1: ok = trySet(tempGoodMax, d, tempGoodMin + 1, tempAvgMax - 1); break;
    case 2: ok = trySet(tempAvgMin, d, 0, tempGoodMin - 1); break;
    case 3: ok = trySet(tempAvgMax, d, tempGoodMax + 1, 99); break;
    case 4: ok = trySet(humGoodMin, d, humAvgMin + 1, humGoodMax - 1); break;
    case 5: ok = trySet(humGoodMax, d, humGoodMin + 1, humAvgMax - 1); break;
    case 6: ok = trySet(humAvgMin, d, 0, humGoodMin - 1); break;
    case 7: ok = trySet(humAvgMax, d, humGoodMax + 1, 99); break;
    case 8: ok = trySet(soilGoodMin, d, soilAvgMin + 1, soilGoodMax - 1); break;
    case 9: ok = trySet(soilGoodMax, d, soilGoodMin + 1, soilAvgMax - 1); break;
    case 10: ok = trySet(soilAvgMin, d, 0, soilGoodMin - 1); break;
    case 11: ok = trySet(soilAvgMax, d, soilGoodMax + 1, 100); break;
    case 12: ok = trySet(pumpThreshold, d, 0, 100); break;
  }
  if (!ok) shakeError();
}

void refreshScreen() {
  display.clearDisplay();
  drawFrame(currentScreen, 0, (currentScreen > 0 && currentScreen < 4));
  display.display();
}

int checkStatus(int mode) {
  if (mode == 1) { 
    if (currentT >= tempGoodMin && currentT <= tempGoodMax) return 2; 
    if ((currentT >= tempAvgMin && currentT < tempGoodMin) || (currentT > tempGoodMax && currentT <= tempAvgMax)) return 1; 
    return 0; 
  } else if (mode == 2) { 
    if (currentH >= humGoodMin && currentH <= humGoodMax) return 2; 
    if ((currentH >= humAvgMin && currentH < humGoodMin) || (currentH > humGoodMax && currentH <= humAvgMax)) return 1; 
    return 0; 
  } else if (mode == 3 || mode == 0) { 
    if (currentSoil >= soilGoodMin && currentSoil <= soilGoodMax) return 2; 
    if ((currentSoil >= soilAvgMin && currentSoil < soilGoodMin) || (currentSoil > soilGoodMax && currentSoil <= soilAvgMax)) return 1; 
    return 0; 
  }
  return 2; 
}

// --- UPDATED: Score maps strictly to 0.0 at worst and 1.0 at best
float getScore(float val, float aMin, float gMin, float gMax, float aMax) {
  if (val >= gMin && val <= gMax) return 1.0;          // Perfect (Outer Ring)
  if (val <= aMin || val >= aMax) return 0.0;          // Worst (Center Point)
  if (val < gMin) return ((val - aMin) / (gMin - aMin)); // Scaling up to good
  if (val > gMax) return ((aMax - val) / (aMax - gMax)); // Scaling down from good
  return 0.0; 
}

// --- DYNAMIC LED LOGIC ---
void updateLED() {
  // 1. Settings Menu: Turn Off
  if (currentScreen == 4) {
    digitalWrite(redPin, LOW);
    digitalWrite(greenPin, LOW);
    analogWrite(speakerPin, 0); 
    return;
  }

  // 2. Screensaver & Intro: General Score Logic
  if (currentScreen == 5 || currentScreen == 0) {
    float sT = getScore(currentT, tempAvgMin, tempGoodMin, tempGoodMax, tempAvgMax);
    float sH = getScore(currentH, humAvgMin, humGoodMin, humGoodMax, humAvgMax);
    float sS = getScore(currentSoil, soilAvgMin, soilGoodMin, soilGoodMax, soilAvgMax);
    
    int health = (int)(((sT + sH + sS) / 3.0) * 100.0);

    if (health > 60) {
      // Green (Optimal Score)
      digitalWrite(redPin, HIGH); 
      digitalWrite(greenPin, LOW);
      analogWrite(speakerPin, 0);
    } 
    else if (health > 50) {
      // Yellow (Neutral Score)
      digitalWrite(redPin, HIGH);
      digitalWrite(greenPin, HIGH);
      analogWrite(speakerPin, 0);
    } 
    else {
      // Red (Bad Score)
      digitalWrite(redPin, LOW); 
      digitalWrite(greenPin, HIGH);
      analogWrite(speakerPin, 85); 
    }
    return;
  }

  // 3. Individual Screens: Link directly to Face status
  int status = checkStatus(currentScreen);
  
  if (status == 2) { 
    // Green (Happy Face)
    digitalWrite(redPin, HIGH); 
    digitalWrite(greenPin, LOW);
    analogWrite(speakerPin, 0); 
  } 
  else if (status == 1) {  
    // Yellow (Neutral Face)
    digitalWrite(redPin, HIGH); 
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 0);
  } 
  else { 
    // Red (Sad Face)
    digitalWrite(redPin, LOW); 
    digitalWrite(greenPin, HIGH);
    analogWrite(speakerPin, 85); 
  }
}

void drawDottedLine(int x1, int x2, int y) {
  for (int i = x1; i <= x2; i += 4) {
    display.drawPixel(i, y, SSD1306_WHITE);
  }
}

void drawFace(int x, int y, int status, bool inverted) {
  int color = inverted ? SSD1306_BLACK : SSD1306_WHITE;
  int bg = inverted ? SSD1306_WHITE : SSD1306_BLACK;
  
  if (inverted) display.fillCircle(x, y, 6, bg); 
  display.drawCircle(x, y, 5, color);
  
  if (status == 2) { 
    bool blinking = (millis() % 2500 < 150); 
    if (blinking) { 
      display.drawLine(x - 3, y - 1, x - 1, y - 1, color);
      display.drawLine(x + 1, y - 1, x + 3, y - 1, color);
    } else {
      display.drawPixel(x - 2, y - 1, color);
      display.drawPixel(x + 2, y - 1, color);
    }
    display.drawPixel(x - 2, y + 1, color);
    display.drawPixel(x + 2, y + 1, color);
    display.drawLine(x - 1, y + 2, x + 1, y + 2, color);
  } 
  else if (status == 1) { 
    bool lookSide = (millis() % 3000 < 400); 
    if (lookSide) {
      display.drawPixel(x - 3, y - 1, color); 
      display.drawPixel(x + 1, y - 1, color);
    } else {
      display.drawPixel(x - 2, y - 1, color);
      display.drawPixel(x + 2, y - 1, color);
    }
    display.drawLine(x - 2, y + 2, x + 2, y + 2, color);
  } 
  else { 
    bool sadAnim = (millis() % 2000 < 500); 
    if (sadAnim) {
      display.drawLine(x - 3, y - 2, x - 1, y, color);
      display.drawLine(x - 3, y, x - 1, y - 2, color);
      display.drawLine(x + 1, y - 2, x + 3, y, color);
      display.drawLine(x + 1, y, x + 3, y - 2, color);
    } else {
      display.drawPixel(x - 2, y - 1, color);
      display.drawPixel(x + 2, y - 1, color);
    }
    display.drawPixel(x - 2, y + 2, color);
    display.drawPixel(x + 2, y + 2, color);
    display.drawLine(x - 1, y + 1, x + 1, y + 1, color);
  }
}

void drawFrame(int mode, int xo, bool showGraph) {
  display.setTextColor(SSD1306_WHITE);
  
  if (mode == 0) {
    display.setFont(&FreeSans9pt7b); 
    display.setTextSize(1);
    display.setCursor(xo + 15, 25); display.print(F("PLANT"));
    display.setCursor(xo + 15, 45); display.print(F("MONITOR"));
    display.setFont(); 
    display.setCursor(xo + 10, 54); display.print(F("SWIPE > TO UNLOCK"));
    return; 
  }

  // --- SCREENSAVER (Classic Spider/Radar Graph) ---
  if (mode == 5) { 
    float sT = getScore(currentT, tempAvgMin, tempGoodMin, tempGoodMax, tempAvgMax);
    float sH = getScore(currentH, humAvgMin, humGoodMin, humGoodMax, humAvgMax);
    float sS = getScore(currentSoil, soilAvgMin, soilGoodMin, soilGoodMax, soilAvgMax);
    
    int health = (int)(((sT + sH + sS) / 3.0) * 100.0);

    int cx = xo + 35, cy = 36, maxR = 26; 
    
    // 1. Draw Concentric Boundaries (The Grid/Web)
    for (int i = 1; i <= 3; i++) {
      int r = (maxR * i) / 3;
      int pt1X = cx;                     int pt1Y = cy - r;
      int pt2X = cx + (int)(0.866 * r);  int pt2Y = cy + (int)(0.5 * r);
      int pt3X = cx - (int)(0.866 * r);  int pt3Y = cy + (int)(0.5 * r);
      
      display.drawLine(pt1X, pt1Y, pt2X, pt2Y, SSD1306_WHITE);
      display.drawLine(pt2X, pt2Y, pt3X, pt3Y, SSD1306_WHITE);
      display.drawLine(pt3X, pt3Y, pt1X, pt1Y, SSD1306_WHITE);
    }

    // 2. Draw 3 Main Axes
    display.drawLine(cx, cy, cx, cy - maxR, SSD1306_WHITE); 
    display.drawLine(cx, cy, cx + (int)(0.866 * maxR), cy + (int)(0.5 * maxR), SSD1306_WHITE); 
    display.drawLine(cx, cy, cx - (int)(0.866 * maxR), cy + (int)(0.5 * maxR), SSD1306_WHITE); 

    // 3. Labels
    display.setTextSize(1);
    display.setCursor(cx - 2, cy - maxR - 8); display.print(F("T"));
    display.setCursor(cx + (int)(0.866 * maxR) + 3, cy + (int)(0.5 * maxR) - 4); display.print(F("H"));
    display.setCursor(cx - (int)(0.866 * maxR) - 8, cy + (int)(0.5 * maxR) - 4); display.print(F("M"));

    // 4. Calculate Data Points (Will collapse to center (cx, cy) if score is 0.0)
    int pxT = cx; 
    int pyT = cy - (sT * maxR);
    int pxH = cx + (int)(0.866 * sH * maxR); 
    int pyH = cy + (int)(0.5 * sH * maxR);
    int pxS = cx - (int)(0.866 * sS * maxR); 
    int pyS = cy + (int)(0.5 * sS * maxR);

    // 5. Draw Dynamic Data Polygon
    display.drawLine(pxT, pyT, pxH, pyH, SSD1306_WHITE);
    display.drawLine(pxH, pyH, pxS, pyS, SSD1306_WHITE);
    display.drawLine(pxS, pyS, pxT, pyT, SSD1306_WHITE);

    // 6. Draw Vertices Markers (Dots)
    display.fillCircle(pxT, pyT, 2, SSD1306_WHITE);
    display.fillCircle(pxH, pyH, 2, SSD1306_WHITE);
    display.fillCircle(pxS, pyS, 2, SSD1306_WHITE);

    // Sidebar Score Output
    display.setCursor(xo + 75, 24); display.print(F("SCORE"));
    display.setTextSize(2);
    display.setCursor(xo + 75, 38); 
    display.print(health); display.print(F("%"));
    return;
  }

  // SETTINGS MENU
  if (mode == 4) {
    display.setTextSize(1);
    display.fillRect(xo, 0, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    
    if (currentSettingPage < 4) { display.setCursor(xo + 15, 2); display.print(F("TEMP THRESHOLDS")); } 
    else if (currentSettingPage < 8) { display.setCursor(xo + 15, 2); display.print(F("HUMIDITY THRESH")); } 
    else if (currentSettingPage < 12) { display.setCursor(xo + 15, 2); display.print(F("SOIL THRESHOLDS")); } 
    else { display.setCursor(xo + 15, 2); display.print(F("PUMP CONTROL")); }
    
    display.setTextColor(SSD1306_WHITE);
    String settingName = ""; int settingVal = 0;
    
    if (currentSettingPage < 12) {
      int subPage = currentSettingPage % 4;
      if (subPage == 0) settingName = "Good Min (Green)";
      else if (subPage == 1) settingName = "Good Max (Green)";
      else if (subPage == 2) settingName = "Avg Min (Yellow)";
      else if (subPage == 3) settingName = "Avg Max (Yellow)";
    }

    switch (currentSettingPage) {
      case 0: settingVal = tempGoodMin; break; case 1: settingVal = tempGoodMax; break;
      case 2: settingVal = tempAvgMin; break;  case 3: settingVal = tempAvgMax; break;
      case 4: settingVal = humGoodMin; break;  case 5: settingVal = humGoodMax; break;
      case 6: settingVal = humAvgMin; break;   case 7: settingVal = humAvgMax; break;
      case 8: settingVal = soilGoodMin; break; case 9: settingVal = soilGoodMax; break;
      case 10: settingVal = soilAvgMin; break; case 11: settingVal = soilAvgMax; break;
      case 12: settingName = "Pump Trigger %"; settingVal = pumpThreshold; break;
    }

    display.setCursor(xo + 5, 22); display.print(settingName);

    if (inEditMode) {
      display.fillRect(xo + 42, 32, 50, 18, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    }
    
    display.setTextSize(2);
    display.setCursor(xo + 46, 34); display.print(settingVal);
    display.setTextColor(SSD1306_WHITE); 

    int thumbY = 14 + (currentSettingPage * 32 / 12); 
    display.drawLine(xo + 126, 14, xo + 126, 50, SSD1306_WHITE); 
    display.fillRect(xo + 124, thumbY, 5, 5, SSD1306_WHITE); 

    display.drawLine(xo, 52, xo + 128, 52, SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(xo + 4, 55);
    if (inEditMode) display.print(F("- DOWN   SAVE   UP +"));
    else display.print(F("< PREV   HOLD   NEXT >"));
    
    return;
  }

  // --- SPLIT-PANE DATA UI ---
  display.fillRect(xo, 0, 128, 13, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(xo + 4, 3);
  
  if (mode == 1) display.print(F("TEMPERATURE"));
  else if (mode == 2) display.print(F("HUMIDITY"));
  else if (mode == 3) display.print(F("SOIL MOISTURE"));

  drawFace(xo + 118, 6, checkStatus(mode), true); 
  display.setTextColor(SSD1306_WHITE);

  if (mode == 1) {
    display.setTextSize(2); display.setCursor(xo + 2, 24); display.print((int)currentT);
    display.setTextSize(1); 
    display.drawCircle(xo + 4, 45, 1, SSD1306_WHITE); 
    display.setCursor(xo + 8, 44); display.print(F("C")); 
  } else if (mode == 2) {
    display.setTextSize(2); display.setCursor(xo + 2, 24); display.print((int)currentH);
    display.setTextSize(1); display.setCursor(xo + 2, 44); display.print(F("%"));
  } else if (mode == 3) {
    display.setTextSize(2); display.setCursor(xo + 2, 24); display.print(currentSoil);
    display.setTextSize(1); display.setCursor(xo + 2, 44); display.print(F("%"));
    if (currentSoil < pumpThreshold) {
      int dropY = (millis() / 50) % 15; 
      display.drawPixel(xo + 24, 40 + dropY, SSD1306_WHITE);
      display.drawPixel(xo + 20, 35 + ((dropY+5)%15), SSD1306_WHITE);
    }
  } 

  if (!showGraph) return;

  int graphX = xo + 38;
  int graphW = GRAPH_POINTS; 
  display.drawLine(graphX, 14, graphX, 63, SSD1306_WHITE); 
  display.drawLine(graphX, 63, graphX + graphW, 63, SSD1306_WHITE); 

  int yMinGood, yMaxGood;
  if (mode == 1) { yMaxGood = map(tempGoodMax, 0, 40, 62, 14); yMinGood = map(tempGoodMin, 0, 40, 62, 14); }
  else if (mode == 2) { yMaxGood = map(humGoodMax, 0, 100, 62, 14); yMinGood = map(humGoodMin, 0, 100, 62, 14); }
  else if (mode == 3) { yMaxGood = map(soilGoodMax, 0, 100, 62, 14); yMinGood = map(soilGoodMin, 0, 100, 62, 14); }

  yMaxGood = constrain(yMaxGood, 14, 62);
  yMinGood = constrain(yMinGood, 14, 62);

  drawDottedLine(graphX + 2, graphX + graphW, yMaxGood);
  drawDottedLine(graphX + 2, graphX + graphW, yMinGood);

  for (int i = 0; i < GRAPH_POINTS - 1; i++) {
    int y1, y2;
    if (mode == 1) { y1 = map(tempQueue[i], 0, 40, 62, 14); y2 = map(tempQueue[i+1], 0, 40, 62, 14); } 
    else if (mode == 2) { y1 = map(humQueue[i], 0, 100, 62, 14); y2 = map(humQueue[i+1], 0, 100, 62, 14); } 
    else if (mode == 3) { y1 = map(soilQueue[i], 0, 100, 62, 14); y2 = map(soilQueue[i+1], 0, 100, 62, 14); } 

    y1 = constrain(y1, 14, 62); y2 = constrain(y2, 14, 62);
    display.drawLine(graphX + i + 1, y1, graphX + (i + 2), y2, SSD1306_WHITE);
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
}
