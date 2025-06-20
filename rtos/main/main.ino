#include "tasks.h"

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_Pump_Controller");
  Serial.println("ESP32 Bluetooth Ready. Waiting for connection...");

  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), pulseCounter, RISING);

  SPI.begin(CLK_PIN, MISO_PIN, MOSI_PIN, SD_CS);
  initializeSDCard();
  availableModes = getAvailableModesFromSD();
  loadParametersFromSD("Otomatis"); 

  pump_setup();

  // Queue & Semaphore init
  sdQueue = xQueueCreate(5, sizeof(SDCommand));
  sdMutex = xSemaphoreCreateMutex();

  // Task init
  xTaskCreatePinnedToCore(TaskBluetooth, "BT", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(TaskSerialDebug, "Serial", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskSensor, "Sensor", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskWaveform, "Waveform", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(TaskControl, "Control", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(TaskSDWorker, "SDWorker", 4096, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(TaskSendActual, "SendActual", 4096, NULL, 1, NULL, 0);
}

void loop() {
  vTaskDelete(NULL); // Not used in FreeRTOS
}
