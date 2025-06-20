#include "tasks.h"

extern PumpParameters pumpParams;
extern float cycleTime, systolicDuration, diastolicDuration, sysPeak;
float calculateSystolicPressure(float progress, float sysPeak);
float calculateDiastolicPressure(float progress, float diastolicDuration);
float generateBloodPressureWaveform();

void TaskWaveform(void *pv) {
  for (;;) {
    float bpm = pumpParams.heartRate;
    cycleTime = 60.0 / bpm;
    systolicDuration = pumpParams.systolicPeriod / 100.0 * cycleTime;
    diastolicDuration = pumpParams.diastolicPeriod / 100.0 * cycleTime;
    sysPeak = pumpParams.systolicPeakTime / 1000.0;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

float calculateSystolicPressure(float progress, float sysPeak) {
  float tPeak = sysPeak;
  float basePressureKPa = pumpParams.basePressure ; // Convert mmHg to kPa
  float systolicPressureKPa = pumpParams.systolicPressure ; // Convert mmHg to kPa
  float notchPressureKPa = pumpParams.notchPressure ; // Convert mmHg to kPa
  
  if (progress <= tPeak) {
    // Rising: basePressure → systolicPressure (half cosine wave)
    float rise = progress / tPeak; // 0 … 1
    float factor = (1.0 - cos(rise * PI)) / 2.0; // 0 … 1
    return basePressureKPa + (systolicPressureKPa - basePressureKPa) * factor;
  } else {
    // Falling: systolicPressure → notchPressure (half cosine wave)
    float fall = (progress - tPeak) / (1.0 - tPeak); // 0 … 1
    float factor = (1.0 + cos(fall * PI)) / 2.0;
    return notchPressureKPa + (systolicPressureKPa - notchPressureKPa) * factor;
  }
}

float calculateDiastolicPressure(float progress, float diastolicDuration) {
  float tPeak = pumpParams.diastolicPeakTime / 1000.0; // Convert per mille to fraction
  float basePressureKPa = pumpParams.basePressure ; // Convert mmHg to kPa
  float diastolicPressureKPa = pumpParams.diastolicPressure ; // Convert mmHg to kPa
  float notchPressureKPa = pumpParams.notchPressure ; // Convert mmHg to kPa
  
  if (progress <= tPeak) {
    // Rising: notchPressure → diastolicPressure (half cosine wave)
    float rise = progress / tPeak; // 0 … 1
    float factor = (1.0 - cos(rise * PI)) / 2.0; // 0 … 1
    return notchPressureKPa + (diastolicPressureKPa - notchPressureKPa) * factor;
  } else {
    // Falling: diastolicPressure → basePressure (half cosine wave)
    float fall = (progress - tPeak) / (1.0 - tPeak); // 0 … 1
    float factor = (1.0 + cos(fall * PI)) / 2.0; // 1 … 0
    return basePressureKPa + (diastolicPressureKPa - basePressureKPa) * factor;
  }
}

float generateBloodPressureWaveform() {
  unsigned long currentTime = millis();
  float timeInCycle = ((currentTime - cycleStartTime) / 1000.0); // convert to seconds
  
  // Reset cycle if completed
  if (timeInCycle >= cycleTime) {
    cycleStartTime = currentTime;
    timeInCycle = 0;
  }
  
  float pressure;
  
  if (timeInCycle <= systolicDuration) {
    // Systolic phase
    float systolicProgress = timeInCycle / systolicDuration;
    pressure = calculateSystolicPressure(systolicProgress, sysPeak);
  } else {
    // Diastolic phase
    float diastolicProgress = (timeInCycle - systolicDuration) / diastolicDuration;
    pressure = calculateDiastolicPressure(diastolicProgress, diastolicDuration);
  }
  
  return pressure;
}