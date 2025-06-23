#include "tasks.h"

void setup() {
  Serial.begin(115200);
  SerialBT.begin("PULSAR18_V1");
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
  // kalau ingin matikan bluetooth dan sd card, hanya jalankan kontrol, comment ini
  // xTaskCreatePinnedToCore(TaskSDWorker, "SDWorker", 4096, NULL, 1, NULL, 0);      // Sd card works
  // xTaskCreatePinnedToCore(TaskSendActual, "SendActual", 4096, NULL, 1, NULL, 0);  // ngirim data sensor ke app terus terusan
  // xTaskCreatePinnedToCore(TaskBluetooth, "BT", 4096, NULL, 2, NULL, 0);           // bluetooth handling

  xTaskCreatePinnedToCore(TaskSerialDebug, "Serial", 4096, NULL, 1, NULL, 0);     // ini klo mau ganti kp ki kd / start stop lewat serial
  // xTaskCreatePinnedToCore(TaskSensor, "Sensor", 4096, NULL, 2, NULL, 0);          // task ini untuk baca flowrate saja

  xTaskCreatePinnedToCore(TaskControl, "Control", 4096, NULL, 3, NULL, 1);        // baca pressure sensor and control


  // task waveform gajadi dipakai (uda digabung ke task control) tapi file 'task_waveform.ino' jangan dihapus!!! malas ngedit lagi.  
  // al, kata gue lu lanjut variable kp ki kd langsung di file integrasi ini kalo ga gue nangis integrasiinnya lagi.
  // edit di task_control.ino, pake aja pumpParams.heartRate (kalo mau varying berdasarkan BPM) atau variable lain di struct itu,
  // pastiin variabel yang diganti uda ditulis di global            -sekar

  // to do: varying pid constants, open loop control
}

void loop() {
  vTaskDelete(NULL); 
}
