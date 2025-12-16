#include "command_station.hpp"
#include <cstdint>
#include <cstdio>
#include <dcc/speed.hpp>
#include "cmsis_os2.h"
#include "main.h"
#include "parameter_manager.h"
#include "stm32h5xx_hal_uart.h"


#define RX_BIDIR_MAX_SIZE 16 // Maximum size of the BiDi receive buffer

static osThreadId_t commandStationThread_id;
static osSemaphoreId_t commandStationStart_sem;
static bool commandStationRunning = false;
static uint8_t commandStationLoop = 0;  // 0=no loop, 1=loop1, 2=loop2, 3=loop3
static uint64_t bitCountMask = 0;

static uint16_t dac_value = 0;
static uint8_t trigger_first_bit = false;
static uint64_t zerobitOverrideMask = 0;
static int32_t zerobitDeltaP = 0;
static int32_t zerobitDeltaN = 0;
static bool currentPhaseIsP = true;  // Track current phase (P or N)

// Custom packet storage
static dcc::Packet customPacket;
static bool customPacketLoaded = false;
static bool customPacketTrigger = false;
static uint32_t customPacketCount = 1;
static uint32_t customPacketDelay = 100;

/* Definitions for cmdStationTask */
const osThreadAttr_t cmdStationTask_attributes = {
  .name = "cmdStationTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityHigh
};


void CommandStation::trackOutputs(bool N, bool P, bool first_bit) 
{ 
  TR_P_GPIO_Port->BSRR = (static_cast<uint32_t>(!N) << TR_N_BR_Pos) | (static_cast<uint32_t>(!P) << TR_P_BR_Pos) |
                            (static_cast<uint32_t>(N) << TR_N_BS_Pos) | (static_cast<uint32_t>(P) << TR_P_BS_Pos);
  TRACK_P_GPIO_Port->BSRR = (static_cast<uint32_t>(!P) << TRACK_P_BR_Pos) |
                            (static_cast<uint32_t>(P) << TRACK_P_BS_Pos);
  
  // Track which phase we're in for delta adjustment
  currentPhaseIsP = P;
  
  if (P)
  {
    if (first_bit)
      bitCountMask = 1;
    else
      bitCountMask <<= 1;
  }

  if (trigger_first_bit)
  {
    first_bit ? HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, GPIO_PIN_SET) : HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, GPIO_PIN_RESET);
  }
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
      auto arr{command_station.transmit()};
      if ((zerobitOverrideMask & bitCountMask) != 0) {
        if (arr >= DCC_TX_MIN_BIT_0_TIMING) {  // only adjust Zero bits
          // Adjust zero bit by deltaP or deltaN based on current phase
          arr = arr + (currentPhaseIsP ? zerobitDeltaP : zerobitDeltaN);
        }
      }
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
    get_dcc_trigger_first_bit(&trigger_first_bit);
    get_dcc_zerobit_override_mask(&zerobitOverrideMask);
    get_dcc_zerobit_deltaP(&zerobitDeltaP);
    get_dcc_zerobit_deltaN(&zerobitDeltaN);

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
      .flags = {.bidi = static_cast<bool>(bidi)},
    });

  // Enable update interrupt
    __HAL_TIM_ENABLE_IT(&htim2, TIM_IT_UPDATE);
    HAL_TIM_PWM_Start_IT(&htim2, TIM_CHANNEL_1);
    commandStationRunning = true;
    dcc::Packet packet{};

    // Check for custom packet trigger when not in loop mode
    if (commandStationLoop == 0) {
      printf("Command station started in custom packet mode\n");
      while (commandStationRunning) {
        if (customPacketTrigger && customPacketLoaded) {
          for (uint32_t i = 0; i < customPacketCount; i++) {
            command_station.packet(customPacket);
            printf("Custom packet transmitted [%lu/%lu]: ", i + 1, customPacketCount);
            for (size_t j = 0; j < customPacket.size(); j++) {
              printf("0x%02X ", customPacket[j]);
            }
            printf("\n");
            if (i < customPacketCount - 1 && customPacketDelay > 0) {
              osDelay(customPacketDelay);
            }
          }
          customPacketTrigger = false;
        }
        osDelay(100u);
      }
    }
    else if (commandStationLoop == 1) {
      // Test loop1: Basic function and speed control (address 3)
      printf("Starting test loop1: Basic function and speed control\n");
      while (commandStationRunning) {
        // Set function F0
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0001u);
        command_station.packet(packet);
        printf("Loop1: set function F0\n");
        osDelay(2000u);

        // Accelerate forward
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 42u);
        command_station.packet(packet);
        printf("Loop1: accelerate to speed step 42 forward\n");
        osDelay(2000u);

        // Stop
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 0u);
        command_station.packet(packet);
        printf("Loop1: stop (forward)\n");
        osDelay(2000u);

        // Clear function
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
        command_station.packet(packet);
        printf("Loop1: clear function F0\n");
        osDelay(2000u);

        // Accelerate reverse
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 42u);
        command_station.packet(packet);
        printf("Loop1: accelerate to speed step 42 reverse\n");
        osDelay(2000u);

        // Stop
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 0u);
        command_station.packet(packet);
        printf("Loop1: stop (reverse)\n");
        osDelay(2000u);
      }
    }
    else if (commandStationLoop == 2) {
      // Test loop2: Emergency stop test (address 3)
      printf("Starting test loop2: Emergency stop test\n");
      while (commandStationRunning) {
        // Turn on headlight (F0)
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b1'0001u);
        command_station.packet(packet);
        printf("Loop2: headlight on\n");
//        osDelay(1000u);

        // Accelerate to speed 60 forward
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 60u);
        command_station.packet(packet);
        printf("Loop2: accelerate to speed 60 forward\n");
        osDelay(3000u);

        // EMERGENCY STOP - Broadcast to all locomotives (address 0)
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_advanced_operations_speed_packet(0u, 1u << 7u | 1u);  // Broadcast emergency stop
        command_station.packet(packet);
        printf("Loop2: EMERGENCY STOP (broadcast)\n");
        osDelay(2000u);

        // Resume normal operation - speed 30 forward
//        BSP_LED_Toggle(LED_GREEN);
//        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 30u);
//        command_station.packet(packet);
//        printf("Loop2: resume speed 30 forward\n");
//        osDelay(3000u);

        // Normal stop (address 3)
//        BSP_LED_Toggle(LED_GREEN);
//        packet = dcc::make_advanced_operations_speed_packet(3u, 1u << 7u | 0u);
//        command_station.packet(packet);
//        printf("Loop2: normal stop\n");
//        osDelay(2000u);

        // Turn off headlight
        BSP_LED_Toggle(LED_GREEN);
        packet = dcc::make_function_group_f4_f0_packet(3u, 0b0'0000u);
        command_station.packet(packet);
        printf("Loop2: headlight off\n");
        osDelay(5000u);
      }
    }
    else if (commandStationLoop == 3) {
      // Test loop3: Speed ramping test (address 10)
      printf("Starting test loop3: Speed ramping test\n");
      while (commandStationRunning) {
        // Ramp up speed forward
        for (uint8_t speed = 0; speed <= 126 && commandStationRunning; speed += 10) {
          BSP_LED_Toggle(LED_GREEN);
          packet = dcc::make_advanced_operations_speed_packet(10u, 1u << 7u | speed);
          command_station.packet(packet);
          printf("Loop3: speed step %d forward\n", speed);
          osDelay(500u);
        }

        osDelay(1000u);

        // Ramp down speed forward
        for (int8_t speed = 126; speed >= 0 && commandStationRunning; speed -= 10) {
          BSP_LED_Toggle(LED_GREEN);
          packet = dcc::make_advanced_operations_speed_packet(10u, 1u << 7u | speed);
          command_station.packet(packet);
          printf("Loop3: speed step %d forward\n", speed);
          osDelay(500u);
        }

        osDelay(1000u);

        // Ramp up speed reverse
        for (uint8_t speed = 0; speed <= 126 && commandStationRunning; speed += 10) {
          BSP_LED_Toggle(LED_GREEN);
          packet = dcc::make_advanced_operations_speed_packet(10u, speed);
          command_station.packet(packet);
          printf("Loop3: speed step %d reverse\n", speed);
          osDelay(500u);
        }

        osDelay(1000u);

        // Ramp down speed reverse
        for (int8_t speed = 126; speed >= 0 && commandStationRunning; speed -= 10) {
          BSP_LED_Toggle(LED_GREEN);
          packet = dcc::make_advanced_operations_speed_packet(10u, speed);
          command_station.packet(packet);
          printf("Loop3: speed step %d reverse\n", speed);
          osDelay(500u);
        }

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
// loop: 0=custom packet, 1=loop1 (basic), 2=loop2 (functions), 3=loop3 (speed ramp)
extern "C" void CommandStation_Start(uint8_t loop)
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

extern "C" bool CommandStation_LoadCustomPacket(const uint8_t* bytes, uint8_t length) {
  if (!bytes || length == 0 || length > DCC_MAX_PACKET_SIZE) {
    return false;
  }
  
  customPacket.clear();
  for (uint8_t i = 0; i < length; i++) {
    customPacket.push_back(bytes[i]);
  }
  customPacketLoaded = true;
  customPacketTrigger = false;
  
  return true;
}

extern "C" void CommandStation_TriggerTransmit(uint32_t count, uint32_t delay_ms) {
  if (customPacketLoaded) {
    customPacketCount = (count > 0) ? count : 1;
    customPacketDelay = delay_ms;
    customPacketTrigger = true;
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

