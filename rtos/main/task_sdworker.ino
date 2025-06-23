// task_sdworker.ino
#include "tasks.h"
#include <SD.h>

extern PumpParameters pumpParams;
extern QueueHandle_t sdQueue;
extern SemaphoreHandle_t sdMutex;

void sendDesiredParameters();
bool loadParametersFromSD(String modeName);
bool saveParametersToSD();
bool addModeToSD(PumpParameters mode);
bool deleteModeFromSD(String mode);

void TaskSDWorker(void *pv) {
  SDCommand cmd;
  for (;;) {
    if (xQueueReceive(sdQueue, &cmd, portMAX_DELAY) == pdPASS) {

      if (xSemaphoreTake(sdMutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        if (cmd.type == "LOAD") {
          if (loadParametersFromSD(cmd.mode)) {
            sendDesiredParameters();
          }
        } else if (cmd.type == "SAVE") {
          pumpParams = cmd.data;
          if (saveParametersToSD()) {
            sendDesiredParameters();
          }
        } else if (cmd.type == "ADD") {
          addModeToSD(cmd.data);
        } else if (cmd.type == "DEL") {
          deleteModeFromSD(cmd.mode);
        }
        xSemaphoreGive(sdMutex);
      }
    }
  }
}

// Load parameters for specific mode
bool loadParametersFromSD(String modeName) {
  File file = SD.open("/" + modeName + ".txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open file: " + modeName + ".txt");
    return false;
  }
  
  // Read parameters line by line - format urutan sesuai yang disimpan
  pumpParams.pumpMode = modeName;
  pumpParams.heartRate = file.readStringUntil('\n').toInt();
  pumpParams.basePressure = file.readStringUntil('\n').toInt();
  pumpParams.systolicPressure = file.readStringUntil('\n').toInt();
  pumpParams.diastolicPressure = file.readStringUntil('\n').toInt();
  pumpParams.systolicPeriod = file.readStringUntil('\n').toInt();
  pumpParams.diastolicPeriod = file.readStringUntil('\n').toInt();
  pumpParams.notchPressure = file.readStringUntil('\n').toInt();
  pumpParams.systolicPeakTime = file.readStringUntil('\n').toInt();
  pumpParams.diastolicPeakTime = file.readStringUntil('\n').toInt();
  
  // === PARAMETER BARU UNTUK OPEN LOOP ===
  pumpParams.closeloop = file.readStringUntil('\n').toInt();
  pumpParams.sysPWM = file.readStringUntil('\n').toInt();
  pumpParams.disPWM = file.readStringUntil('\n').toInt();
  pumpParams.sysPeriod = file.readStringUntil('\n').toInt();
  pumpParams.disPeriod = file.readStringUntil('\n').toInt();
  pumpParams.sysHighPercent = file.readStringUntil('\n').toInt();
  pumpParams.disHighPercent = file.readStringUntil('\n').toInt();
  
  file.close();
  
  Serial.println("Parameters loaded for mode: " + modeName);
  return true;
}

// Modifikasi saveParametersToSD() - menambahkan parameter open loop
bool saveParametersToSD() {
  File file = SD.open("/" + pumpParams.pumpMode + ".txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + pumpParams.pumpMode + ".txt");
    return false;
  }
  
  // Write parameters line by line - urutan harus sama dengan load
  file.println(pumpParams.heartRate);
  file.println(pumpParams.basePressure);
  file.println(pumpParams.systolicPressure);
  file.println(pumpParams.diastolicPressure);
  file.println(pumpParams.systolicPeriod);
  file.println(pumpParams.diastolicPeriod);
  file.println(pumpParams.notchPressure);
  file.println(pumpParams.systolicPeakTime);
  file.println(pumpParams.diastolicPeakTime);
  
  // === PARAMETER BARU UNTUK OPEN LOOP ===
  file.println(pumpParams.closeloop);
  file.println(pumpParams.sysPWM);
  file.println(pumpParams.disPWM);
  file.println(pumpParams.sysPeriod);
  file.println(pumpParams.disPeriod);
  file.println(pumpParams.sysHighPercent);
  file.println(pumpParams.disHighPercent);
  
  file.close();
  
  Serial.println("Parameters saved for mode: " + pumpParams.pumpMode);
  return true;
}

// Modifikasi addModeToSD() - menambahkan parameter open loop
bool addModeToSD(PumpParameters mode) {
  // Check if mode already exists
  if (SD.exists("/" + mode.pumpMode + ".txt")) {
    Serial.println("Mode already exists: " + mode.pumpMode);
    return false; // Mode already exists
  }
  
  File file = SD.open("/" + mode.pumpMode + ".txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create file: " + mode.pumpMode + ".txt");
    return false;
  }
  
  // Write parameters line by line - urutan harus sama dengan load dan save
  file.println(mode.heartRate);
  file.println(mode.basePressure);
  file.println(mode.systolicPressure);
  file.println(mode.diastolicPressure);
  file.println(mode.systolicPeriod);
  file.println(mode.diastolicPeriod);
  file.println(mode.notchPressure);
  file.println(mode.systolicPeakTime);
  file.println(mode.diastolicPeakTime);
  
  // === PARAMETER BARU UNTUK OPEN LOOP ===
  file.println(mode.closeloop);
  file.println(mode.sysPWM);
  file.println(mode.disPWM);
  file.println(mode.sysPeriod);
  file.println(mode.disPeriod);
  file.println(mode.sysHighPercent);
  file.println(mode.disHighPercent);
  
  file.close();
  
  Serial.println("New mode added: " + mode.pumpMode);
  return true;
}

// Delete mode from SD card
bool deleteModeFromSD(String modeName) {
  String fileName = "/" + modeName + ".txt";
  
  if (SD.exists(fileName)) {
    bool result = SD.remove(fileName);
    if (result) {
      Serial.println("Mode deleted: " + modeName);
    } else {
      Serial.println("Failed to delete mode: " + modeName);
    }
    return result;
  } else {
    Serial.println("Mode file does not exist: " + modeName);
    return false;
  }
}

// Check if mode is available
bool isModeAvailable(String mode) {
  for (const String& availableMode : availableModes) {
    if (availableMode == mode) {
      return true;
    }
  }
  return false;
}
