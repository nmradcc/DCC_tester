#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "main.h"
#include "tx_api.h"
#include "stm32h5xx_nucleo.h"
#include "CLI.h"


ULONG cli_queue_storage[5];

TX_QUEUE cli_queue;
TX_MUTEX cli_mutex;

char OutputBuffer[MAX_OUTPUT_SIZE];
char InputBuffer[MAX_INPUT_SIZE];

int8_t cRxedChar;
const char * cli_prompt = "\r\ncli> ";
/* CLI escape sequences*/
uint8_t backspace[] = "\b \b";
uint8_t backspace_tt[] = " \b";

void uart_receive_callback(char *input) {
    tx_queue_send(&cli_queue, input, TX_NO_WAIT);
}

int _write(int file, char *data, int len)
{
    (void)(file);
    // Transmit data using UART
    for (int i = 0; i < len; i++)
    {
        // Send the character
        USART3->TDR = (uint16_t)data[i];
        // Wait for the transmit buffer to be empty
        while (!(USART3->ISR & USART_ISR_TXE));
    }
    return len;
}


/*************************************************************************************************/
void cliWrite(const char *str)
{
   printf("%s", str);
   // flush stdout
   fflush(stdout);
}
/*************************************************************************************************/
void vCommandConsoleTask(void *pvParameters)
{
    (void)(pvParameters);
    uint8_t cInputIndex = 0; // simply used to keep track of the index of the input string
    uint32_t receivedValue;  // used to store the received value from the notification

    tx_queue_create(&cli_queue, "Queue", TX_1_ULONG, cli_queue_storage, sizeof(cli_queue_storage));

    for (;;)
    {
        // Wait for data from ISR
        tx_queue_receive(&cli_queue, &receivedValue, TX_WAIT_FOREVER);

        //echo recevied char
        cRxedChar = receivedValue & 0xFF;
    }
}


#endif /* CLI_COMMANDS_H */
