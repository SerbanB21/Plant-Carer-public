#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Adafruit_APDS9960.h>

// --- CONFIGURATION ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

Adafruit_APDS9960 apds;

#define SOIL_MOISTURE_PIN A0
#define DHT_PIN 2
#define DHT_TYPE DHT11

#define BUZZER_PIN 10
#define RED_LED 5
#define GREEN_LED 6
#define BLUE_LED 7

DHT dht(DHT_PIN, DHT_TYPE);

// Variables
int displayMode = 0; // 0=Temp, 1=Moisture, 2=Condition
unsigned long lastUpdate = 0;
String plantCondition = "Checking...";
float temp = 0;
float hum = 0;
int moisturePercent = 0;

void setup() {
    Serial.begin(115200);
    dht.begin();

    pinMode(SOIL_MOISTURE_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);
    pinMode(BLUE_LED, OUTPUT);
    
    digitalWrite(RED_LED, HIGH);
    digitalWrite(GREEN_LED, HIGH);
    digitalWrite(BLUE_LED, HIGH);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED fail");
        while (1);
    }
    
    if(!apds.begin()){
        Serial.println("Gesture fail");
        while(1);
    }
    apds.enableProximity(true);
    apds.enableGesture(true);

    // Initial Draw
    drawFrame(0, 0); 
    display.display();
}

// --- HELPER: Set LED Color ---
void setColor(bool red, bool green, bool blue) {
    digitalWrite(RED_LED, red ? LOW : HIGH);
    digitalWrite(GREEN_LED, green ? LOW : HIGH);
    digitalWrite(BLUE_LED, blue ? LOW : HIGH);
}

// --- CORE ANIMATION FUNCTION ---
// This function draws the content of a specific page at a specific X offset
void drawFrame(int mode, int x_offset) {
    
    // PAGE 0: TEMP
    if (mode == 0) {
        display.setCursor(x_offset, 0);
        display.setTextSize(1);
        display.println("Temp & Humidity");
        display.drawLine(x_offset, 10, x_offset + 128, 10, WHITE);

        display.setCursor(x_offset + 10, 20);
        display.setTextSize(2);
        display.print(temp, 1); display.print("C");

        display.setCursor(x_offset + 10, 45);
        display.setTextSize(1);
        display.print("Hum: "); display.print(hum, 0); display.print("%");
    }
    // PAGE 1: MOISTURE
    else if (mode == 1) {
        display.setCursor(x_offset, 0);
        display.setTextSize(1);
        display.println("Soil Moisture");
        display.drawLine(x_offset, 10, x_offset + 128, 10, WHITE);

        display.setCursor(x_offset + 10, 20);
        display.setTextSize(2);
        display.print(moisturePercent); display.print("%");

        display.setCursor(x_offset + 10, 45);
        display.setTextSize(1);
        if(moisturePercent < 30) display.print("NEEDS WATER!");
        else display.print("Soil is wet");
    }
    // PAGE 2: CONDITION
    else if (mode == 2) {
        display.setCursor(x_offset, 0);
        display.setTextSize(1);
        display.println("Overall Health");
        display.drawLine(x_offset, 10, x_offset + 128, 10, WHITE);

        display.setCursor(x_offset + 5, 25);
        display.setTextSize(2);
        display.println(plantCondition);

        int barWidth = (plantCondition == "OPTIMAL") ? 128 : (plantCondition == "AVERAGE" ? 64 : 20);
        display.fillRect(x_offset, 50, barWidth, 8, WHITE);
    }
}

// --- TRANSITION ANIMATION ---
// Moves from 'prevMode' to 'nextMode' direction (1 = slide left, -1 = slide right)
void animateTransition(int prevMode, int nextMode, int direction) {
    int step = 16; // Speed of scroll (higher = faster but choppier)
    
    for (int i = 0; i <= 128; i += step) {
        display.clearDisplay();
        
        if (direction == 1) { // SCROLL LEFT (Old moves left, New comes from right)
            drawFrame(prevMode, -i);         // Move old out to -128
            drawFrame(nextMode, 128 - i);    // Move new in from 128
        } 
        else { // SCROLL RIGHT (Old moves right, New comes from left)
            drawFrame(prevMode, i);          // Move old out to 128
            drawFrame(nextMode, -128 + i);   // Move new in from -128
        }
        
        display.display();
    }
}

void loop() {
    // 1. CHECK GESTURES
    uint8_t gesture = apds.readGesture();
    
    if(gesture == APDS9960_LEFT) {
        int nextMode = displayMode + 1;
        if (nextMode > 2) nextMode = 0;
        
        // Trigger Animation: Slide LEFT
        animateTransition(displayMode, nextMode, 1);
        
        displayMode = nextMode; // Save new state
    }
    else if(gesture == APDS9960_RIGHT) {
        int nextMode = displayMode - 1;
        if (nextMode < 0) nextMode = 2;
        
        // Trigger Animation: Slide RIGHT
        animateTransition(displayMode, nextMode, -1);
        
        displayMode = nextMode; // Save new state
    }

    // 2. CHECK SENSORS (Every 2 seconds)
    if (millis() - lastUpdate > 2000) {
        lastUpdate = millis();

        int rawMoisture = analogRead(SOIL_MOISTURE_PIN);
        moisturePercent = map(rawMoisture, 1023, 300, 0, 100);
        moisturePercent = constrain(moisturePercent, 0, 100);

        temp = dht.readTemperature();
        hum = dht.readHumidity();
        if (isnan(temp)) temp = 0;

        // Condition Logic
        if (moisturePercent < 30 || temp > 32.0 || temp < 10.0) {
            plantCondition = "CRITICAL";
            setColor(1, 0, 0);
        } else if (moisturePercent < 50) {
            plantCondition = "AVERAGE";
            setColor(0, 0, 1);
        } else {
            plantCondition = "OPTIMAL";
            setColor(0, 1, 0);
        }
        
        // Redraw current frame (no animation) to update numbers
        display.clearDisplay();
        drawFrame(displayMode, 0);
        display.display();
    }
}