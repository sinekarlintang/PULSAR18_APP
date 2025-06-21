#include "tasks.h"

extern PumpParameters pumpParams;
extern const unsigned long sampleInterval;
extern unsigned long lastSampleTime;
extern float setpointPressure;
extern float generateBloodPressureWaveform();
void controlPump();
void pump_loop();
void resetControlSystem();

void TaskControl(void *pv) {
  for (;;) {
    if (pumpParams.startPump) {
      unsigned long now = millis();
      if (now - lastSampleTime >= sampleInterval) {
        float bpm = pumpParams.heartRate;
        cycleTime = 60.0 / bpm;
        systolicDuration = pumpParams.systolicPeriod / 100.0 * cycleTime;
        diastolicDuration = pumpParams.diastolicPeriod / 100.0 * cycleTime;
        sysPeak = pumpParams.systolicPeakTime / 1000.0;
        pump_loop();
        lastSampleTime = now;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void pump_loop() {  
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

    // Update actual pressure for monitoring 
    pumpParams.pressureActual = (int)(x);

    // Dummy
    // pumpParams.pressureActual = (int)setpointPressure;

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
    
          // Output for monitoring (tab-separated for Serial Plotter)
    Serial.print(setpointPressure, 2);
    Serial.print(',');
    Serial.print(x, 2); // Filtered pressure
    Serial.print(',');
    Serial.print(setpointPWM);
    Serial.print(',');
    Serial.print(currentPWM);
    Serial.println();
  }
}

void controlPump() {
  // pump_loop();  
  if (pumpParams.startPump == 0) {
    resetControlSystem();
    setpointPressure = 0;
    ledcWrite(PUMP_PWM_PIN, 0);
    currentPWM = 0;
    integral_outer = 0;
    integral_inner = 0;
    cycleStartTime = millis();
  } else {
    pump_loop();
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