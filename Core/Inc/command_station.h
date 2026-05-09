#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CommandStation_Init(void);
bool CommandStation_Start(uint8_t loop);  // loop: 0=no loop, 1=loop1, 2=loop2, 3=loop3. Returns true if started, false if already running
bool CommandStation_Stop(void);  // Returns true if stopped, false if not running
bool CommandStation_bidi_Threshold(uint16_t threshold);
bool CommandStation_LoadCustomPacket(const uint8_t* bytes, uint8_t length, bool replace);
void CommandStation_TriggerTransmit(uint32_t delay_ms);
bool CommandStation_IsCustomPacketQueueFull(void);
uint8_t CommandStation_GetCustomPacketQueueCount(void);

// Streaming bits mode (bypasses DCC library; raw half-bit timings)
// bits[]: one byte per bit (0 or 1), bit_count <= 512
// bit1_duration / bit0_duration: half-bit timer ticks (µs at 1 MHz)
// replace: true = clear existing buffer first
bool CommandStation_LoadStreamBits(const uint8_t* bits, uint16_t bit_count,
                                   uint16_t bit1_duration, uint16_t bit0_duration,
                                   bool replace);
bool CommandStation_TriggerStreamBits(uint16_t count);
void CommandStation_ClearStreamBits(void);
bool CommandStation_IsStreamBitsActive(void);
uint16_t CommandStation_GetStreamBitCount(void);

// RAM-only override parameter getters/setters
void CommandStation_SetZerobitOverrideMask(uint64_t mask);
uint64_t CommandStation_GetZerobitOverrideMask(void);
void CommandStation_SetZerobitDeltaP(int32_t delta);
int32_t CommandStation_GetZerobitDeltaP(void);
void CommandStation_SetZerobitDeltaN(int32_t delta);
int32_t CommandStation_GetZerobitDeltaN(void);


#ifdef __cplusplus
}
#endif
