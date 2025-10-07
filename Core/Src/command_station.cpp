#include "command_station.hpp"
#include <cstdint>
#include <cstdio>
#include "cmsis_os2.h"
#include "main.h"
#include "stm32h5xx_hal_uart.h"

#define RX_BIDIR_MAX_SIZE 16 // Maximum size of the BiDi receive buffer

static osThreadId_t commandStationThread_id;
static osSemaphoreId_t commandStationStart_sem;
static bool commandStationRunning = false;
static bool commandStationBidi = false;

//static uint16_t dac_value =  DEFAULT_BIDIR_THRESHOLD; // DEFAULT BIDIR threshold value for 12-bit DAC
static uint16_t dac_value =  100; // DEFAULT BIDIR threshold value for 12-bit DAC

static uint8_t rx_byte;
static uint8_t bidirBuffer[RX_BIDIR_MAX_SIZE] = {0}; // Buffer for BiDi data
volatile uint16_t write_index = 0;

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

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        bidirBuffer[write_index++] = rx_byte;
//        write_index %= sizeof(bidirBuffer);
        HAL_UART_Receive_IT(huart, &rx_byte, 1);
    }
}




void CommandStation::biDiStart() {
  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET));   // Set BR_ENABLE low
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BiDi high
#if 0
  huart6.Instance->CR1 |= USART_CR1_RXNEIE;
  for (uint16_t i = 0; i < RX_BIDIR_MAX_SIZE; ++i) {
    bidirBuffer[i] = 0; // Clear the buffer
  }
  write_index = 0; // Reset write index
//  HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
#endif
}

void CommandStation::biDiChannel1() {}

void CommandStation::biDiChannel2() {}

void CommandStation::biDiEnd() {
//  HAL_UART_AbortReceive_IT(&huart6); // Stop receiving BiDi data
//  huart6.Instance->CR1 &= ~USART_CR1_RXNEIE;
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET)); // Set BiDi low
  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
}

CommandStation command_station;


/* only use callback if NOT using custom interrupt handler! */
extern "C" void CS_HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set DCC trigger high
  auto const arr{command_station.transmit()};
  htim->Instance->ARR = arr;
  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, GPIO_PIN_RESET); // Set DCC trigger low
}


void CommandStationThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(commandStationStart_sem, osWaitForever);

    // Initialize DCC Command Station
    if (commandStationBidi) {

      // Start DAC channel 2
      HAL_DAC_Start(&hdac1, DAC_CHANNEL_2);

      // Write value to DAC OUT2
      HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_2, DAC_ALIGN_12B_R, dac_value);
      printf("DAC value: %d\n", dac_value);
    command_station.init({
      .num_preamble = DCC_TX_MIN_PREAMBLE_BITS,
      .bit1_duration = 58u,
      .bit0_duration = 100u,
        .flags = {.bidi = true},
    });
    } else {
      command_station.init({
        .num_preamble = DCC_TX_MIN_PREAMBLE_BITS,
        .bit1_duration = 58u,
        .bit0_duration = 100u,
        .flags = {.bidi = false},
      });
    }

  // Enable update interrupt
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
    HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
    commandStationRunning = true;

    // Main loop
    // Send a few packets to test the command station
    // This is not part of the command station functionality, but rather a test
    // to see if the command station is working correctly.

    dcc::Packet packet{};
//    dcc::tx::Timings timings{};

while (commandStationRunning) {

      // Set function F0
      BSP_LED_Toggle(LED_GREEN);
      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0001u);
//      timings = dcc::tx::bytes2timings(packet);
//    for (auto i{0uz}; i < timings.size(); ++i)
//        command_station.transmit();
//      command_station.transmit();
      command_station.packet(packet);
      printf("Command station: set function F0\n");
      osDelay(300u);
      // Clear function
      BSP_LED_Toggle(LED_GREEN);
      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
//      timings = dcc::tx::bytes2timings(packet);
//      command_station.transmit();
      command_station.packet(packet);
      printf("Command station: clear function F0\n");
      osDelay(300);
#if 0
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
//#endif
      // Clear function
      BSP_LED_Toggle(LED_GREEN);
      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
      command_station.packet(packet);
      printf("Command station: clear function F0\n");
      osDelay(2000u);

      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0010u);
      command_station.packet(packet);
      printf("Command station: clear function F0\n");
      osDelay(2000u);
      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0100u);
      command_station.packet(packet);
      printf("Command station: clear function F0\n");
      osDelay(2000u);



    //  #if 0
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
#endif
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
extern "C" void CommandStation_Start(bool bidi)
{
  if (!commandStationRunning) {
    commandStationBidi = bidi;
    HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
    osSemaphoreRelease(commandStationStart_sem);
    printf("Command station started\n");
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

