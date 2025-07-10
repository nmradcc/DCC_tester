#include "command_station.hpp"
#include <cstdio>
#include "cmsis_os2.h"
#include "main.h"

static osThreadId_t commandStationThread_id;
static osSemaphoreId_t commandStationStart_sem;

/* Definitions for cmdStationTask */
const osThreadAttr_t cmdStationTask_attributes = {
  .name = "cmdStationTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityHigh
};


void CommandStation::trackOutputs(bool N, bool P) 
{ 
 TR_P_GPIO_Port->BSRR = (static_cast<uint32_t>(!N) << TR_N_BR_Pos) | (static_cast<uint32_t>(!P) << TR_P_BR_Pos) |
                           (static_cast<uint32_t>(N) << TR_N_BS_Pos) | (static_cast<uint32_t>(P) << TR_P_BS_Pos);
 TRACK_P_GPIO_Port->BSRR = (static_cast<uint32_t>(!P) << TRACK_P_BR_Pos) |
                           (static_cast<uint32_t>(P) << TRACK_P_BS_Pos);
}

void CommandStation::biDiStart() {}

void CommandStation::biDiChannel1() {}

void CommandStation::biDiChannel2() {}

void CommandStation::biDiEnd() {}

CommandStation command_station;


/* only use callback if NOT using custom interrupt handler! */
extern "C" void CS_HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  HAL_GPIO_WritePin(DCC_TRG_GPIO_Port, DCC_TRG_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set DCC trigger high
  auto const arr{command_station.transmit()};
  htim->Instance->ARR = arr;
  HAL_GPIO_WritePin(DCC_TRG_GPIO_Port, DCC_TRG_Pin, GPIO_PIN_RESET); // Set DCC trigger low
}


void CommandStationThread(void *argument) {
  command_station.init({
    .num_preamble = DCC_TX_MIN_PREAMBLE_BITS,
    .bit1_duration = 58u,
    .bit0_duration = 100u,
    .flags = {.invert = false, .bidi = false},
  });

  // Block until externally started
  osSemaphoreAcquire(commandStationStart_sem, osWaitForever);
  
  // Enable update interrupt
  __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
  HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);

#if defined(DEBUG)
  SCB->CCR &= ~SCB_CCR_UNALIGN_TRP_Msk;
#endif
  // Main loop
  // Send a few packets to test the command station
  // This is not part of the command station functionality, but rather a test
  // to see if the command station is working correctly.
  dcc::Packet packet{};
  for (;;) {
    // Accelerate
    BSP_LED_Toggle(LED_GREEN);
    packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 42u);
    command_station.packet(packet);
    printf("\nCommand station: accelerate to speed step 42\n");
    osDelay(2000u);

    // Set function F3
    BSP_LED_Toggle(LED_GREEN);
    packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0001u);
    command_station.packet(packet);
    printf("Command station: set function F0\n");
    osDelay(2000u);

    // Decelerate
    BSP_LED_Toggle(LED_GREEN);
    packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 0u);
    command_station.packet(packet);
    printf("Command station: stop\n");
    osDelay(2000u);

    // Clear function
    BSP_LED_Toggle(LED_GREEN);
    packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
    command_station.packet(packet);
    printf("Command station: clear function F0\n");
    osDelay(2000u);
  }
}

// Called at system init
extern "C" void CommandStationThread_Init(void)
{
    commandStationStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
    commandStationThread_id = osThreadNew(CommandStationThread, NULL, &cmdStationTask_attributes);
}

// Can be called from anywhere
extern "C" void CommandStationThread_Start(void)
{
    osSemaphoreRelease(commandStationStart_sem);
    HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
}


