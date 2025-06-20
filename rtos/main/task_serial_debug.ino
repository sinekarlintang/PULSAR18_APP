#include "tasks.h"

extern String serialInputBuffer;
void handleSerialInput();
void processSerialCommand(String command);

void TaskSerialDebug(void *pv) {
  for (;;) {
    handleSerialInput();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void handleSerialInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (serialInputBuffer.length() > 0) {
        processSerialCommand(serialInputBuffer);
        serialInputBuffer = "";
      }
    } else {
      serialInputBuffer += c;
    }
  }
}

void processSerialCommand(String command) {
  command.trim();
  command.toUpperCase();
  
  // Parse command format: PARAMETER=VALUE
  int equalIndex = command.indexOf('=');
  if (equalIndex == -1) {
    if (command == "HELP" || command == "H") {
      printPIDHelp();
    } else if (command == "STATUS" || command == "S") {
      printPIDStatus();
    } else {
      Serial.println("Invalid command. Type HELP for available commands.");
    }
    return;
  }
  
  String parameter = command.substring(0, equalIndex);
  String valueStr = command.substring(equalIndex + 1);
  float value = valueStr.toFloat();
  
  bool parameterSet = false;
  
  // Outer PID parameters
  if (parameter == "KP_OUTER" || parameter == "KPO") {
    if (value >= 0 && value <= 50) {
      Kp_outer = value;
      parameterSet = true;
      Serial.println("Kp_outer set to: " + String(Kp_outer, 3));
    } else {
      Serial.println("Invalid range for Kp_outer (0-50)");
    }
  }
  else if (parameter == "KI_OUTER" || parameter == "KIO") {
    if (value >= 0 && value <= 50) {
      Ki_outer = value;
      parameterSet = true;
      Serial.println("Ki_outer set to: " + String(Ki_outer, 3));
    } else {
      Serial.println("Invalid range for Ki_outer (0-50)");
    }
  }
  else if (parameter == "KD_OUTER" || parameter == "KDO") {
    if (value >= 0 && value <= 10) {
      Kd_outer = value;
      parameterSet = true;
      Serial.println("Kd_outer set to: " + String(Kd_outer, 3));
    } else {
      Serial.println("Invalid range for Kd_outer (0-10)");
    }
  }
  else if (parameter == "N_OUTER" || parameter == "NO") {
    if (value >= 1 && value <= 1000) {
      N_outer = value;
      parameterSet = true;
      Serial.println("N_outer set to: " + String(N_outer, 1));
    } else {
      Serial.println("Invalid range for N_outer (1-1000)");
    }
  }
  // Inner PID parameters
  else if (parameter == "KP_INNER" || parameter == "KPI") {
    if (value >= 0 && value <= 10) {
      Kp_inner = value;
      parameterSet = true;
      Serial.println("Kp_inner set to: " + String(Kp_inner, 3));
    } else {
      Serial.println("Invalid range for Kp_inner (0-10)");
    }
  }
  else if (parameter == "KI_INNER" || parameter == "KII") {
    if (value >= 0 && value <= 10) {
      Ki_inner = value;
      parameterSet = true;
      Serial.println("Ki_inner set to: " + String(Ki_inner, 3));
    } else {
      Serial.println("Invalid range for Ki_inner (0-10)");
    }
  }
  else if (parameter == "KD_INNER" || parameter == "KDI") {
    if (value >= 0 && value <= 5) {
      Kd_inner = value;
      parameterSet = true;
      Serial.println("Kd_inner set to: " + String(Kd_inner, 3));
    } else {
      Serial.println("Invalid range for Kd_inner (0-5)");
    }
  }
  else {
    Serial.println("Unknown parameter: " + parameter);
    Serial.println("Type HELP for available parameters.");
  }
  
  // Reset integrators when PID parameters change
  if (parameterSet) {
    integral_outer = 0;
    integral_inner = 0;
    Serial.println("PID integrators reset.");
  }
}

void printPIDHelp() {
  Serial.println("=== PID Parameter Control ===");
  Serial.println("Commands:");
  Serial.println("  HELP or H        - Show this help");
  Serial.println("  STATUS or S      - Show current PID values");
  Serial.println("");
  Serial.println("Set Parameters (format: PARAMETER=VALUE):");
  Serial.println("Outer PID (Pressure Control):");
  Serial.println("  KP_OUTER=value   - Set Kp_outer (0-50)");
  Serial.println("  KI_OUTER=value   - Set Ki_outer (0-50)");
  Serial.println("  KD_OUTER=value   - Set Kd_outer (0-10)");
  Serial.println("  N_OUTER=value    - Set N_outer (1-1000)");
  Serial.println("");
  Serial.println("Inner PID (PWM Control):");
  Serial.println("  KP_INNER=value   - Set Kp_inner (0-10)");
  Serial.println("  KI_INNER=value   - Set Ki_inner (0-10)");
  Serial.println("  KD_INNER=value   - Set Kd_inner (0-5)");
  Serial.println("");
  Serial.println("Short forms: KPO, KIO, KDO, NO, KPI, KII, KDI");
  Serial.println("Examples:");
  Serial.println("  KP_OUTER=5.5");
  Serial.println("  KPO=7.2");
  Serial.println("  STATUS");
}

void printPIDStatus() {
  Serial.println("=== Current PID Parameters ===");
  Serial.println("Outer PID (Pressure):");
  Serial.println("  Kp_outer: " + String(Kp_outer, 3));
  Serial.println("  Ki_outer: " + String(Ki_outer, 3));
  Serial.println("  Kd_outer: " + String(Kd_outer, 3));
  Serial.println("  N_outer:  " + String(N_outer, 1));
  Serial.println("");
  Serial.println("Inner PID (PWM):");
  Serial.println("  Kp_inner: " + String(Kp_inner, 3));
  Serial.println("  Ki_inner: " + String(Ki_inner, 3));
  Serial.println("  Kd_inner: " + String(Kd_inner, 3));
  Serial.println("=============================");
}