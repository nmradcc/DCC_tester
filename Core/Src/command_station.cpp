#include "command_station.hpp"
#include <cstdint>
#include <cstdio>
#include "cmsis_os2.h"
#include "main.h"
#include "parameter_manager.h"
#include "stm32h5xx_hal_uart.h"
#include "rpc_core.hpp"

#define RX_BIDIR_MAX_SIZE 16 // Maximum size of the BiDi receive buffer

static osThreadId_t commandStationThread_id;
static osSemaphoreId_t commandStationStart_sem;
static bool commandStationRunning = false;
static bool commandStationLoop = false;
static uint16_t dac_value = 0;

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

void CommandStation::biDiStart() {
  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET));   // Set BR_ENABLE low
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BiDi high
}

void CommandStation::biDiChannel1() {}

void CommandStation::biDiChannel2() {}

void CommandStation::biDiEnd() {
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET)); // Set BiDi low
  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
}

CommandStation command_station;



/**
  * @brief This function handles TIM2 global interrupt.
  */
extern "C" void TIM2_IRQHandler(void)
{

  uint32_t itsource = htim2.Instance->DIER;
  uint32_t itflag   = htim2.Instance->SR;

  /* Capture compare 1 event */
  if ((itflag & (TIM_FLAG_CC1)) == (TIM_FLAG_CC1))
  {
    if ((itsource & (TIM_IT_CC1)) == (TIM_IT_CC1))
    {
      {
        __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_CC1);
        htim2.Channel = HAL_TIM_ACTIVE_CHANNEL_1;
        htim2.Channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED;
      }
    }
  }
  /* TIM Update event */
  if ((itflag & (TIM_FLAG_UPDATE)) == (TIM_FLAG_UPDATE))
  {
    if ((itsource & (TIM_IT_UPDATE)) == (TIM_IT_UPDATE))
    {
      __HAL_TIM_CLEAR_FLAG(&htim2, TIM_FLAG_UPDATE);
  auto const arr{command_station.transmit()};
      htim2.Instance->ARR = arr; // Set auto-reload register for next interrupt
    }
  }
}

void CommandStationThread(void *argument) {
  (void)argument;  // Unused parameter

  uint8_t preamble_bits = 0;
  uint8_t bit1_duration = 0;
  uint8_t bit0_duration = 0;
  uint8_t bidi = false;

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(commandStationStart_sem, osWaitForever);

    get_dcc_preamble_bits(&preamble_bits);
    get_dcc_bit1_duration(&bit1_duration);
    get_dcc_bit0_duration(&bit0_duration);
    get_dcc_bidi_enable(&bidi);
    get_dcc_bidi_dac(&dac_value);

    // Initialize DCC Command Station
    if (bidi) {

      // Start DAC channel 2
      HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);

      // Write value to DAC OUT2
      HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, dac_value);
      printf("DAC value: %d\n", dac_value);
    }
    command_station.init({
      .num_preamble = preamble_bits,
      .bit1_duration = bit1_duration,
      .bit0_duration = bit0_duration,
      .flags = {.bidi = bidi},
    });

  // Enable update interrupt
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
    HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
    commandStationRunning = true;
    dcc::Packet packet{};

    if (commandStationLoop) {
      // Test loop
      // Send a few packets to test the command station
      // This is not part of the command station functionality, but rather a test loop
      // to see if the command station is working correctly.
      while (commandStationRunning) {
        // Set function F0
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0001u);
        command_station.packet(packet);
        printf("Command station: set function F0\n");
        osDelay(2000u);

        // Accelerate
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 42u);
        command_station.packet(packet);
        printf("\nCommand station: accelerate to speed step 42 forward\n");
        osDelay(2000u);

        // Decelerate
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 0u);
        command_station.packet(packet);
        printf("Command station: stop (forward)\n");
        osDelay(2000u);

        // Clear function
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
        command_station.packet(packet);
        printf("Command station: clear function F0\n");
        osDelay(2000u);

        // Accelerate
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 42u);
        command_station.packet(packet);
        printf("\nCommand station: accelerate to speed step 42 reverse\n");
        osDelay(2000u);

        // Decelerate
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 0u);
        command_station.packet(packet);
        printf("Command station: stop (reverse)\n");
        osDelay(2000u);

      }
    }
    else {
      // Wait until stopped
      while (commandStationRunning) {
        //TODO: test for (RPC) commands via queue to send packets
        osDelay(100u);
      }
    }
    HAL_TIM_PWM_Stop_IT(&htim2, TIM_CHANNEL_1);
    __HAL_TIM_DISABLE_IT(&htim2, TIM_IT_UPDATE);
    osSemaphoreRelease(commandStationStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }

}

// Called at system init
extern "C" void CommandStation_Init(void)
{
    commandStationStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
    commandStationThread_id = osThreadNew(CommandStationThread, NULL, &cmdStationTask_attributes);
}

// Can be called from anywhere
extern "C" void CommandStation_Start(bool loop)
{
  if (!commandStationRunning) {
    commandStationLoop = loop;
    HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
    osSemaphoreRelease(commandStationStart_sem);
    printf("Command station started (loop=%d)\n", loop);
  }
  else {
    printf("Command station already running\n");
  }
}

// Can be called from anywhere
extern "C" void CommandStation_Stop(void)
{
  if (commandStationRunning) {
    printf("Command station stopping\n");
    commandStationRunning = false;
    osSemaphoreAcquire(commandStationStart_sem, osWaitForever);
    HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET));   // Set BR_ENABLE low
    printf("Command station stopped\n");
  }
  else {
    printf("Command station not running\n");
  }
}

// Can be called from anywhere
extern "C" bool CommandStation_bidi_Threshold(uint16_t threshold)
{
  dac_value = threshold;
  printf("Command station bidi threshold %d\n", threshold);
  if (commandStationRunning) {
    // Write value to DAC OUT2
    // update threshold
    HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, dac_value);
    printf("DAC value: %d\n", dac_value);
    return true;
  }
  return false;
}

