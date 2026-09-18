# Raspberry PI Pico 2W plant monitor
## I have a private repository with more information such as reports if needed
[Watch the Demonstration Video](<WhatsApp Video 2026-04-11 at 17.52.10 (1).mp4>)
## Group project| Group 3| Circuits and Systems Design| 2025-2026| TCD Dublin
 Automated Plant watering system based on a Pico 2W with Moisture, Humidity, Temperature, Gesture sensors with a self-triggering pump that waters the plant when the humidity, temperature, 
 or moisture sensors sit outside certain user-adjustable thresholds.

# Features
Real-time environmental monitoring (temperature, humidity, soil moisture).
Gesture-controlled screen navigation using APDS-9960.
Visual and Audio alerts based on plant condition

# Hardware Components
Microcontroller: Raspberry Pi Pico 2W
Sensors: DHT11 Temperature/Humidity Sensor, APDS-9960 Gesture/Proximity Sensor, Capacitive Soil Moisture Sensor
Display & Output:OLED Display (3.3 to 5V DC operating voltage, with 3.3V logic voltage from fdata pins), Tri-Color LED, Speaker
Other Hardware: Breadboard, jumper wires, USB cable, Adafruit DRV8871 Pump Driver (Motor Supply Voltage 6.5V to 45V DC supplied by 9V battery), 
                Water Pump (Rated Voltage up to 5V DC), LM386N-1 Audio Amplifier (Operating supply voltage 4V to 12V DC with input logic threshold triggered by Pico 3.3V PWM tone() output)
                4 Buttons

# Potential Applications
Plant monitoring, agriculture, monitoring concrete curing, climate control, food storage, room leak detection



# Software & Libraries:
Adafruit_APDS9960
Adafruit_GFX, Adafruit_SSD1306
DHT sensor library
