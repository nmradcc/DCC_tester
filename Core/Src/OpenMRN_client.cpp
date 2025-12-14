#include <cstdio>
#include <cstring>
#include <atomic>
extern "C" {
#include "cmsis_os2.h"
#include "main.h"
#include "stm32h5xx_hal_fdcan.h"
}

#include "os/os.h"
#include "openlcb/SimpleStack.hxx"
#include "openlcb/SimpleNodeInfoMockUserFile.hxx"
#include "openlcb/EventHandlerTemplates.hxx"

static FDCAN_HandleTypeDef *hOpenMRNCAN;
static osThreadId_t openMRNThread_id;
static osSemaphoreId_t openMRNStart_sem;
static std::atomic<bool> openMRNRunning{false};

uint64_t node_id = 0x0501010118F000ULL; // Example Node ID  TODO: move to header

static openlcb::SimpleCanStack *g_stack = nullptr;
static openlcb::MockSNIPUserFile *g_nodeinfo = nullptr;

/* Definitions for openMRNTask */
static const osThreadAttr_t openMRNTask_attributes = {
  .name = "openMRNTask",
  .stack_size = 1024 * 4, // 4 KB stack size
  .priority = (osPriority_t) osPriorityNormal
};

extern "C" void OpenMRN_ClientThread(void *argument) {
  (void)argument;  // Unused parameter

  for (;;) {
    osSemaphoreAcquire(openMRNStart_sem, osWaitForever);
    openMRNRunning.store(true);

    while (openMRNRunning.load()) {
      osDelay(10u);
    }

    osSemaphoreRelease(openMRNStart_sem);
    osDelay(5u);
  }
}



extern "C" void OpenMRN_Client_Init(FDCAN_HandleTypeDef *hfdcan) {
  hOpenMRNCAN = hfdcan;
  openMRNStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
  // Create persistent stack object
  g_stack = new openlcb::SimpleCanStack(node_id);
  g_nodeinfo = new openlcb::MockSNIPUserFile("My Node", "Description");

  // Don't call stack.start() - the stack typically starts automatically

  openMRNThread_id = osThreadNew(OpenMRN_ClientThread, NULL, &openMRNTask_attributes);

}

extern "C" void OpenMRN_Client_Start(void) {
  if (!openMRNRunning.load()) {
    openMRNRunning.store(true);
    osSemaphoreRelease(openMRNStart_sem);
  }
}

extern "C" void OpenMRN_Client_Stop(void) {
  if (openMRNRunning.load()) {
    openMRNRunning.store(false);
    osSemaphoreAcquire(openMRNStart_sem, osWaitForever);
  }
}

extern "C" HAL_StatusTypeDef OpenMRN_Client_SendMessage(uint32_t arbitration_id, const uint8_t *data, uint8_t dlc) {
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
  std::memcpy(txData, data, dlc);

  return HAL_FDCAN_AddMessageToTxFifoQ(hOpenMRNCAN, &txHeader, txData);
}
