#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include "cmsis_os2.h"
#include "main.h"

#include "SUSI.h"
#include "stm32h5xx_hal_def.h"
#include "stm32h5xx_hal_spi.h"

static SPI_HandleTypeDef *hSlaveSPI;
static osThreadId_t susiThread_id;
static osSemaphoreId_t susiStart_sem;
static bool susiRunning = false;

/* Definitions for susiTask */
static const osThreadAttr_t susiTask_attributes = {
  .name = "susiTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};

uint8_t rxBuffer[3];
uint8_t txBuffer[5];

static osEventFlagsId_t spiRxEvent;

#define SPI_RX_STAGE3_FLAG  (1 << 0)
#define EXTENDED_PACKET_PATTERN   0x70
#define EXTENDED_PACKET_MASK      0xF0

void SUSI_S_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  (void)hspi;  // Unused parameter

  osEventFlagsSet(spiRxEvent, SPI_RX_STAGE3_FLAG);
}


HAL_StatusTypeDef spi_conditional_rx(void) {
    osEventFlagsClear(spiRxEvent, SPI_RX_STAGE3_FLAG);
    HAL_SPI_DeInit(hSlaveSPI);
    HAL_SPI_Init(hSlaveSPI);
    rxBuffer[0] = 0;  // Clear the first byte
    rxBuffer[1] = 0;  // Clear the second byte
    rxBuffer[2] = 0;  // Clear the third byte 

    SUSI_Master_Start();
    uint32_t flags;
    while (true) {
      HAL_StatusTypeDef status = HAL_SPI_Receive_IT(hSlaveSPI, rxBuffer, 3);
      if (status != HAL_OK)
        return status;
      flags = osEventFlagsWait(spiRxEvent, SPI_RX_STAGE3_FLAG, osFlagsWaitAny, PACKET_TIMEOUT_MS);
      if (flags == osErrorTimeout)
        break;
    }

    return HAL_OK;
}


/**
 * @brief SUSI Slave Thread
 * 
 * This thread waits for the semaphore to be released, indicating that the SUSI slave should start.
 * It runs in a loop until the SUSI slave is stopped, executing the necessary operations.
 */
void SUSI_SlaveThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(susiStart_sem, osWaitForever);
    
    susiRunning = true;

    spiRxEvent = osEventFlagsNew(NULL);
    while (susiRunning) {
      if (spi_conditional_rx() == HAL_OK) {
        // Process received data
        switch (rxBuffer[0]) {
          case SUSI_NOOP:
            
            break;
          case SUSI_FG1:
            printf("SUSI_FG1 received: 0x%02X\r\n", rxBuffer[1]);

            break;
          case SUSI_FG2:
 
            break;
          default:
            // Handle unexpected or extended packet type
            if ((rxBuffer[0] & EXTENDED_PACKET_MASK) == EXTENDED_PACKET_PATTERN) {
              printf("Extended packet received: 0x%02X 0x%02X\r\n", rxBuffer[1], rxBuffer[2]);
            }
            else {
              printf("Unexpected packet received: 0x%02X\r\n", rxBuffer[0]);
            }
            break;
        }
      }
      else {
        printf("SPI receive error or timeout\r\n");
      }
//      osDelay(100u);
    }
    
    osSemaphoreRelease(susiStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }
}



void SUSI_Slave_Init(SPI_HandleTypeDef *hspi) {
  hSlaveSPI = hspi;
  susiStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
  susiThread_id = osThreadNew(SUSI_SlaveThread, NULL, &susiTask_attributes);
}

void SUSI_Slave_Start(void) {
  if (!susiRunning) {
    susiRunning = true;
    osSemaphoreRelease(susiStart_sem);
  }
}

void SUSI_Slave_Stop(void) {
  if (susiRunning) {
    susiRunning = false;
    osSemaphoreRelease(susiStart_sem);
  }
}
