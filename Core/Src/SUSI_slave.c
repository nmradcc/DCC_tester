#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "cmsis_os2.h"
#include "main.h"

#include "SUSI.h"
#include "stm32h5xx_hal_def.h"
#include "stm32h5xx_hal_spi.h"

static osThreadId_t susiThread_id;
static osSemaphoreId_t susiStart_sem;
static bool susiRunning = false;

/* Definitions for susiTask */
static const osThreadAttr_t susiTask_attributes = {
  .name = "susiTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};

typedef struct {
    SPI_HandleTypeDef *hspi;
    uint8_t rxBuffer[64];
    uint8_t txBuffer[64];
} SUSI_Slave;

static osEventFlagsId_t spiRxEvent;

#define SPI_RX_STAGE1_FLAG  (1 << 0)
#define SPI_RX_STAGE2_FLAG  (1 << 1)
#define EXTENDED_PACKET_PATTERN   0x70
#define EXTENDED_PACKET_MASK      0xF0


HAL_StatusTypeDef spi_conditional_rx(SPI_HandleTypeDef *hspi, uint8_t *rxBuf, uint32_t timeout) {
    osEventFlagsClear(spiRxEvent, SPI_RX_STAGE1_FLAG | SPI_RX_STAGE2_FLAG);
    HAL_SPI_DeInit(hspi);
    HAL_SPI_Init(hspi);

    // Stage 1: Receive first byte
    HAL_StatusTypeDef status = HAL_SPI_Receive_IT(hspi, rxBuf, 2);
    if (status != HAL_OK)
      return status;

    // Wait for stage 1 completion
    uint32_t flags = osEventFlagsWait(spiRxEvent, SPI_RX_STAGE1_FLAG, osFlagsWaitAny, 10000);
    if (!(flags & SPI_RX_STAGE1_FLAG)) 
      return HAL_TIMEOUT;

    // Decide whether to receive third byte
    if ((rxBuf[0] & EXTENDED_PACKET_MASK) == EXTENDED_PACKET_PATTERN)
    {
        // Stage 2: Receive third byte
        status = HAL_SPI_Receive_IT(hspi, &rxBuf[2], 1);
        if (status != HAL_OK)
          return status;

        flags = osEventFlagsWait(spiRxEvent, SPI_RX_STAGE2_FLAG, osFlagsWaitAny, timeout);
        if (!(flags & SPI_RX_STAGE2_FLAG))
          return HAL_TIMEOUT;
    }

    return HAL_OK;
}


void SUSI_S_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
  (void)hspi;  // Unused parameter
  static bool stage1Complete = false;

  if (!stage1Complete) {
      stage1Complete = true;
      osEventFlagsSet(spiRxEvent, SPI_RX_STAGE1_FLAG);
  } else {
      stage1Complete = false;
      osEventFlagsSet(spiRxEvent, SPI_RX_STAGE2_FLAG);
      }
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
    
    // Initialize SPI SUSI Slave
    SUSI_Slave susi;
    susi.hspi = &hspi2;  // Assign the SPI handle

    susiRunning = true;

    spiRxEvent = osEventFlagsNew(NULL);
    while (susiRunning) {
      if (spi_conditional_rx(susi.hspi, susi.rxBuffer, PACKET_TIMEOUT_MS) == HAL_OK) {
        // Process received data
        switch (susi.rxBuffer[0]) {
          case SUSI_FG1:
            printf("SUSI_FG1 received: 0x%02X\r\n", susi.rxBuffer[1]);

            break;
          case SUSI_FG2:
 
            break;
          default:
            // Handle unexpected or extended packet type
            if ((susi.rxBuffer[0] & EXTENDED_PACKET_MASK) == EXTENDED_PACKET_PATTERN) {
              printf("Extended packet received: 0x%02X 0x%02X\r\n", susi.rxBuffer[1], susi.rxBuffer[2]);
            }
            else {
              printf("Unexpected packet received: 0x%02X\r\n", susi.rxBuffer[0]);
            }
            break;
        }
      }
//      osDelay(100u);
    }
    
    osSemaphoreRelease(susiStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }
}



void SUSI_Slave_Init(void) {
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
