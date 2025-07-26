#include <stdbool.h>
#include "cmsis_os2.h"
#include "main.h"

#include "SUSI.h"

static osThreadId_t susiThread_id;
static osSemaphoreId_t susiStart_sem;
static bool susiRunning = false;

/* Definitions for susiTask */
static const osThreadAttr_t susiTask_attributes = {
  .name = "susiTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};


void SUSI_MasterThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(susiStart_sem, osWaitForever);
    
    // Initialize SPI SUSI Master
//    susi.init();

    susiRunning = true;

    while (susiRunning) {
//      susi.execute();
      osDelay(100u);
    }
    
    osSemaphoreRelease(susiStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }
}




void SUSI_Master_Init(void) {
  susiStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
  susiThread_id = osThreadNew(SUSI_MasterThread, NULL, &susiTask_attributes);
}

void SUSI_Master_Start(void) {
  if (!susiRunning) {
    susiRunning = true;
    osSemaphoreRelease(susiStart_sem);
  }
}

void SUSI_Master_Stop(void) {
  if (susiRunning) {
    susiRunning = false;
    osSemaphoreRelease(susiStart_sem);
  }
}


