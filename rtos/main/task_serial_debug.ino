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
  command.toLowerCase(); // Changed to lowercase
  
  // Check for help and status commands first
  if (command == "help" || command == "h") {
    printPIDHelp();
    return;
  } else if (command == "status" || command == "s") {
    printPIDStatus();
    return;
  }
  
  // Parse direct parameter commands (e.g., kp6, kpi7, ki5, etc.)
  bool parameterSet = false;
  float value = 0;
  
  // Parse parameter commands: kp6, kpi7, ki5, kii8, kd3, kdi4, n100
  if (command.startsWith("kp") && command.length() > 2 && !command.startsWith("kpi")) {
    // Outer PID Kp: kp6 means set Kp_outer = 6
    String valueStr = command.substring(2);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 50) {
      Kp_outer = value;
      parameterSet = true;
      Serial.println("Kp_outer set to: " + String(Kp_outer, 3));
    } else {
      Serial.println("Invalid range for Kp_outer (0-50)");
    }
  }
  else if (command.startsWith("kpi") && command.length() > 3) {
    // Inner PID Kp: kpi7 means set Kp_inner = 7
    String valueStr = command.substring(3);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 10) {
      Kp_inner = value;
      parameterSet = true;
      Serial.println("Kp_inner set to: " + String(Kp_inner, 3));
    } else {
      Serial.println("Invalid range for Kp_inner (0-10)");
    }
  }
  else if (command.startsWith("ki") && command.length() > 2 && !command.startsWith("kii")) {
    // Outer PID Ki: ki5 means set Ki_outer = 5
    String valueStr = command.substring(2);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 50) {
      Ki_outer = value;
      parameterSet = true;
      Serial.println("Ki_outer set to: " + String(Ki_outer, 3));
    } else {
      Serial.println("Invalid range for Ki_outer (0-50)");
    }
  }
  else if (command.startsWith("kii") && command.length() > 3) {
    // Inner PID Ki: kii8 means set Ki_inner = 8
    String valueStr = command.substring(3);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 10) {
      Ki_inner = value;
      parameterSet = true;
      Serial.println("Ki_inner set to: " + String(Ki_inner, 3));
    } else {
      Serial.println("Invalid range for Ki_inner (0-10)");
    }
  }
  else if (command.startsWith("kd") && command.length() > 2 && !command.startsWith("kdi")) {
    // Outer PID Kd: kd3 means set Kd_outer = 3
    String valueStr = command.substring(2);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 10) {
      Kd_outer = value;
      parameterSet = true;
      Serial.println("Kd_outer set to: " + String(Kd_outer, 3));
    } else {
      Serial.println("Invalid range for Kd_outer (0-10)");
    }
  }
  else if (command.startsWith("kdi") && command.length() > 3) {
    // Inner PID Kd: kdi4 means set Kd_inner = 4
    String valueStr = command.substring(3);
    value = valueStr.toFloat();
    if (value >= 0 && value <= 5) {
      Kd_inner = value;
      parameterSet = true;
      Serial.println("Kd_inner set to: " + String(Kd_inner, 3));
    } else {
      Serial.println("Invalid range for Kd_inner (0-5)");
    }
  }
  else if (command.startsWith("n") && command.length() > 1) {
    // Outer PID N: n100 means set N_outer = 100
    String valueStr = command.substring(1);
    value = valueStr.toFloat();
    if (value >= 1 && value <= 1000) {
      N_outer = value;
      parameterSet = true;
      Serial.println("N_outer set to: " + String(N_outer, 1));
    } else {
      Serial.println("Invalid range for N_outer (1-1000)");
    }
  }
  else {
    Serial.println("Unknown command: " + command);
    Serial.println("Available commands: kp[value], kpi[value], ki[value], kii[value], kd[value], kdi[value], n[value]");
    Serial.println("Examples: kp6, kpi7, ki5, kii8, kd3, kdi4, n100");
    Serial.println("Type help for more information.");
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
  Serial.println("Pump Control:");
  Serial.println("  START_PUMP=value - Set pump start/stop (0 or 1)");
  Serial.println("");
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
  Serial.println("  START_PUMP=1");
  Serial.println("  KP_OUTER=5.5");
  Serial.println("  KPO=7.2");
  Serial.println("  STATUS");
}

void printPIDStatus() {
  Serial.println("=== Current PID Parameters ===");
  Serial.println("Pump Status:");
  Serial.println("  Start Pump: " + String(pumpParams.startPump));
  Serial.println("");
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