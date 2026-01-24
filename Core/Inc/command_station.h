#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void CommandStation_Init(void);
bool CommandStation_Start(uint8_t loop);  // loop: 0=no loop, 1=loop1, 2=loop2, 3=loop3. Returns true if started, false if already running
bool CommandStation_Stop(void);  // Returns true if stopped, false if not running
bool CommandStation_bidi_Threshold(uint16_t threshold);
bool CommandStation_LoadCustomPacket(const uint8_t* bytes, uint8_t length);
void CommandStation_TriggerTransmit(uint32_t count, uint32_t delay_ms);

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
