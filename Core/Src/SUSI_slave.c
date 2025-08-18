#include <stdbool.h>
#include <stdio.h>
#include "cmsis_os2.h"
#include "main.h"

#include "SUSI.h"
#include "stm32h5xx_hal_spi.h"

static SPI_HandleTypeDef *hSlaveSPI;
static osThreadId_t susiThread_id;
static osSemaphoreId_t susiStart_sem;
static volatile bool susiRunning = false;

/* Definitions for susiTask */
static const osThreadAttr_t susiTask_attributes = {
  .name = "susiTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};

uint8_t rxBuffer[3];
uint8_t txBuffer[5];

static osEventFlagsId_t spiRxEvent;
#define SPI_RX_3BYTES_FLAG  (1 << 0)

void SUSI_S_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  (void)hspi;  // Unused parameter

  osEventFlagsSet(spiRxEvent, SPI_RX_3BYTES_FLAG);
}


HAL_StatusTypeDef spi_conditional_rx(void) {
    osEventFlagsClear(spiRxEvent, SPI_RX_3BYTES_FLAG);
    HAL_SPI_DeInit(hSlaveSPI);
    HAL_SPI_Init(hSlaveSPI);
    rxBuffer[0] = 0;  // Clear the first byte
    rxBuffer[1] = 0;  // Clear the second byte
    rxBuffer[2] = 0;  // Clear the third byte 

    HAL_StatusTypeDef status = HAL_SPI_Receive_IT(hSlaveSPI, rxBuffer, 3);
    if (status != HAL_OK)
      return status;
    osEventFlagsWait(spiRxEvent, SPI_RX_3BYTES_FLAG, osFlagsWaitAny, PACKET_TIMEOUT_MS);
    if ((rxBuffer[0] == 0) && (rxBuffer[1] == 0) && (rxBuffer[2] == 0))
      return HAL_TIMEOUT;  // No data received within timeout
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
          case SUSI_FG1:
            printf("SUSI_FG1 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG2:
            printf("SUSI_FG2 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG3:
            printf("SUSI_FG3 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG4:
            printf("SUSI_FG4 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG5:
            printf("SUSI_FG5 received: 0x%02X\r\n", rxBuffer[1]);
            break; 
          case SUSI_FG6: 
            printf("SUSI_FG6 received: 0x%02X\r\n", rxBuffer[1]);
            break;  
          case SUSI_FG7:
            printf("SUSI_FG7 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG8:
            printf("SUSI_FG8 received: 0x%02X\r\n", rxBuffer[1]);
            break;
          case SUSI_FG9:
            printf("SUSI_FG9 received: 0x%02X\r\n", rxBuffer[1]);
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
    osSemaphoreAcquire(susiStart_sem, osWaitForever);
  }
}
