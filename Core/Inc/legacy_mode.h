#ifndef LEGACY_MODE_H
#define LEGACY_MODE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEGACY_PACKET_RESET 0U
#define LEGACY_PACKET_IDLE  1U
#define LEGACY_PACKET_HARD  2U
#define LEGACY_PACKET_BASE  3U

void LegacyMode_Init(void);
bool LegacyMode_Start(void);
bool LegacyMode_Stop(void);
bool LegacyMode_IsRunning(void);

bool LegacyMode_SetReservedTimer(uint8_t timer_id);
uint8_t LegacyMode_GetReservedTimer(void);

bool LegacyMode_SelectPacket(uint8_t packet_id);
uint8_t LegacyMode_GetSelectedPacket(void);
const char* LegacyMode_GetSelectedPacketName(void);
const char* LegacyMode_GetModeName(void);

bool LegacyMode_ApplyCompatKey(char key_cmd);

#ifdef __cplusplus
}
#endif

#endif // LEGACY_MODE_H