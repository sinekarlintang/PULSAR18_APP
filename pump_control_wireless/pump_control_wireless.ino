
// pump_control_wireless_integrated.ino
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// Pin definitions for SD card
#define SD_CS 5
#define MOSI_PIN 25
#define MISO_PIN 19
#define CLK_PIN 18

// Pump control pins
#define PUMP_PWM_PIN     2
#define PWM_FREQ         30000
#define PWM_RESOLUTION   8

BluetoothSerial SerialBT;

// --- ADS1115 Configuration ---
Adafruit_ADS1115 ads;  // Use default address 0x48 (ADDR to GND)
const uint8_t ADS_CHANNEL = 0;  // A0 pin on ADS1115

// --- Pressure Sensor Configuration ---
const float SENSOR_SUPPLY = 4.75;     // Actual sensor supply voltage
const float SENSOR_VMIN = 0.45;      // Sensor minimum output (V)
const float SENSOR_VMAX = 4.05;      // Sensor maximum output (V)
float VD_RATIO = 0.6687;             // Voltage divider ratio
float VOUT_MIN = 0.992;              // ~0.317V
float VOUT_MAX = 2.8481;             // ~2.857V
const float P_MIN = 0.0;             // Minimum pressure (mmHg)
const float P_MAX = 750.062;         // Maximum pressure (mmHg)
float SCALE = 272.86;                // kPa per volt

// Flow sensor configuration
const int sensorPin = 4;              // Flow sensor input pin
volatile int pulseCount = 0;          // Pulse count for flow measurement
float flowRate = 0.0;                 // Flow rate in L/min
unsigned long lastFlowTime = 0;       // Last time flow was calculated
float calibrationFactor = 13.5;       // Pulses per L/min based on graph
float flowRateArray[5] = {0};         // Array for flow rate smoothing
int flowIndex = 0;                    // Index for flow rate array
volatile unsigned long lastPulseTime = 0; // For debouncing

// --- PID Configuration ---
// Outer PID (Pressure)
float Kp_outer = 7;    // Proportional gain
float Ki_outer = 5;    // Integral gain
float Kd_outer = 0.5;   // Derivative gain
float N_outer = 100;    // Derivative filter coefficient
float setpointPressure = 0; // Current setpoint (pressure in kPa)
float integral_outer = 0;
float lastError_outer = 0;
float filteredErrorRate_outer = 0; // Filtered derivative term
const float deadZone_outer = 0.5;  // Dead zone

// Inner PID (PWM/Speed)
float Kp_inner = 1.0;    // Proportional gain for inner loop
float Ki_inner = 0;      // Integral gain for inner loop
float Kd_inner = 0;      // Derivative gain for inner loop
float integral_inner = 0;
float lastError_inner = 0;
float filteredErrorRate_inner = 0;
float setpointPWM = 0;   // PWM setpoint from outer loop

// --- Pump Control Variables ---
uint8_t currentPWM = 0;

// --- Waveform State Variables ---
unsigned long cycleStartTime = 0;
float cycleTime = 0;              // seconds per cycle
float systolicDuration = 0;       // duration of systolic phase
float diastolicDuration = 0;      // duration of diastolic phase
float sysPeak = 0;                // normalized systolic peak time

// --- Kalman Filter Configuration ---
float x = 0;      // Filtered pressure estimate
float p = 1.0;    // Error covariance
float r = 1;      // Measurement noise
float q = 0.005;  // Process noise

// --- Sampling Configuration ---
const unsigned long sampleInterval = 10;      // 10ms for outer loop
const unsigned long innerSampleInterval = 1;  // 1ms for inner loop
unsigned long lastSampleTime = 0;
unsigned long lastInnerSampleTime = 0;

// Structure for pump parameters
struct PumpParameters {
  int heartRate = 80;             // BPM                   (desired)
  int systolicPressure = 120;     // mmHg                  (desired)
  int diastolicPressure = 80;     // mmHg                  (desired)
  int systolicPeriod = 60;        // % from cycle          (desired)
  int diastolicPeriod = 40;       // % from cycle          (desired)
  int notchPressure = 60;         // mmHg                  (desired)
  int systolicPeakTime = 600;     // per mille from cycle start (desired)
  int diastolicPeakTime = 300;    // per mille from cycle start (desired)
  int flowRate = 80;              // ml/min                (actual)
  int pressureActual = 80;        // mmhg                  (actual)
  String pumpMode = "Otomatis";   // (desired)
  int startPump = 0;              // 0 = stop, 1 = start   (desired)
  int basePressure = 80;          // mmHg                  (desired)
} pumpParams;


// Buffer for receiving data
String receivedData = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 50; // Send data every 50ms

// Available modes storage
std::vector<String> availableModes;

// Function declarations
void pump_setup();
void pump_loop();


String serialInputBuffer = "";

// Tambahkan fungsi ini sebelum setup()
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SENSOR READ TASKS - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// --- ADS1115 Reading Function ---
float readPressureFromADS() {
  // Read from ADS1115 channel A0
  int16_t adcValue = ads.readADC_SingleEnded(ADS_CHANNEL);
  
  // Convert to voltage
  // ADS1115 with gain 1 has range of ±4.096V
  // Resolution is 4.096V / 32768 = 0.125mV per bit
  float voltage = adcValue * 0.000125; // Convert to volts
  
  // Calculate pressure from voltage
  float pressure = (SCALE * voltage) - 27.07;
  
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// WAVEFORM GENERATION TASKS - TO BE MOVED TO CORE 0
// Akan dipanggil task control untuk penentuan setpoint

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// --- Waveform Calculation Functions ---
void updateWaveformParameters() {
  cycleTime = 60.0 / pumpParams.heartRate; // seconds per cycle
  systolicDuration = (pumpParams.systolicPeriod / 100.0) * cycleTime;
  diastolicDuration = cycleTime - systolicDuration;
  sysPeak = (pumpParams.systolicPeakTime / 1000.0); // convert from per mille to fraction
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// PUMP CONTROL TASKS - TO BE MOVED TO CORE 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void pump_setup() {
  Serial.println("Initializing pump control system...");
  
  // Initialize I2C with ESP32 default pins
  Wire.begin(21, 22); // SDA, SCL
  
  // // Initialize ADS1115
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

  // Initialize waveform parameters
  updateWaveformParameters();
  cycleStartTime = millis();

  // Initialize Kalman filter with first reading
  delay(100);
  // float initialPressure = 0;
  // for(int i = 0; i < 10; i++) {
  //   initialPressure += readPressureFromADS();
  //   delay(10);
  // }
  // x = initialPressure / 10.0;
  
  Serial.println("Pump control system initialized");
  Serial.print("Initial pressure: "); Serial.print(x); Serial.println(" kPa");
}

void pump_open_loop(){

}

void pump_loop() {
  // Only run pump control if pump is started
  if (pumpParams.startPump != 1) {
    // Stop pump immediately if not started
    ledcWrite(PUMP_PWM_PIN, 0);
    currentPWM = 0;
    // Reset integrators
    integral_outer = 0;
    integral_inner = 0;
    return;
  }
  
  unsigned long currentTime = millis();
  
  // Update waveform parameters and generate setpoint
  updateWaveformParameters();
  setpointPressure = generateBloodPressureWaveform();

  // Outer PID control loop (Pressure)
  if (currentTime - lastSampleTime >= sampleInterval) {
    // Read pressure from ADS1115
    float currentPressure = readPressureFromADS();

    // Kalman Filter for smoothing pressure
    p += q; // Prediction
    float k = p / (p + r); // Kalman Gain
    x += k * (currentPressure - x); // Update estimate
    p *= (1 - k); // Update error covariance

    // Update actual pressure for monitoring (convert kPa to mmHg)
    pumpParams.pressureActual = (int)generateBloodPressureWaveform();

    // Outer PID: Calculate PWM setpoint
    float dt_outer = sampleInterval / 1000.0; // Convert to seconds
    float error_outer = setpointPressure - x; // Use filtered pressure

    // Calculate PID components
    float P_term = Kp_outer * error_outer;
    float I_term = Ki_outer * integral_outer;
    float D_term = 0;
    
    // Only update integral and derivative if outside dead zone
    if (abs(error_outer) >= deadZone_outer) {
      // Update derivative with filter
      float errorRate_outer = (error_outer - lastError_outer) / dt_outer;
      filteredErrorRate_outer = (N_outer * errorRate_outer + filteredErrorRate_outer * (1.0 / (dt_outer * N_outer))) / (N_outer + 1.0 / dt_outer);
      D_term = Kd_outer * filteredErrorRate_outer;
      
      // Update integral with anti-windup
      float temp_integral = integral_outer + error_outer * dt_outer;
      float temp_output = P_term + Ki_outer * temp_integral + D_term;
      
      // Only integrate if we're not saturated
      if (temp_output >= 0 && temp_output <= 255) {
        integral_outer = temp_integral;
        integral_outer = constrain(integral_outer, -50, 50); // Secondary limit
      }
    }
    
    // Calculate total control output
    float control_outer = P_term + I_term + D_term;
    
    // Add feedforward/bias term based on average expected pressure
    float pwm_bias = 30; // Adjust this based on your system's characteristics
    
    // Set PWM setpoint with bias
    setpointPWM = pwm_bias + control_outer;
    setpointPWM = constrain(setpointPWM, 0, 255);

    lastError_outer = error_outer;
    lastSampleTime = currentTime;

    // // Output for monitoring (tab-separated for Serial Plotter)
    // Serial.print(setpointPressure / 0.133322, 2);
    // Serial.print('\t');
    // Serial.print(x / 0.133322, 2); // Filtered pressure
    // Serial.print('\t');
    // Serial.print(setpointPWM);
    // Serial.print('\t');
    // Serial.print(currentPWM);
    Serial.println(pumpParams.pressureActual);
  }

  // Inner PID control loop (PWM/Speed)
  if (currentTime - lastInnerSampleTime >= innerSampleInterval) {
    float dt_inner = innerSampleInterval / 1000.0; // Convert to seconds
    float error_inner = setpointPWM - currentPWM;

    // Calculate PID components for inner loop
    float P_inner = Kp_inner * error_inner;
    float I_inner = Ki_inner * integral_inner;
    float D_inner = 0;
    
    // Update integral with anti-windup
    float temp_integral_inner = integral_inner + error_inner * dt_inner;
    float temp_control_inner = P_inner + Ki_inner * temp_integral_inner + D_inner;
    
    // Only integrate if we won't saturate the rate limiter
    if (abs(temp_control_inner) <= 30) {
      integral_inner = temp_integral_inner;
      integral_inner = constrain(integral_inner, -50, 50);
    }
    
    if (Kd_inner > 0) {
      float errorRate_inner = (error_inner - lastError_inner) / dt_inner;
      filteredErrorRate_inner = errorRate_inner;
      D_inner = Kd_inner * filteredErrorRate_inner;
    }
    
    float control_inner = P_inner + I_inner + D_inner;
    control_inner = constrain(control_inner, -40, 40);

    // Update PWM
    currentPWM += control_inner;
    currentPWM = constrain(currentPWM, 0, 255);

    // Apply PWM to pump
    ledcWrite(PUMP_PWM_PIN, currentPWM);

    lastError_inner = error_inner;
    lastInnerSampleTime = currentTime;
  }
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SD CARD READ TASKS - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SENSOR READ TASKS - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void IRAM_ATTR pulseCounter() {
  unsigned long currentTime = micros();
  if (currentTime - lastPulseTime > 1000) { // Minimum 1 ms between pulses
    pulseCount++;
    lastPulseTime = currentTime;
  }
}

void setup() {
  Serial.begin(115200);                                                          // inisiasi serial dan bluetooth
  SerialBT.begin("ESP32_Pump_Controller"); // Bluetooth device name
  Serial.println("ESP32 Bluetooth Ready. Waiting for connection...");

  pinMode(sensorPin, INPUT_PULLUP);                                              // inisiasi flowrate sensor
  attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, RISING);

  SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
  initializeSDCard();
  availableModes = getAvailableModesFromSD();
  loadParametersFromSD(); 

  pump_setup();
}

void loop() {
  handleBluetoothReceive();                         // handle permintaan user dari bluetooth (read/write sd card dan start/stop kendali pompa)
  handleSerialInput();                              // hanya untuk serial debugging
  if (millis() - lastSendTime >= SEND_INTERVAL) {   // bluetooth comm ke app yang harus terus-terusan dilakukan, mengirim data sensor flowarate dan pressure
    sendActualParameters();
    lastSendTime = millis();
  }
  updateActualValues();                             // panggil pembacaan sensor flowrate dan pressure
  controlPump();                                    // kendali pompa hanya dijalankan ketika user meminta dari bluetooth
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// HANDLE BLUETOOTH TASKS - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
      if (newStartPump == 1) {
        integral_outer = 0;
        integral_inner = 0;
        cycleStartTime = millis();
        Serial.println("Pump STARTED - Control system reset");
      } else {
        Serial.println("Pump STOPPED");
      }
      
      sendStartStopConfirmation(newStartPump);
      
      Serial.println("Pump start/stop changed to: " + String(newStartPump));
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// part bluetooh handle yang berhubungann dengan task sensor read - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void updateActualValues() {
  // Update actual values
  updateFlowRate();
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate > 50) { 
    float pressureKPa = readPressureFromADS();
    pumpParams.pressureActual = (int)generateBloodPressureWaveform();
    lastUpdate = millis();
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// part bluetooh handle yang berhubungann dengan task control - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void controlPump() {
  if (pumpParams.startPump == 1) {
    pump_loop();  // pump is started
  } else {
    ledcWrite(PUMP_PWM_PIN, 0);
    currentPWM = 0;
    integral_outer = 0;
    integral_inner = 0;
    cycleStartTime = millis();
  }
}

void resetControlSystem() {
  integral_outer = 0;
  integral_inner = 0;
  lastError_outer = 0;
  lastError_inner = 0;
  filteredErrorRate_outer = 0;
  filteredErrorRate_inner = 0;
  cycleStartTime = millis();
  currentPWM = 0;
  ledcWrite(PUMP_PWM_PIN, 0);
  
  float currentPressure = readPressureFromADS();
  x = currentPressure;
  p = 1.0;
  
  Serial.println("Control system reset");
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SD Card Integration Functions - TO BE MOVED TO CORE 0

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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