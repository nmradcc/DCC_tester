#include <stdbool.h>
#include "cmsis_os2.h"
#include "fx_stm32_sd_driver.h"
#include "main.h"

#include "SUSI.h"
#include "stm32h5xx_hal_spi.h"

static SPI_HandleTypeDef *hMasterSPI;
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
  (void)argument;

  static uint8_t pData[3] = {0x60,0x10,0xAA};  // Example function packet data
  static uint8_t pExData[3] = {0x71,0xA5,0x5A};  // Example extended packet data

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(susiStart_sem, osWaitForever);
    
    susiRunning = true;

    while (susiRunning) {
      HAL_SPI_Transmit(hMasterSPI, (const uint8_t *)&pData[0], 2, HAL_MAX_DELAY);
//      osDelay(500u);
HAL_Delay(100);
//      HAL_SPI_Transmit(hMasterSPI, (const uint8_t *)&pExData[0], 3, HAL_MAX_DELAY);
//      osDelay(500u);
    }
    
    osSemaphoreRelease(susiStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }
}




void SUSI_Master_Init(SPI_HandleTypeDef *hspi) {
  hMasterSPI = hspi;
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


