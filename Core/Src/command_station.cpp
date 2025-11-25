#include "command_station.hpp"
#include <cstdint>
#include <cstdio>
#include "cmsis_os2.h"
#include "main.h"
#include "stm32h5xx_hal_uart.h"
#include "rpc_core.hpp"


static osThreadId_t commandStationThread_id;
static osSemaphoreId_t commandStationStart_sem;
static bool commandStationRunning = false;
static bool commandStationBidi = false;
static bool commandStationLoop = false;

static uint16_t dac_value =  DEFAULT_BIDIR_THRESHOLD; // DEFAULT BIDIR threshold value for 12-bit DAC

volatile size_t write_index = 0;

CommandStation command_station;
dcc::bidi::Datagram<> rx_datagram;
dcc::bidi::Datagram<> received_datagram;
size_t received_datagram_size = 0;

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
 
  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set DCC trigger high

  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET));   // Set BR_ENABLE low
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BiDi high

  std::fill(rx_datagram.begin(), rx_datagram.end(), 0);
  write_index = 0; // Reset write index

  huart6.Instance->CR1 |= USART_CR1_RXNEIE;
}

void CommandStation::biDiChannel1() {}

void CommandStation::biDiChannel2() {}

void CommandStation::biDiEnd() {
  HAL_UART_AbortReceive_IT(&huart6); // Stop receiving BiDi data
  huart6.Instance->CR1 &= ~USART_CR1_RXNEIE;
  HAL_GPIO_WritePin(BIDIR_EN_GPIO_Port, BIDIR_EN_Pin, static_cast<GPIO_PinState>(GPIO_PIN_RESET)); // Set BiDi low
  HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
  if (write_index > 1) {
    received_datagram = rx_datagram;
    received_datagram_size = write_index;
  }
  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, GPIO_PIN_RESET); // Set DCC trigger low
}

/**
  * @brief This function handles USART6 global interrupt.
  */
extern "C" void USART6_IRQHandler(void)
{
  if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE)) {
        // Read all bytes in FIFO
        while (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE)) {
            rx_datagram[write_index++] = (uint8_t)(huart6.Instance->RDR);  // Read one byte
        }
    }
}

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
//      packet = dcc::make_cv_access_short_write_packet(3u, 0b0010u, 8u, 145u);
//      command_station.packet(packet);
//      printf("Command station: set CV 3, 0b0010u, 8u, 145u\n");

      if (received_datagram_size >= 2) {
        printf("CMS:BiDi RX datagram of size %d: 0x%02X 0x%02X\n", received_datagram_size, received_datagram[0], received_datagram[1]);
        received_datagram_size = 0;

        dcc::bidi::Dissector dissector{received_datagram, packet};

        // Iterate
        for (auto const& dg : dissector)
          if (auto adr_low{get_if<dcc::bidi::app::AdrLow>(&dg)}) {
            // Use app:adr_low data here
            printf("Command station: received app:adr_low data: 0x%02X\n", *adr_low);
          } else if (auto dyn{get_if<dcc::bidi::app::Dyn>(&dg)}) {
            // Use app:dyn data here
            printf("Command station: received app:dyn data: 0x%02X\n", *dyn);
          }

        std::fill(received_datagram.begin(), received_datagram.end(), 0);

      }

      osDelay(300u);


      BSP_LED_Toggle(LED_GREEN);
      packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
      command_station.packet(packet);
      printf("Command station: clear function F0\n");
      osDelay(300);
#if 0
//#endif
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
#endif
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
extern "C" void CommandStation_Start(bool bidi, bool loop)
{
  if (!commandStationRunning) {
    commandStationBidi = bidi;
    commandStationLoop = loop;
    HAL_GPIO_WritePin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set BR_ENABLE high
    osSemaphoreRelease(commandStationStart_sem);
    printf("Command station started (bidi=%d, loop=%d)\n", bidi, loop);
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

