// pumpControl.ino
// Control PWM for Pressure with Cascade PID
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

#define PUMP_PWM_PIN     2
#define PWM_FREQ         30000
#define PWM_RESOLUTION   8

uint8_t currentPWM = 0;

// --- ADS1115 Configuration ---
Adafruit_ADS1115 ads;  // Use default address 0x48 (ADDR to GND)
const uint8_t ADS_CHANNEL = 0;  // A0 pin on ADS1115

// --- Konfigurasi Pressure Sensor ---
const float SENSOR_SUPPLY = 4.7;     // Actual sensor supply voltage
const float SENSOR_VMIN = 0.47;      // Sensor minimum output (V)
const float SENSOR_VMAX = 4.23;      // Sensor maximum output (V)
float VD_RATIO = 0.6732;             // Voltage divider ratio
float VOUT_MIN = SENSOR_VMIN * VD_RATIO;  // ~0.317V
float VOUT_MAX = SENSOR_VMAX * VD_RATIO;  // ~2.857V
const float P_MIN = 0.0;             // Minimum pressure (mmHg)
const float P_MAX = 750.062;           // Maximum pressure (mmHg)
float SCALE = (P_MAX - P_MIN) / (VOUT_MAX - VOUT_MIN);  // kPa per volt

// --- Konfigurasi Outer PID (Pressure) ---
float Kp_outer = 100;    // Proportional gain
float Ki_outer = 100;    // Integral gain
float Kd_outer = 10;   // Derivative gain
float N_outer = 100;    // Derivative filter coefficient
float setpointPressure = 0; // Current setpoint (pressure in kPa)
float integral_outer = 0;
float lastError_outer = 0;
float filteredErrorRate_outer = 0; // Filtered derivative term
const float deadZone_outer = 0.1;  // Dead zone

// --- Konfigurasi Inner PID (PWM/Speed) ---
float Kp_inner = 1.0;    // Proportional gain for inner loop
float Ki_inner = 0;      // Integral gain for inner loop
float Kd_inner = 0;      // Derivative gain for inner loop
float integral_inner = 0;
float lastError_inner = 0;
float filteredErrorRate_inner = 0;
float setpointPWM = 0;   // PWM setpoint from outer loop

// --- Konfigurasi Blood Pressure Waveform ---
float systolicPressure = 120;    // kPa (converted from mmHg: ~120mmHg)
float diastolicPressure = 110;   // kPa (converted from mmHg: ~105mmHg)  
float notchPressure = 108;     // kPa (dicrotic notch pressure)
float basePressure = 80;   // kPa (baseline pressure)
float systolicPeriod = 50.0;    // Percentage of cycle (40%)
float systolicPeakTime = 600.0; // Per mille (60% of systolic period)
float heartRate = 90.0;         // BPM

// --- Waveform State Variables ---
unsigned long cycleStartTime = 0;
float cycleTime = 0;              // seconds per cycle
float systolicDuration = 0;       // duration of systolic phase
float diastolicDuration = 0;      // duration of diastolic phase
float sysPeak = 0;                // normalized systolic peak time

// --- Konfigurasi Kalman Filter (for Pressure) ---
float x = 0;      // Filtered pressure estimate
float p = 1.0;    // Error covariance
float r = 0.1;      // Measurement noise
float q = 0.003;  // Process noise

// --- Sampling Configuration ---
const unsigned long sampleInterval = 10;      // 10ms for outer loop
const unsigned long innerSampleInterval = 1;  // 1ms for inner loop
unsigned long lastSampleTime = 0;
unsigned long lastInnerSampleTime = 0;

// --- ADS1115 Reading Function ---
float readPressureFromADS() {
  // Read from ADS1115 channel A0
  int16_t adcValue = ads.readADC_SingleEnded(ADS_CHANNEL);
  
  // Convert to voltage
  // ADS1115 with gain 1 has range of ±4.096V
  // Resolution is 4.096V / 32768 = 0.125mV per bit
  float voltage = adcValue * 0.000125; // Convert to volts
  
  // Calculate pressure from voltage
  float pressure = (SCALE * voltage);
  
  return pressure;
}

// --- Waveform Calculation Functions ---
void updateWaveformParameters() {
  cycleTime = 60.0 / heartRate; // seconds per cycle
  systolicDuration = (systolicPeriod / 100.0) * cycleTime;
  diastolicDuration = cycleTime - systolicDuration;
  sysPeak = (systolicPeakTime / 1000.0); // convert from per mille to fraction
}

float calculateSystolicPressure(float progress, float sysPeak) {
  float tPeak = sysPeak;
  if (progress <= tPeak) {
    // Rising: basePressure → systolicPressure (half cosine wave)
    float rise = progress / tPeak; // 0 … 1
    float factor = (1.0 - cos(rise * PI)) / 2.0; // 0 … 1
    return basePressure + (systolicPressure - basePressure) * factor;
  } else {
    // Falling: systolicPressure → notchPressure (half cosine wave)
    float fall = (progress - tPeak) / (1.0 - tPeak); // 0 … 1
    float factor = (1.0 + cos(fall * PI)) / 2.0;
    return notchPressure + (systolicPressure - notchPressure) * factor;
  }
}

float calculateDiastolicPressure(float progress, float diastolicDuration) {
  float tPeak = 0.3; // Fixed at 30% of diastolic period
  if (progress <= tPeak) {
    // Rising: notchPressure → diastolicPressure (half cosine wave)
    float rise = progress / tPeak; // 0 … 1
    float factor = (1.0 - cos(rise * PI)) / 2.0; // 0 … 1
    return notchPressure + (diastolicPressure - notchPressure) * factor;
  } else {
    // Falling: diastolicPressure → basePressure (half cosine wave)
    float fall = (progress - tPeak) / (1.0 - tPeak); // 0 … 1
    float factor = (1.0 + cos(fall * PI)) / 2.0; // 1 … 0
    return basePressure + (diastolicPressure - basePressure) * factor;
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

// --- Update Parameters ---
void updateParameter(String param, float value) {
  param.toLowerCase();
  if (param == "kp") {
    Kp_outer = value;
  } else if (param == "ki") {
    Ki_outer = value;
  } else if (param == "kd") {
    Kd_outer = value;
  } else if (param == "n") {
    N_outer = value;
  } else if (param == "kpi") {
    Kp_inner = value;
  } else if (param == "kii") {
    Ki_inner = value;
  } else if (param == "kdi") {
    Kd_inner = value;
  } else if (param == "r") {
    r = value;
  } else if (param == "q") {
    q = value;
  } else if (param == "vd") {
    VD_RATIO = value;
    VOUT_MIN = SENSOR_VMIN * VD_RATIO;
    VOUT_MAX = SENSOR_VMAX * VD_RATIO;
    SCALE = (P_MAX - P_MIN) / (VOUT_MAX - VOUT_MIN);
    Serial.println("Voltage divider ratio and pressure calculation updated");
    return;
  } else if (param == "sys") {
    systolicPressure = value;
    updateWaveformParameters();
  } else if (param == "dia") {
    diastolicPressure = value;
    updateWaveformParameters();
  } else if (param == "notch") {
    notchPressure = value;
    updateWaveformParameters();
  } else if (param == "base") {
    basePressure = value;
    updateWaveformParameters();
  } else if (param == "hr") {
    heartRate = value;
    updateWaveformParameters();
  } else if (param == "sysp") {
    systolicPeriod = value;
    updateWaveformParameters();
  } else if (param == "peak") {
    systolicPeakTime = value;
    updateWaveformParameters();
  } else if (param == "reset") {
    integral_outer = 0;
    integral_inner = 0;
    Serial.println("Integrators reset");
    return;
  }
  
  Serial.print("Updated ");
  Serial.print(param);
  Serial.print(" to ");
  Serial.println(value);
}

// --- Setup ---
void pump_setup() {
  delay(1000);
  Serial.println("=== Blood Pressure Cascade PID with ADS1115 ===");
  Serial.println("I2C: SDA=21, SCL=22, ADDR=GND (0x48)");
  Serial.println("Outer PID: 'kp <value>', 'ki <value>', 'kd <value>', 'n <value>'");
  Serial.println("Inner PID: 'kpi <value>', 'kii <value>', 'kdi <value>'");
  Serial.println("Kalman: 'r <value>', 'q <value>'");
  Serial.println("Waveform: 'sys <value>', 'dia <value>', 'notch <value>', 'base <value>'");
  Serial.println("         'hr <value>', 'sysp <value>', 'peak <value>'");
  Serial.println("Other: 'vd <ratio>', 'reset'");
  
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

  // Initialize waveform parameters
  updateWaveformParameters();
  cycleStartTime = millis();

  // Initialize Kalman filter with first reading
  delay(100);
  float initialPressure = 0;
  for(int i = 0; i < 10; i++) {
    initialPressure += readPressureFromADS();
    delay(10);
  }
  x = initialPressure / 10.0;
  
  Serial.println("System initialized. Blood pressure waveform generation started.");
  Serial.print("Initial pressure: "); Serial.print(x); Serial.println(" kPa");
  Serial.print("Cycle time: "); Serial.print(cycleTime); Serial.println(" seconds");
  Serial.print("Systolic duration: "); Serial.print(systolicDuration); Serial.println(" seconds");
  Serial.print("Diastolic duration: "); Serial.print(diastolicDuration); Serial.println(" seconds");
}

void pump_loop() {
  unsigned long currentTime = millis();

  // Generate blood pressure waveform setpoint
  setpointPressure = generateBloodPressureWaveform();

  // Handle Serial input for parameter adjustment
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    int spaceIndex = input.indexOf(' ');
    if (spaceIndex != -1) {
      String param = input.substring(0, spaceIndex);
      String valueStr = input.substring(spaceIndex + 1);
      float value = valueStr.toFloat();
      updateParameter(param, value);
    } else if (input == "reset") {
      updateParameter("reset", 0);
    } else {
      Serial.println("Format: <parameter> <value>");
    }
  }

  // Outer PID control loop (Pressure)
  if (currentTime - lastSampleTime >= sampleInterval) {
    // Read pressure from ADS1115
    float currentPressure = readPressureFromADS();

    // Kalman Filter for smoothing pressure
    p += q; // Prediction
    float k = p / (p + r); // Kalman Gain
    x += k * (currentPressure - x); // Update estimate
    p *= (1 - k); // Update error covariance

    // Outer PID: Calculate PWM setpoint
    float dt_outer = sampleInterval / 1000.0; // Convert to seconds
    float error_outer = setpointPressure - x; // Use filtered pressure
    float error = (setpointPressure/error_outer) * 100;

    float control_outer = 0;
    if (abs(error_outer) >= deadZone_outer) {
      float errorRate_outer = (error_outer - lastError_outer) / dt_outer;
      // Apply derivative filter
      filteredErrorRate_outer = (N_outer * errorRate_outer + filteredErrorRate_outer * (1.0 / (dt_outer * N_outer))) / (N_outer + 1.0 / dt_outer);
      integral_outer += error_outer * dt_outer;
      integral_outer = constrain(integral_outer, -100, 100); // Anti-windup

      control_outer = Kp_outer * error_outer + Ki_outer * integral_outer + Kd_outer * filteredErrorRate_outer;
      setpointPWM = constrain(control_outer, 0, 255); // PWM setpoint for inner loop
    } else {
      integral_outer = 0; // Reset integral in dead zone
      filteredErrorRate_outer = 0; // Reset filtered derivative
      setpointPWM = currentPWM; // Maintain current PWM
    }

    lastError_outer = error_outer;
    lastSampleTime = currentTime;

    // Output for monitoring (tab-separated for Serial Plotter)
    // Serial.print(setpointPressure, 2);
    // Serial.print('\t');
    // Serial.print(x, 2); // Filtered pressure
    // Serial.print('\t');
    // Serial.print(setpointPWM);
    // Serial.print('\t');
    // Serial.print(error);
    // Serial.println();
  }

  // Inner PID control loop (PWM/Speed)
  if (currentTime - lastInnerSampleTime >= innerSampleInterval) {
    float dt_inner = innerSampleInterval / 1000.0; // Convert to seconds
    float error_inner = setpointPWM - currentPWM;

    float control_inner = 0;
    float errorRate_inner = (error_inner - lastError_inner) / dt_inner;
    // Apply derivative filter (no filter coefficient for simplicity)
    filteredErrorRate_inner = errorRate_inner; // Can add filter if needed
    integral_inner += error_inner * dt_inner;
    integral_inner = constrain(integral_inner, -50, 50); // Anti-windup for inner loop

    control_inner = Kp_inner * error_inner + Ki_inner * integral_inner + Kd_inner * filteredErrorRate_inner;
    control_inner = constrain(control_inner, -30, 30); // Limit rate of change

    // Update PWM
    currentPWM += control_inner;
    currentPWM = constrain(currentPWM, 0, 255);

    // Apply PWM to pump (no inversion needed based on your setup)
    ledcWrite(PUMP_PWM_PIN, currentPWM);

    lastError_inner = error_inner;
    lastInnerSampleTime = currentTime;
  }
}