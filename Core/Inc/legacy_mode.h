#ifndef LEGACY_MODE_H
#define LEGACY_MODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LEGACY_PACKET_RESET 0U
#define LEGACY_PACKET_IDLE  1U
#define LEGACY_PACKET_HARD  2U
#define LEGACY_PACKET_BASE  3U

void LegacyMode_Init(void);
bool LegacyMode_Start(void);
bool LegacyMode_SetStartArgs(const char* args_text, char* error_buf, size_t error_buf_size);
bool LegacyMode_Stop(void);
bool LegacyMode_IsRunning(void);

bool LegacyMode_SelectPacket(uint8_t packet_id);
uint8_t LegacyMode_GetSelectedPacket(void);
const char* LegacyMode_GetSelectedPacketName(void);
const char* LegacyMode_GetModeName(void);
const char* LegacyMode_GetStartupConfigName(void);
bool LegacyMode_IsStartupConfigLoaded(void);
bool LegacyMode_GetStartupManual(void);
bool LegacyMode_GetStartupLogPkts(void);
char LegacyMode_GetStartupDecoderType(void);
void LegacyMode_RefreshStartupConfigFromSd(void);
bool LegacyMode_WriteUserDocsToSd(void);
void LegacyMode_PrintStartupConfigStub(void);

bool LegacyMode_ApplyCompatKey(char key_cmd);

#ifdef __cplusplus
}
#endif

#endif // LEGACY_MODE_H