extern String receivedData;
extern BluetoothSerial SerialBT;
extern PumpParameters pumpParams;
extern float readPressureFromADS();
extern std::vector<String> availableModes;
extern bool saveParametersToSD();
extern bool loadParametersFromSD(String mode);
extern bool addModeToSD(PumpParameters mode);
extern bool deleteModeFromSD(String mode);
extern bool isModeAvailable(String mode);
extern void resetControlSystem();

void sendErrorResponse(String msg);
void sendDesiredParameters();
void sendActualParameters();
void sendAvailableModes();
void sendModeConfirmation(String mode);
void sendModeAddedConfirmation(String modeName);
void sendModeDeletedConfirmation(String modeName);
void sendParametersConfirmation();
void sendStartStopConfirmation(int startPump);

void processReceivedData(String data);
void handleSetMode(DynamicJsonDocument &doc);
void handleGetParameters();
void handleSetParameters(DynamicJsonDocument &doc);
void handleSetStartStop(DynamicJsonDocument &doc);
void handleGetAvailableModes();
void handleAddMode(DynamicJsonDocument &doc);
void handleDeleteMode(DynamicJsonDocument &doc);


void TaskBluetooth(void *pv) {
  for (;;) {
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
    vTaskDelay(pdMS_TO_TICKS(10));
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
      // Reset control system when mode changes
      integral_outer = 0;
      integral_inner = 0;
      cycleStartTime = millis();
      
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
      
      // Reset control system when starting/stopping
      if (pumpParams.startPump == 1) {
        integral_outer = 0;
        integral_inner = 0;
        cycleStartTime = millis();
        Serial.println("Pump STARTED - Control system reset");
      } else {
        resetControlSystem();
        setpointPressure = 0;
        ledcWrite(PUMP_PWM_PIN, 0);
        currentPWM = 0;
        integral_outer = 0;
        integral_inner = 0;
        Serial.println("Pump STOPPED");
      }
      
      sendStartStopConfirmation(newStartPump);
      
      Serial.println("Pump start/stop changed to: " + String(pumpParams.startPump));
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
  // Serial.println("Sent: " + pumpParams.pressureActual);
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
