#include <vector>
#include <Arduino.h>
#include <SD.h>

extern volatile int pulseCount;
extern volatile unsigned long lastPulseTime;

std::vector<String> getAvailableModesFromSD() {
  std::vector<String> modes;
   
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open SD card directory");
    return modes;
  }

  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;

    if (!entry.isDirectory()) {
      String fileName = entry.name();
      if (fileName.endsWith(".txt")) {
        String modeName = fileName.substring(0, fileName.length() - 4);
        modes.push_back(modeName);
      }
    }
    entry.close();
  }
  root.close();

  if (modes.size() == 0) {
    Serial.println("No modes available on SD card");
    modes.push_back("Otomatis");
    modes.push_back("Manual");
  }

  Serial.println("Available modes:");
  for (const String& mode : modes) {
    Serial.println("- " + mode);
  }

  return modes;
}

void IRAM_ATTR pulseCounter() {
  unsigned long currentTime = micros();
  if (currentTime - lastPulseTime > 1000) {
    pulseCount++;
    lastPulseTime = currentTime;
  }
}

void initializeSDCard() {
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
  }
  Serial.println("SD card initialized.");
}

void pump_setup() {
  Serial.println("Initializing pump control system...");
  
  // Initialize I2C with ESP32 default pins
  Wire.begin(21, 22); // SDA, SCL
  
  // Initialize ADS1115
  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS1115!");
    while (1);
  }
  
  // Set gain to 1 for ±4.096V range (optimal for 0-3.3V signal)
  ads.setGain(GAIN_ONE);
  
  // Set data rate to 860 SPS (fastest)
  ads.setDataRate(RATE_ADS1115_860SPS);
  
  Serial.println("ADS1115 initialized successfully (Gain=1, ±4.096V)");
  Serial.print("Voltage Divider Ratio: "); Serial.println(VD_RATIO);
  Serial.print("Expected voltage range: "); Serial.print(VOUT_MIN); 
  Serial.print("V - "); Serial.print(VOUT_MAX); Serial.println("V");
  
  // Setup PWM
  ledcAttach(PUMP_PWM_PIN, PWM_FREQ, PWM_RESOLUTION);
  ledcWrite(PUMP_PWM_PIN, 0); // Start with pump OFF (0 = off)
  cycleStartTime = millis();

  // Initialize Kalman filter with first reading
  delay(100);
  float initialPressure = 0;
  for(int i = 0; i < 10; i++) {
    initialPressure += readPressureFromADS();
    delay(10);
  }
  x = initialPressure / 10.0;
  
  Serial.println("Pump control system initialized");
  Serial.print("Initial pressure: "); Serial.print(x); Serial.println(" kPa");
}