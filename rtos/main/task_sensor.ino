#include "tasks.h"

extern Adafruit_ADS1115 ads;
extern float SCALE;
extern PumpParameters pumpParams;
extern float flowRateArray[5];
extern int flowIndex;
extern volatile int pulseCount;
extern unsigned long lastFlowTime;
extern float calibrationFactor;
extern float generateBloodPressureWaveform();

float readPressureFromADS();
void updateFlowRate();

void TaskSensor(void *pv) {
  for (;;) {
    updateFlowRate();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
float readPressureFromADS() {
  // Read from ADS1115 channel A0
  int16_t adcValue = ads.readADC_SingleEnded(ADS_CHANNEL);
  
  // Convert to voltage
  // ADS1115 with gain 1 has range of ±4.096V
  // Resolution is 4.096V / 32768 = 0.125mV per bit
  float voltage = adcValue * 0.000125; // Convert to volts
  
  // Calculate pressure from voltage
  float pressure = (SCALE * voltage) - 12.07;
  
  return pressure;
}

void updateFlowRate() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastFlowTime >= 1000) { // Update every 1 second
    // Temporarily disable interrupt to read pulse count
    detachInterrupt(digitalPinToInterrupt(sensorPin));
    
    // Calculate flow rate in L/min
    float currentFlowRate = (pulseCount / (calibrationFactor / 60.0)); // Convert to L/min
    
    // Store in smoothing array
    flowRateArray[flowIndex] = currentFlowRate;
    flowIndex = (flowIndex + 1) % 5;
    
    // Calculate average flow rate for smoothing
    float sum = 0;
    for (int i = 0; i < 5; i++) {
      sum += flowRateArray[i];
    }
    flowRate = sum / 5.0;
    
    // Convert L/min to ml/min for pumpParams
    pumpParams.flowRate = (int)(flowRate * 1000); // Convert L/min to ml/min
    
    // Reset pulse count and timer
    pulseCount = 0;
    lastFlowTime = currentTime;
    
    // // Debug output
    // Serial.print("Flow Rate: ");
    // Serial.print(flowRate, 2);
    // Serial.print(" L/min (");
    // Serial.print(pumpParams.flowRate);
    // Serial.println(" ml/min)");
  }
}