#include "decoder.hpp"
#include "decoder.h"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <dcc/bidi/timing.hpp>
#include "cmsis_os2.h"
#include "main.h"
#include "stm32h563xx.h"

static osThreadId_t decoderThread_id;
static osSemaphoreId_t decoderStart_sem;
static bool decoderRunning = false;

Decoder decoder;

/* Definitions for decoderTask */
const osThreadAttr_t decoderTask_attributes = {
  .name = "decoderTask",
  .stack_size = 8192,
  .priority = (osPriority_t) osPriorityHigh
};


void Decoder::direction(uint16_t addr, bool dir) {}

void Decoder::speed(uint16_t addr, int32_t speed) {
  if (speed) {
    printf("\nDecoder: accelerate to speed step %d\n", speed);
  } else {
    printf("Decoder: stop\n");
  }
}

void Decoder::function(uint16_t addr, uint32_t mask, uint32_t state) {
  if (!(mask & 0b0'0001u)) return;
  else if (state & 0b0'0001u) {
//  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, static_cast<GPIO_PinState>(GPIO_PIN_SET));   // Set DCC trigger high
    printf("Decoder: set function F0\n");
//  HAL_GPIO_WritePin(SCOPE_GPIO_Port, SCOPE_Pin, GPIO_PIN_RESET); // Set DCC trigger low
  } else {
    printf("Decoder: clear function F0\n");
  }
}

void Decoder::serviceModeHook(bool service_mode) {}

void Decoder::serviceAck() {}

const int TX_MAX_DELAY_MS = 100;
void Decoder::transmitBiDi(std::span<uint8_t const> bytes) {
  HAL_UART_Transmit(&huart4, bytes.data(), static_cast<uint16_t>(bytes.size()), TX_MAX_DELAY_MS);
}

uint8_t Decoder::readCv(uint32_t cv_addr, uint8_t) {
  if (cv_addr >= _cvs.size()) return 0u;
  return _cvs[cv_addr];
}

uint8_t Decoder::writeCv(uint32_t cv_addr, uint8_t byte) {
  if (cv_addr >= _cvs.size()) return 0u;
  return _cvs[cv_addr] = byte;
}

bool Decoder::readCv(uint32_t cv_addr, bool, uint32_t pos) { return false; }

bool Decoder::writeCv(uint32_t cv_addr, bool bit, uint32_t pos) {
  return false;
}


extern "C" void TIM14_IRQHandler(void)
{
  uint32_t itsource = htim14.Instance->DIER;
  uint32_t itflag   = htim14.Instance->SR;

  /* TIM Update event */
  if ((itflag & (TIM_FLAG_UPDATE)) == (TIM_FLAG_UPDATE))
  {
    if ((itsource & (TIM_IT_UPDATE)) == (TIM_IT_UPDATE))
    {
      __HAL_TIM_CLEAR_FLAG(&htim14, TIM_FLAG_UPDATE);
      htim14.Channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED;
      HAL_TIM_Base_Stop(&htim14);
      // TODO: check for quite track voltage
      // we will use BR_ENABLE pin state for the time being 
      // but should be replaced with proper no voltage on track detection
      // as we can not assume we are always using our command station!
      if (HAL_GPIO_ReadPin(BR_ENABLE_GPIO_Port, BR_ENABLE_Pin) == GPIO_PIN_RESET) 
      {
        decoder.biDiChannel1();
        decoder.biDiChannel2();
      }
    }
  }
}

extern "C" void TIM15_IRQHandler(void)
{
  uint32_t itsource = htim15.Instance->DIER;
  uint32_t itflag   = htim15.Instance->SR;

  /* Capture compare 1 event */
  if ((itflag & (TIM_FLAG_CC1)) == (TIM_FLAG_CC1))
  {
    if ((itsource & (TIM_IT_CC1)) == (TIM_IT_CC1))
    {
      {
        __HAL_TIM_CLEAR_FLAG(&htim15, TIM_FLAG_CC1);
        htim15.Channel = HAL_TIM_ACTIVE_CHANNEL_1;

        /* Input capture event */
        if ((htim15.Instance->CCMR1 & TIM_CCMR1_CC1S) != 0x00U)
        {
          // Get captured value (CH1)
          uint32_t ccr = HAL_TIM_ReadCapturedValue(&htim15, TIM_CHANNEL_1);
          decoder.receive(ccr);
          if (decoder.packetEnd()) {
            HAL_TIM_Base_Start_IT(&htim14);  // delay for BiDi response
          }
        }
        htim15.Channel = HAL_TIM_ACTIVE_CHANNEL_CLEARED;
      }
    }
  }
  /* TIM Update event */
  if ((itflag & (TIM_FLAG_UPDATE)) == (TIM_FLAG_UPDATE))
  {
    if ((itsource & (TIM_IT_UPDATE)) == (TIM_IT_UPDATE))
    {
      __HAL_TIM_CLEAR_FLAG(&htim15, TIM_FLAG_UPDATE);
    }
  }
}


void DecoderThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(decoderStart_sem, osWaitForever);

    decoder.init();
    
    htim14.Init.Period = (dcc::bidi::TTS1 - BIDI_CH1_START_OVERHEAD_US);  // cutout to start delay minus overhead
    HAL_TIM_Base_Init(&htim14);  // Reinitialize with new settings

    // Enable update interrupt
    __HAL_TIM_ENABLE_IT(&htim15, TIM_IT_UPDATE);
    HAL_TIM_IC_Start_IT(&htim15, TIM_CHANNEL_1);
    decoderRunning = true;

    while (decoderRunning) {
      if (decoder.execute()) {
        // Processed a packet
      }
      osDelay(3u);
    }
    HAL_TIM_IC_Stop_IT(&htim15, TIM_CHANNEL_1);
    __HAL_TIM_DISABLE_IT(&htim15, TIM_IT_UPDATE);
    osSemaphoreRelease(decoderStart_sem);
    osDelay(5u); // Give some time for the semaphore to be released
  }

}

// Called at system init
extern "C" void Decoder_Init(void)
{
    decoderStart_sem = osSemaphoreNew(1, 0, NULL);  // Start locked
    decoderThread_id = osThreadNew(DecoderThread, NULL, &decoderTask_attributes);
}

// Can be called from anywhere
extern "C" void Decoder_Start(void)
{
  if (!decoderRunning) {
    osSemaphoreRelease(decoderStart_sem);
    printf("Decoder started\n");
  }
  else {
    printf("Decoder already running\n");
  } 
}

extern "C" void Decoder_Stop(void)
{
  if (decoderRunning) {
    printf("Decoder stopping\n");
    decoderRunning = false;
    osSemaphoreAcquire(decoderStart_sem, osWaitForever);
    printf("Decoder stopped\n");
  }
  else {
    printf("Decoder not running\n");
  }
} 
