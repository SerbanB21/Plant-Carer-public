// --- UNO R4 SOIL SENSOR TEST SCRIPT ---

const int sensorPin = A0; 

void setup() {
  // Uno R4 handles high baud rates easily, but make sure your Serial Monitor matches!
  Serial.begin(9600);
  
  // Wait a moment for the Serial Monitor to open
  delay(2000); 
  Serial.println("Starting Uno R4 Sensor Test...");
}

void loop() {
  int sensorValue = analogRead(sensorPin);
  
  Serial.print("Raw Sensor Value: ");
  Serial.println(sensorValue);
  
  delay(500); // Read twice a second
}