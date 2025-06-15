#ifndef CLI_APP_H
#define CLI_APP_H

#include "stdint.h"

void processRxedChar(uint8_t rxChar);
void handleNewline(const char *const InputBuffer, char *OutputBuffer, uint8_t *cInputIndex);
void handleCharacterInput(uint8_t *cInputIndex, char *InputBuffer);
void vRegisterCLICommands(void);
void vCommandConsoleTask(void *pvParameters);
#endif // CLI_APP_H
