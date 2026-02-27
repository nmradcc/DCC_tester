#ifndef LEGACY_MODE_H
#define LEGACY_MODE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void LegacyMode_Init(void);
bool LegacyMode_Start(void);
bool LegacyMode_Stop(void);
bool LegacyMode_IsRunning(void);

bool LegacyMode_SetReservedTimer(uint8_t timer_id);
uint8_t LegacyMode_GetReservedTimer(void);

#ifdef __cplusplus
}
#endif

#endif // LEGACY_MODE_H