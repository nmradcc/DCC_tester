#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "tx_api.h"
#include "stm32h5xx_nucleo.h"
#include "cli_app.h"

typedef struct Command {
    const char *name;
    void (*execute)(void);
    struct Command *next;
} Command;


ULONG command_queue_storage[5];
TX_QUEUE command_queue;

static char InputBuffer[32];
static char OutputBuffer[32];
static unsigned int inputIndex = 0;

void uart_receive_callback(char *input) {
    tx_queue_send(&command_queue, input, TX_NO_WAIT);
}

void hello_command(void) {
    printf("Hello from ThreadX CLI!\n");
}

Command cmd_hello = {"hello", hello_command, NULL};
Command *command_list = &cmd_hello;  // Add more commands here





// Function to write data to UART
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

void vCommandConsoleTask(void *pvParameters)
{
    (void)(pvParameters);
    uint32_t receivedChar;  // used to store the received value from the notification
    char N_char = '\n';
    tx_queue_create(&command_queue, "Queue", TX_1_ULONG, command_queue_storage, sizeof(command_queue_storage));

    for (;;)
    {
        // Wait for data from ISR
        tx_queue_receive(&command_queue, &receivedChar, TX_WAIT_FOREVER);
        if (receivedChar == '\b' || receivedChar == 0x7F) {
            // user pressed backspace, remove last character from input string
            if (inputIndex > 0) {
                inputIndex--;
                InputBuffer[inputIndex] = '\0'; // Null terminate the string
                sprintf(OutputBuffer,"\b \b"); // Move cursor back, print space to overwrite, and move back again
                _write(0, OutputBuffer, strlen(OutputBuffer)); // Echo backspace to console
            }
        }
        else if (inputIndex < sizeof(InputBuffer) - 1) {
            // user pressed a character add it to the input string
            InputBuffer[inputIndex++] = receivedChar;
            InputBuffer[inputIndex] = '\0'; // Null terminate the string
            _write(0, (char *)&receivedChar, 1); // Echo the character to the console
            if (receivedChar == '\r' || receivedChar == '\n') {
                // Process the command when Enter is pressed
                InputBuffer[inputIndex - 1] = '\0'; // Null terminate the string
                inputIndex = 0; // Reset input index for next command
                _write(0, &N_char, 1); // Echo the character to the console
                // Here you can add code to parse and execute the command
                Command *current = command_list;
//                bool command_found = false;
                while (current != NULL) {
                    if (strcmp(InputBuffer, current->name) == 0) {
                        current->execute();
//                        command_found = true;
                        break;
                    }
                    current = current->next;
                }
            }   
        }
        else if (inputIndex >= sizeof(InputBuffer) - 1) {
            // input buffer is full, ignore further input
        }


    }
}


#endif /* CLI_COMMANDS_H */
