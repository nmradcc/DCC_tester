#ifndef CLI_APP_H
#define CLI_APP_H

#include "stdint.h"
#include "cmsis_os2.h"

void uart_receive_callback(char *input);
void CliApp_SetActiveCommandQueue(osMessageQueueId_t queue);
osMessageQueueId_t CliApp_GetActiveCommandQueue(void);
void vCommandConsoleTask(void *pvParameters);
void vRPCTask(void *pvParameters);
#endif // CLI_APP_H
