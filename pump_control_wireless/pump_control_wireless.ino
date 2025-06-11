// pump_control_wireless.ino
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>

// Pin definitions for SD card
#define SD_CS 5
#define MOSI_PIN 25
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

// Function declarations
void pump_setup();
void pump_loop();

std::vector<String> getAvailableModesFromSD() {
  std::vector<String> modes;
  
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open SD card directory");
    return modes;
  }
  
  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break; // No more files
    }
    if (!entry.isDirectory()) {
      String fileName = entry.name();
      if (fileName.endsWith(".txt")) {
        // Extract mode name by removing the ".txt" extension
        String modeName = fileName.substring(0, fileName.length() - 4);
        modes.push_back(modeName);
      }
    }
    entry.close();
  }
  root.close();
  
  if (modes.size() == 0) {
    Serial.println("No modes available on SD card");
    // Add default modes if none exist
    modes.push_back("Otomatis");
    modes.push_back("Manual");
  }
  
  Serial.println("Available modes:");
  for (const String& mode : modes) {
    Serial.println("- " + mode);
  }
  
  return modes;
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Pump_Controller"); // Bluetooth device name
  Serial.println("ESP32 Bluetooth Ready. Waiting for connection...");

  SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
  initializeSDCard();
  availableModes = getAvailableModesFromSD();
  loadParametersFromSD(); 

  pump_setup();
}

void loop() {
  handleBluetoothReceive();
  if (millis() - lastSendTime >= SEND_INTERVAL) {
    sendActualParameters();
    lastSendTime = millis();
  }
  updateActualValues();
  controlPump();
  delay(50);
}

void initializeSDCard() {
  Serial.println("Initializing SD card...");
  if (!SD.begin(SD_CS)) {
    Serial.println("SD card initialization failed!");
    while (1); // Halt if SD card fails
  }
  Serial.println("SD card initialized.");
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
    
    if (loadParametersFromSD(newMode)) {
      sendModeConfirmation(newMode);
      sendDesiredParameters();
      Serial.println("Mode changed to: " + newMode);
    } else {
      sendErrorResponse("Failed to load mode parameters");
    }
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
    if (saveParametersToSD()) {
      sendParametersConfirmation();
      sendDesiredParameters(); // Send updated parameters back
      Serial.println("Parameters updated and saved");
    } else {
      sendErrorResponse("Failed to save parameters to SD card");
    }
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
  
  // Create a copy of current parameters with new mode name
  PumpParameters newMode = pumpParams;
  newMode.pumpMode = modeName;
  
  // Add mode to SD card
  if (addModeToSD(newMode)) {
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
    pump_loop();  ///pump is started
    
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

// Load default parameters (overloaded function)
void loadParametersFromSD() {
  Serial.println("Loading default parameters from SD Card");
  if (!loadParametersFromSD("Otomatis")) {
    Serial.println("Failed to load default parameters, using hardcoded values");
    // Keep current default values in pumpParams
  }
}

// Load parameters for specific mode
bool loadParametersFromSD(String modeName) {
  File file = SD.open("/" + modeName + ".txt", FILE_READ);
  if (!file) {
    Serial.println("Failed to open file: " + modeName + ".txt");
    return false;
  }
  
  // Read parameters line by line
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
  
  file.close();
  
  Serial.println("Parameters loaded for mode: " + modeName);
  return true;
}

// Save current parameters to SD card
bool saveParametersToSD() {
  File file = SD.open("/" + pumpParams.pumpMode + ".txt", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + pumpParams.pumpMode + ".txt");
    return false;
  }
  
  // Write parameters line by line
  file.println(pumpParams.heartRate);
  file.println(pumpParams.basePressure);
  file.println(pumpParams.systolicPressure);
  file.println(pumpParams.diastolicPressure);
  file.println(pumpParams.systolicPeriod);
  file.println(pumpParams.diastolicPeriod);
  file.println(pumpParams.notchPressure);
  file.println(pumpParams.systolicPeakTime);
  file.println(pumpParams.diastolicPeakTime);
  
  file.close();
  
  Serial.println("Parameters saved for mode: " + pumpParams.pumpMode);
  return true;
}

// Add new mode to SD card
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
  
  // Write parameters line by line
  file.println(mode.heartRate);
  file.println(mode.basePressure);
  file.println(mode.systolicPressure);
  file.println(mode.diastolicPressure);
  file.println(mode.systolicPeriod);
  file.println(mode.diastolicPeriod);
  file.println(mode.notchPressure);
  file.println(mode.systolicPeakTime);
  file.println(mode.diastolicPeakTime);
  
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