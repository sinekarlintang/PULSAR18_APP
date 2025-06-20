#include "tasks.h"

extern unsigned long lastSendTime;
extern const unsigned long SEND_INTERVAL;
extern void sendActualParameters();

void TaskSendActual(void *pv) {
  for (;;) {
    unsigned long now = millis();
    if (now - lastSendTime >= SEND_INTERVAL) {
      sendActualParameters();
      lastSendTime = now;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}