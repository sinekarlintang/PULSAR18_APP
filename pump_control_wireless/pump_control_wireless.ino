#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions for SD card
#define SD_CS 5
#define MOSI_PIN 23
#define MISO_PIN 19
#define CLK_PIN 18

BluetoothSerial SerialBT;

// Structure for pump parameters
struct PumpParameters {
  int heartRate = 80;             // BPM                   (desired)
  int systolicPressure = 120;     // mmHg                  (desired)
  int diastolicPressure = 80;     // mmHg                  (desired)
  int systolicPeriod = 60;        // % from cycle          (desired)
  int diastolicPeriod = 40;       // % from cycle          (desired)
  int notchPressure = 60;         // mmHg                  (desired)
  int systolicPeakTime = 0;       // ms from cycle start   (desired)
  int diastolicPeakTime = 0;      // ms from cycle start   (desired)
  int flowRate = 80;              // ml/min                (actual)
  int pressureActual = 80;        // mmhg                  (actual)
  String pumpMode = "Otomatis";   // (desired)
  int startPump = 0;              // 0 = stop, 1 = start   (desired)
  int basePressure = 0;
} pumpParams;

// Buffer for receiving data
String receivedData = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 50; // Send data every 50ms
float t = 0;

// Available modes storage
std::vector<String> availableModes;

std::vector<String> getAvailableModesFromSD() {
  std::vector<String> modes;
  // Load available modes from SD card
  // For now return dummy data
  modes.push_back("Otomatis");
  modes.push_back("Manual");
  modes.push_back("Test");
  modes.push_back("Pediatric");
  modes.push_back("Cardiac");
  
  Serial.println("Reading available modes from SD Card (dummy implementation)");
  
  return modes;
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Pump_Controller"); // Bluetooth device name
  Serial.println("ESP32 Bluetooth Ready. Waiting for connection...");

  // Initialize available modes
  availableModes = getAvailableModesFromSD();
  
  loadParametersFromSD(); 
}

void loop() {
  handleBluetoothReceive();
  
  // Send actual data periodically
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendActualParameters();
    lastSendTime = millis();
  }
  
  updateActualValues();
  
  controlPump();
  
  delay(50);
}

void handleBluetoothReceive() {
  while (SerialBT.available()) {
    char c = SerialBT.read();
    
    if (c == '\n' || c == '\r') {
      if (receivedData.length() > 0) {
        processReceivedData(receivedData);
        receivedData = "";
      }
    } else {
      receivedData += c;
    }
  }
}

void processReceivedData(String data) {
  Serial.println("Received: " + data);
  DynamicJsonDocument doc(1024);
  DeserializationError error = deserializeJson(doc, data);
  
  if (error) {
    Serial.println("JSON parsing failed!");
    sendErrorResponse("Invalid JSON format");
    return;
  }
  
  String messageType = doc["type"];
  
  if (messageType == "SET_MODE") {
    handleSetMode(doc);
  } else if (messageType == "GET_PARAMETERS") {
    handleGetParameters();
  } else if (messageType == "SET_PARAMETERS") {
    handleSetParameters(doc);
  } else if (messageType == "SET_START_STOP") {
    handleSetStartStop(doc);
  } else if (messageType == "GET_AVAILABLE_MODES") {
    handleGetAvailableModes();
  } else if (messageType == "ADD_MODE") {
    handleAddMode(doc);
  } else if (messageType == "DELETE_MODE") {
    handleDeleteMode(doc);
  } else {
    sendErrorResponse("Unknown message type");
  }
}

void handleSetMode(DynamicJsonDocument &doc) {
  String newMode = doc["mode"];
  if (isModeAvailable(newMode)) {
    pumpParams.pumpMode = newMode;
    
    loadParametersFromSD(newMode);
    
    sendModeConfirmation(newMode);
    sendDesiredParameters();
    
    Serial.println("Mode changed to: " + newMode);
  } else {
    sendErrorResponse("Invalid mode or mode not available");
  }
}

void handleGetParameters() {
  sendDesiredParameters();
}

void handleSetParameters(DynamicJsonDocument &doc) {
  bool parametersChanged = false;
  
  if (doc.containsKey("heartRate")) {
    int newValue = doc["heartRate"];
    if (newValue >= 30 && newValue <= 200) { // Valid heart rate range
      pumpParams.heartRate = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("systolicPressure")) {
    int newValue = doc["systolicPressure"];
    if (newValue >= 50 && newValue <= 250) { // Valid systolic pressure range
      pumpParams.systolicPressure = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("diastolicPressure")) {
    int newValue = doc["diastolicPressure"];
    if (newValue >= 30 && newValue <= 150) { // Valid diastolic pressure range
      pumpParams.diastolicPressure = newValue;
      parametersChanged = true;
    }
  }
  if (doc.containsKey("basePressure")) {
    int newValue = doc["basePressure"];
    if (newValue >= 30 && newValue <= 150) { // Valid base
      pumpParams.basePressure = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("systolicPeriod")) {
    int newValue = doc["systolicPeriod"];
    if (newValue >= 10 && newValue <= 90) { // Valid percentage range
      pumpParams.systolicPeriod = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("diastolicPeriod")) {
    int newValue = doc["diastolicPeriod"];
    if (newValue >= 10 && newValue <= 90) { // Valid percentage range
      pumpParams.diastolicPeriod = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("notchPressure")) {
    int newValue = doc["notchPressure"];
    if (newValue >= 20 && newValue <= 150) { // Valid notch pressure range
      pumpParams.notchPressure = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("systolicPeakTime")) {
    int newValue = doc["systolicPeakTime"];
    if (newValue >= 0 && newValue <= 5000) { // Valid time range in ms
      pumpParams.systolicPeakTime = newValue;
      parametersChanged = true;
    }
  }
  
  if (doc.containsKey("diastolicPeakTime")) {
    int newValue = doc["diastolicPeakTime"];
    if (newValue >= 0 && newValue <= 5000) { // Valid time range in ms
      pumpParams.diastolicPeakTime = newValue;
      parametersChanged = true;
    }
  }
  
  if (parametersChanged) {
    saveParametersToSD();
    sendParametersConfirmation();
    sendDesiredParameters(); // Send updated parameters back
    Serial.println("Parameters updated and saved");
  } else {
    sendErrorResponse("No valid parameters provided or values out of range");
  }
}

void handleSetStartStop(DynamicJsonDocument &doc) {
  if (doc.containsKey("startPump")) {
    int newStartPump = doc["startPump"];
    if (newStartPump == 0 || newStartPump == 1) {
      pumpParams.startPump = newStartPump;
      
      sendStartStopConfirmation(newStartPump);
      
      Serial.println("Pump start/stop changed to: " + String(newStartPump));
      Serial.println(newStartPump == 1 ? "Pump STARTED" : "Pump STOPPED");
    } else {
      sendErrorResponse("Invalid startPump value (must be 0 or 1)");
    }
  } else {
    sendErrorResponse("Missing startPump parameter");
  }
}

void handleGetAvailableModes() {
  sendAvailableModes();
}

void handleAddMode(DynamicJsonDocument &doc) {
  if (!doc.containsKey("modeName")) {
    sendErrorResponse("Missing modeName parameter");
    return;
  }
  
  String modeName = doc["modeName"];
  
  // Validate mode name
  if (modeName.length() == 0 || modeName.length() > 50) {
    sendErrorResponse("Invalid mode name length (1-50 characters)");
    return;
  }
  
  // Check if mode already exists
  if (isModeAvailable(modeName)) {
    sendErrorResponse("Mode already exists");
    return;
  }
  
  // Add mode to SD card and memory
  if (addModeToSD(modeName)) {
    // Add to available modes list
    availableModes.push_back(modeName);
    
    // Send confirmation
    sendModeAddedConfirmation(modeName);
    
    // Send updated available modes list
    sendAvailableModes();
    
    Serial.println("Mode added successfully: " + modeName);
  } else {
    sendErrorResponse("Failed to add mode to storage");
  }
}

void handleDeleteMode(DynamicJsonDocument &doc) {
  if (!doc.containsKey("modeName")) {
    sendErrorResponse("Missing modeName parameter");
    return;
  }
  
  String modeName = doc["modeName"];
  
  // Check if mode exists
  if (!isModeAvailable(modeName)) {
    sendErrorResponse("Mode does not exist");
    return;
  }
  
  // Prevent deletion of essential modes
  if (modeName == "Otomatis" || modeName == "Manual") {
    sendErrorResponse("Cannot delete essential system modes");
    return;
  }
  
  // Delete mode from SD card
  if (deleteModeFromSD(modeName)) {
    // Remove from available modes list
    for (auto it = availableModes.begin(); it != availableModes.end(); ++it) {
      if (*it == modeName) {
        availableModes.erase(it);
        break;
      }
    }
    
    // If current mode was deleted, switch to default mode
    if (pumpParams.pumpMode == modeName) {
      pumpParams.pumpMode = "Otomatis";
      loadParametersFromSD("Otomatis");
      sendModeConfirmation("Otomatis");
      sendDesiredParameters();
    }
    
    // Send confirmation
    sendModeDeletedConfirmation(modeName);
    
    // Send updated available modes list
    sendAvailableModes();
    
    Serial.println("Mode deleted successfully: " + modeName);
  } else {
    sendErrorResponse("Failed to delete mode from storage");
  }
}

void sendModeConfirmation(String mode) {
  DynamicJsonDocument doc(512);
  doc["type"] = "MODE_CONFIRMED";
  doc["mode"] = mode;
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendDesiredParameters() {
  DynamicJsonDocument doc(1024);
  doc["type"] = "DESIRED_PARAMETERS";
  doc["heartRate"] = pumpParams.heartRate;
  doc["systolicPressure"] = pumpParams.systolicPressure;
  doc["diastolicPressure"] = pumpParams.diastolicPressure;
  doc["systolicPeriod"] = pumpParams.systolicPeriod;
  doc["diastolicPeriod"] = pumpParams.diastolicPeriod;
  doc["notchPressure"] = pumpParams.notchPressure;
  doc["systolicPeakTime"] = pumpParams.systolicPeakTime;
  doc["diastolicPeakTime"] = pumpParams.diastolicPeakTime;
  doc["mode"] = pumpParams.pumpMode;
  doc["startPump"] = pumpParams.startPump;
  doc["basePressure"] = pumpParams.basePressure;
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendActualParameters() {
  DynamicJsonDocument doc(512);
  doc["type"] = "ACTUAL_PARAMETERS";
  doc["flowRate"] = pumpParams.flowRate;
  doc["pressureActual"] = pumpParams.pressureActual;
  doc["startPump"] = pumpParams.startPump;
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendAvailableModes() {
  DynamicJsonDocument doc(512);
  doc["type"] = "AVAILABLE_MODES";
  
  JsonArray modesArray = doc.createNestedArray("modes");
  
  for (const String& mode : availableModes) {
    modesArray.add(mode);
  }
  
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
  
  Serial.println("Available modes sent");
}

void sendModeAddedConfirmation(String modeName) {
  DynamicJsonDocument doc(512);
  doc["type"] = "MODE_ADDED";
  doc["modeName"] = modeName;
  doc["message"] = "Mode '" + modeName + "' added successfully";
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendModeDeletedConfirmation(String modeName) {
  DynamicJsonDocument doc(512);
  doc["type"] = "MODE_DELETED";
  doc["modeName"] = modeName;
  doc["message"] = "Mode '" + modeName + "' deleted successfully";
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendParametersConfirmation() {
  DynamicJsonDocument doc(256);
  doc["type"] = "PARAMETERS_SAVED";
  doc["message"] = "Parameters saved successfully";
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendStartStopConfirmation(int startPump) {
  DynamicJsonDocument doc(512);
  doc["type"] = "START_STOP_CONFIRMED";
  doc["startPump"] = startPump;
  doc["message"] = startPump == 1 ? "Pump started" : "Pump stopped";
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void sendErrorResponse(String errorMessage) {
  DynamicJsonDocument doc(256);
  doc["type"] = "ERROR";
  doc["message"] = errorMessage;
  doc["timestamp"] = millis();
  
  String jsonString;
  serializeJson(doc, jsonString);
  SerialBT.println(jsonString);
}

void controlPump() {
  if (pumpParams.startPump == 1) {
    // Pump is active - implement pump control
    
  } else {
    // Pump is stopped
  }
}

void updateActualValues() {
  // Update actual values only if pump is running
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 50) { 
    
    if (pumpParams.startPump == 1) {
      // Simulate values when pump is running
      pumpParams.flowRate = 75 + random(-10, 11);
      pumpParams.pressureActual = 100 + 20 * sin(t) + random(-5, 5);  // ~80–120 mmHg
      t += 0.1;
    } else {
      // Values when pump is stopped
      pumpParams.flowRate = 0;
      pumpParams.pressureActual = 0;
      t = 0; // Reset time
    }
    
    lastUpdate = millis();
  }
}

//////////////////////////////////////// SD Card Integration Functions /////////////////////////////////////////

void loadParametersFromSD() {
  // Load default parameters
  Serial.println("Loading default parameters from SD Card");
}

void loadParametersFromSD(String mode) {
  // Load parameters based on mode from SD Card
  Serial.println("Loading parameters for mode: " + mode);
   
  // Dummy implementation
  if (mode == "Hipertension") {
    pumpParams.heartRate = 80;
    pumpParams.systolicPressure = 120;
    pumpParams.diastolicPressure = 80;
    pumpParams.systolicPeriod = 60;
    pumpParams.diastolicPeriod = 40;
    pumpParams.notchPressure = 75;
    pumpParams.systolicPeakTime = 100;
    pumpParams.diastolicPeakTime = 300;
    pumpParams.basePressure = 60;
  } else if (mode == "Normal") {
    pumpParams.heartRate = 70;
    pumpParams.systolicPressure = 110;
    pumpParams.diastolicPressure = 70;
    pumpParams.systolicPeriod = 55;
    pumpParams.diastolicPeriod = 45;
    pumpParams.notchPressure = 55;
    pumpParams.systolicPeakTime = 120;
    pumpParams.diastolicPeakTime = 350;
    pumpParams.basePressure = 40;
  } else if (mode == "Test") {
    pumpParams.heartRate = 60;
    pumpParams.systolicPressure = 100;
    pumpParams.diastolicPressure = 80;
    pumpParams.systolicPeriod = 50;
    pumpParams.diastolicPeriod = 50;
    pumpParams.notchPressure = 75;
    pumpParams.systolicPeakTime = 150;
    pumpParams.diastolicPeakTime = 400;
    pumpParams.basePressure = 60;
  } else if (mode == "Pediatric") {
    pumpParams.heartRate = 120;
    pumpParams.systolicPressure = 95;
    pumpParams.diastolicPressure = 80;
    pumpParams.systolicPeriod = 65;
    pumpParams.diastolicPeriod = 35;
    pumpParams.notchPressure = 70;
    pumpParams.systolicPeakTime = 80;
    pumpParams.diastolicPeakTime = 250;
    pumpParams.basePressure = 60;
  } else if (mode == "Cardiac") {
    pumpParams.heartRate = 65;
    pumpParams.systolicPressure = 140;
    pumpParams.diastolicPressure = 90;
    pumpParams.systolicPeriod = 70;
    pumpParams.diastolicPeriod = 30;
    pumpParams.notchPressure = 70;
    pumpParams.systolicPeakTime = 90;
    pumpParams.diastolicPeakTime = 280;
    pumpParams.basePressure = 60;
  }
}

void saveParametersToSD() {
  // Save parameters to SD Card
  Serial.println("Saving parameters to SD Card");
  // TODO: Implement actual SD Card saving

}

bool addModeToSD(String modeName) {
  // Add new mode to SD Card
  Serial.println("Adding mode to SD Card: " + modeName);

  return true; // Return true for dummy implementation
}

bool deleteModeFromSD(String modeName) {
  // Delete mode from SD Card
  Serial.println("Deleting mode from SD Card: " + modeName);

  return true; // Return true for dummy implementation
}

bool isModeAvailable(String mode) {
  // Check if mode is available
  for (const String& availableMode : availableModes) {
    if (availableMode == mode) {
      return true;
    }
  }
  return false;
}