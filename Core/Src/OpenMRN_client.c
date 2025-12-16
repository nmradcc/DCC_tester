#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "cmsis_os2.h"
#include "main.h"

#include "stm32h5xx_hal_fdcan.h"

static FDCAN_HandleTypeDef *hOpenMRNCAN;
static osThreadId_t openMRNThread_id;
static osSemaphoreId_t openMRNStart_sem;
static volatile bool openMRNRunning = false;

/* Definitions for openMRNTask */
static const osThreadAttr_t openMRNTask_attributes = {
  .name = "openMRNTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};

#if 0
#define OPENMRN_RX_FIFO_SIZE  64

static FDCAN_RxHeaderTypeDef rxHeader;
static uint8_t rxData[8];
static osEventFlagsId_t canRxEvent;
#define CAN_RX_MSG_FLAG  (1 << 0)

/**
 * @brief CAN RX FIFO 0 callback
 * 
 * Called when a message is received in FIFO 0
 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs) {
  if (hfdcan == hOpenMRNCAN) {
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
      osEventFlagsSet(canRxEvent, CAN_RX_MSG_FLAG);
    }
  }
}

/**
 * @brief Process received CAN message
 * 
 * @return HAL_OK if message received and processed, HAL_TIMEOUT if no message
 */
HAL_StatusTypeDef can_receive_message(void) {
  osEventFlagsClear(canRxEvent, CAN_RX_MSG_FLAG);
  
  // Get message from FIFO 0
  HAL_StatusTypeDef status = HAL_FDCAN_GetRxMessage(hOpenMRNCAN, FDCAN_RX_FIFO0, &rxHeader, rxData);
  
  if (status != HAL_OK) {
    return status;
  }
  
  // Process the received message
  printf("OpenMRN CAN Message - ID: 0x%X, DLC: %d, Data: ", rxHeader.Identifier, rxHeader.DataLength);
  for (uint8_t i = 0; i < rxHeader.DataLength; i++) {
    printf("%02X ", rxData[i]);
  }
  printf("\r\n");
  
  return HAL_OK;
}
#endif
/**
 * @brief OpenMRN Client Thread
 * 
 * This thread waits for the semaphore to be released, indicating that the OpenMRN client should start.
 * It runs in a loop until the OpenMRN client is stopped, receiving and processing CAN messages.
 */
void OpenMRN_ClientThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(openMRNStart_sem, osWaitForever);
    
    openMRNRunning = true;

    #if 0
    canRxEvent = osEventFlagsNew(NULL);
    
    // Enable CAN interrupt for RX FIFO 0
    HAL_FDCAN_ActivateNotification(hOpenMRNCAN, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    #endif
    while (openMRNRunning) {
    #if 0
      // Wait for CAN message or timeout
      uint32_t flags = osEventFlagsWait(canRxEvent, CAN_RX_MSG_FLAG, osFlagsWaitAny, osWaitForever);
      
      if (flags & CAN_RX_MSG_FLAG) {
        can_receive_message();
      }
    #endif
      osDelay(10u); // Placeholder delay to prevent tight loop
    }
    
    // Disable CAN interrupt
    HAL_FDCAN_DeactivateNotification(hOpenMRNCAN, FDCAN_IT_RX_FIFO0_NEW_MESSAGE);
    
    osSemaphoreRelease(openMRNStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }
}

/**
 * @brief Initialize OpenMRN Client
 * 
 * @param hfdcan Pointer to FDCAN_HandleTypeDef
 */
void OpenMRN_Client_Init(FDCAN_HandleTypeDef *hfdcan) {
  hOpenMRNCAN = hfdcan;
  openMRNStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
  openMRNThread_id = osThreadNew(OpenMRN_ClientThread, NULL, &openMRNTask_attributes);
}

/**
 * @brief Start OpenMRN Client
 */
void OpenMRN_Client_Start(void) {
  if (!openMRNRunning) {
    openMRNRunning = true;
    osSemaphoreRelease(openMRNStart_sem);
  }
}

/**
 * @brief Stop OpenMRN Client
 */
void OpenMRN_Client_Stop(void) {
  if (openMRNRunning) {
    openMRNRunning = false;
    osSemaphoreAcquire(openMRNStart_sem, osWaitForever);
  }
}

/**
 * @brief Send OpenMRN CAN message
 * 
 * @param arbitration_id CAN identifier
 * @param data Pointer to data buffer
 * @param dlc Data length code (0-8)
 * @return HAL_OK if successful
 */
HAL_StatusTypeDef OpenMRN_Client_SendMessage(uint32_t arbitration_id, const uint8_t *data, uint8_t dlc) {
  if (dlc > 8) return HAL_ERROR;
  
  FDCAN_TxHeaderTypeDef txHeader = {
    .Identifier = arbitration_id,
    .IdType = FDCAN_STANDARD_ID,
    .TxFrameType = FDCAN_DATA_FRAME,
    .DataLength = dlc,
    .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
    .BitRateSwitch = FDCAN_BRS_OFF,
    .FDFormat = FDCAN_CLASSIC_CAN,
    .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
  };
  
  uint8_t txData[8];
  memcpy(txData, data, dlc);
  
  return HAL_FDCAN_AddMessageToTxFifoQ(hOpenMRNCAN, &txHeader, txData);
}
