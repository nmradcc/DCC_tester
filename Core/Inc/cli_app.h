#ifndef CLI_APP_H
#define CLI_APP_H

#include "stdint.h"

void uart_receive_callback(char *input);
void vCommandConsoleTask(void *pvParameters);
void vRPCTask(void *pvParameters);
#endif // CLI_APP_H
