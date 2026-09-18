#include <DHT.h>

#define DHTPIN 4     // Digital pin connected to the DHT sensor
#define DHTTYPE DHT11   // DHT 11

DHT dht(DHTPIN, DHTTYPE);

const int sensorPin = A0;

// YOUR CALIBRATION VALUES
const int AirValue = 500;   
const int WaterValue = 260; 

int sensorValue = 0;
int soilMoisturePercent = 0;

void setup() {
  Serial.begin(9600);
  Serial.println(F("DHTxx test!"));
  dht.begin();
}

void loop() {
  // Wait a few seconds between measurements. 
  // DHT11 is slow, so 2000ms is recommended to avoid unstable readings.
  delay(2000);

  float h = dht.readHumidity();
  float t = dht.readTemperature();

  // --- ERROR CHECK START ---
  // Check if any reads failed.
  if (isnan(h) || isnan(t)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return; // Try again next loop
  }
  // --- ERROR CHECK END --- 
  // (We closed the bracket above, so the code below runs if the DHT is OK)

  // --- SOIL MOISTURE LOGIC ---
  sensorValue = analogRead(sensorPin);
  
  // Map the raw value to a percentage
  soilMoisturePercent = map(sensorValue, AirValue, WaterValue, 0, 100);

  // Constraints
  if(soilMoisturePercent > 100) { soilMoisturePercent = 100; }
  if(soilMoisturePercent < 0) { soilMoisturePercent = 0; }

  // --- PRINTING ---
  Serial.print("Soil Raw: ");
  Serial.print(sensorValue);
  Serial.print(" | Soil: ");
  Serial.print(soilMoisturePercent);
  Serial.print("% | ");
  
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("% | Temp: "));
  Serial.print(t);
  Serial.println(F("°C "));
}