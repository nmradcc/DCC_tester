#ifndef CLI_APP_H
#define CLI_APP_H

#include "stdint.h"

void processRxedChar(uint8_t rxChar);
void handleNewline(const char *const pcInputString, char *cOutputBuffer, uint8_t *cInputIndex);
void handleCharacterInput(uint8_t *cInputIndex, char *pcInputString);
void vRegisterCLICommands(void);
void vCommandConsoleTask(void *pvParameters);
#endif // CLI_APP_H
