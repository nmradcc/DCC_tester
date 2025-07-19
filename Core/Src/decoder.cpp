#include "decoder.hpp"
#include <climits>
#include <cstdio>
#include "cmsis_os2.h"
#include "main.h"

static osThreadId_t decoderThread_id;
static osSemaphoreId_t decoderStart_sem;
static bool decoderRunning = false;

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
    printf("Decoder: set function F0\n");
  } else {
    printf("Decoder: clear function F0\n");
  }
}

void Decoder::serviceModeHook(bool service_mode) {}

void Decoder::serviceAck() {}

void Decoder::transmitBiDi(std::span<uint8_t const> bytes) {}

uint8_t Decoder::readCv(uint32_t cv_addr, uint8_t) {
  if (cv_addr >= size(_cvs)) return 0u;
  return _cvs[cv_addr];
}

uint8_t Decoder::writeCv(uint32_t cv_addr, uint8_t byte) {
  if (cv_addr >= size(_cvs)) return 0u;
  return _cvs[cv_addr] = byte;
}

bool Decoder::readCv(uint32_t cv_addr, bool, uint32_t pos) { return false; }

bool Decoder::writeCv(uint32_t cv_addr, bool bit, uint32_t pos) {
  return false;
}

Decoder decoder;

/* only use callback if NOT using custom interrupt handler! */
extern "C" void DC_HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
     // Get captured value (CH1)
    uint32_t ccr = HAL_TIM_ReadCapturedValue(&htim15, TIM_CHANNEL_1);

    decoder.receive(ccr);
}

void DecoderThread(void *argument) {
  (void)argument;  // Unused parameter

  while (true) {
    // Block until externally started
    osSemaphoreAcquire(decoderStart_sem, osWaitForever);

    decoder.init();

    // Enable update interrupt
    __HAL_TIM_ENABLE_IT(&htim15, TIM_IT_UPDATE);
    HAL_TIM_IC_Start_IT(&htim15, TIM_CHANNEL_1);
    decoderRunning = true;

    while (decoderRunning) {
      decoder.execute();
      osDelay(5u);
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
