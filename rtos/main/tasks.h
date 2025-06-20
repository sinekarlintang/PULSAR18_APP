#ifndef TASKS_H
#define TASKS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <vector>
#include "BluetoothSerial.h"
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

// --- SD Card Pin Definitions ---
#define SD_CS     5
#define MOSI_PIN  25
#define MISO_PIN  19
#define CLK_PIN   18

// --- Pump Control PWM ---
#define PUMP_PWM_PIN     2
#define PWM_FREQ         30000
#define PWM_RESOLUTION   8

// --- ADS1115 Configuration ---
const uint8_t ADS_CHANNEL = 0; // A0 input

// --- Pressure Sensor Parameters ---
const float SENSOR_SUPPLY = 4.75;
const float SENSOR_VMIN = 0.45;
const float SENSOR_VMAX = 4.05;

// --- Pump Parameters Structure ---
struct PumpParameters {
  int heartRate = 80;
  int systolicPressure = 120;
  int diastolicPressure = 80;
  int systolicPeriod = 60;
  int diastolicPeriod = 40;
  int notchPressure = 60;
  int systolicPeakTime = 600;
  int diastolicPeakTime = 300;
  int flowRate = 80;
  int pressureActual = 80;
  String pumpMode = "Otomatis";
  int startPump = 0;
  int basePressure = 80;
};

// Structure for SD card command
struct SDCommand {
  String type;            // "LOAD", "SAVE", "ADD", "DEL"
  String mode;            // Mode name
  PumpParameters data;    // Optional data for SAVE/ADD
};

// Task function prototypes
void TaskBluetooth(void *pv);
void TaskSerialDebug(void *pv);
void TaskSensor(void *pv);
void TaskWaveform(void *pv);
void TaskControl(void *pv);
void TaskSDWorker(void *pv);
void TaskSendActual(void *pv);

// Function prototypes
void pulseCounter();
void initializeSDCard();
std::vector<String> getAvailableModesFromSD();
bool loadParametersFromSD(String modeName);
void pump_setup();

// --- Global Variables (extern declarations) ---
// Bluetooth
extern BluetoothSerial SerialBT;

// Hardware objects
extern Adafruit_ADS1115 ads;

// Pressure sensor calibration
extern float VD_RATIO;
extern float VOUT_MIN;
extern float VOUT_MAX;
extern const float P_MIN;
extern const float P_MAX;
extern float SCALE;

// Flow sensor
extern const int sensorPin;
extern volatile int pulseCount;
extern float flowRate;
extern unsigned long lastFlowTime;
extern float calibrationFactor;
extern float flowRateArray[5];
extern int flowIndex;
extern volatile unsigned long lastPulseTime;

// PID Outer Loop (Pressure)
extern float Kp_outer;
extern float Ki_outer;
extern float Kd_outer;
extern float N_outer;
extern float setpointPressure;
extern float integral_outer;
extern float lastError_outer;
extern float filteredErrorRate_outer;
extern const float deadZone_outer;

// PID Inner Loop (PWM/Speed)
extern float Kp_inner;
extern float Ki_inner;
extern float Kd_inner;
extern float integral_inner;
extern float lastError_inner;
extern float filteredErrorRate_inner;
extern float setpointPWM;

// Pump Control State
extern uint8_t currentPWM;

// Waveform Generation
extern unsigned long cycleStartTime;
extern float cycleTime;
extern float systolicDuration;
extern float diastolicDuration;
extern float sysPeak;

// Kalman Filter
extern float x;
extern float p;
extern float r;
extern float q;

// Sampling Timing
extern const unsigned long sampleInterval;
extern const unsigned long innerSampleInterval;
extern unsigned long lastSampleTime;
extern unsigned long lastInnerSampleTime;

// Main pump parameters
extern PumpParameters pumpParams;

// Bluetooth Message Buffers
extern String receivedData;
extern unsigned long lastSendTime;
extern const unsigned long SEND_INTERVAL;

// Mode management
extern std::vector<String> availableModes;

// Serial Debug
extern String serialInputBuffer;

// Shared handles
extern QueueHandle_t sdQueue;
extern SemaphoreHandle_t sdMutex;

#endif