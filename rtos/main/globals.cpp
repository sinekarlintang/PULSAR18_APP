// File: globals.cpp
#include "tasks.h"

// --- Global Variable Definitions ---

// Bluetooth
BluetoothSerial SerialBT;

// Hardware objects
Adafruit_ADS1115 ads;         // Address: 0x48 (ADDR to GND)

// Pressure sensor calibration
float VD_RATIO = 0.6687;
float VOUT_MIN = 0.992;
float VOUT_MAX = 2.8481;
const float P_MIN = 0.0;
const float P_MAX = 750.062;
float SCALE = 200; //  per volt

// Flow sensor
const int sensorPin = 4;
volatile int pulseCount = 0;
float flowRate = 0.0;
unsigned long lastFlowTime = 0;
float calibrationFactor = 13.5;
float flowRateArray[5] = {0};
int flowIndex = 0;
volatile unsigned long lastPulseTime = 0;

// PID Outer Loop (Pressure)
float Kp_outer = 10;
float Ki_outer = 10;
float Kd_outer = 1;
float N_outer = 100;
float setpointPressure = 0;
float integral_outer = 0;
float lastError_outer = 0;
float filteredErrorRate_outer = 0;
const float deadZone_outer = 0.5;

// PID Inner Loop (PWM/Speed)
float Kp_inner = 1.0;
float Ki_inner = 0;
float Kd_inner = 0;
float integral_inner = 0;
float lastError_inner = 0;
float filteredErrorRate_inner = 0;
float setpointPWM = 0;

// Pump Control State
uint8_t currentPWM = 0;

// Waveform Generation
unsigned long cycleStartTime = 0;
float cycleTime = 0;
float systolicDuration = 0;
float diastolicDuration = 0;
float sysPeak = 0;

// Kalman Filter for Pressure Smoothing
float x = 0;
float p = 1.0;
float r = 1;
float q = 0.002;

// Sampling Timing
const unsigned long sampleInterval = 10;
const unsigned long innerSampleInterval = 1;
unsigned long lastSampleTime = 0;
unsigned long lastInnerSampleTime = 0;

// Main pump parameters
PumpParameters pumpParams;

// Bluetooth Message Buffers
String receivedData = "";
unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 50;

// Mode management
std::vector<String> availableModes;

// Serial Debug
String serialInputBuffer = "";

// FreeRTOS handles
QueueHandle_t sdQueue;
SemaphoreHandle_t sdMutex;