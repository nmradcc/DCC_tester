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

static int cmd_clearScreen(char *pcWriteBuffer, size_t xWriteBufferLen,
                                  const char *pcCommandString)
{
    (void)pcCommandString;
    (void)xWriteBufferLen;
    memset(pcWriteBuffer, 0x00, xWriteBufferLen);
    printf("\033[2J\033[1;1H");
    return false;
}

static int cmd_toggle_led(char *pcWriteBuffer, size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    (void)xWriteBufferLen; // contains the length of the write buffer
    
    const char *pcParameter1;
    int xParameter1StringLength;

    pcParameter1 = CLIGetParameter
                        (
                          /* The command string itself. */
                          pcCommandString,
                          /* Return the first parameter. */
                          1,
                          /* Store the parameter string length. */
                          &xParameter1StringLength
                        );
    // convert the string to a number
    int32_t xValue1 = strtol(pcParameter1, NULL, 10);

    for (int i = 0; i < xValue1*2; i++)
    {
        /* Toggle the LED */
        BSP_LED_Toggle(LED_GREEN);
        // Delay for a short time
        HAL_Delay(500);
    }
  
    /* Write the response to the buffer */
    sprintf(pcWriteBuffer, "LED toggled %d times\r\n", (int)xValue1);
    
    return false;
}

static int cmd_add(char *pcWriteBuffer, size_t xWriteBufferLen,
                                 const char *pcCommandString)
{
    (void)xWriteBufferLen;
    const char *pcParameter1, *pcParameter2;
    int xParameter1StringLength, xParameter2StringLength;

    pcParameter1 = CLIGetParameter
                        (
                          /* The command string itself. */
                          pcCommandString,
                          /* Return the first parameter. */
                          1,
                          /* Store the parameter string length. */
                          &xParameter1StringLength
                        );
    pcParameter2 = CLIGetParameter
                        (
                          /* The command string itself. */
                          pcCommandString,
                          /* Return the first parameter. */
                          2,
                          /* Store the parameter string length. */
                          &xParameter2StringLength
                        );
    // convert the string to a number
    int32_t xValue1 = strtol(pcParameter1, NULL, 10);
    int32_t xValue2 = strtol(pcParameter2, NULL, 10);
    // add the two numbers
    int32_t xResultValue = xValue1 + xValue2;
    // convert the result to a string
    char cResultString[10];
    itoa(xResultValue, cResultString, 10);
    // copy the result to the write buffer
    strcpy(pcWriteBuffer, cResultString);
    
    return false;
}

// Define commands
static const CLI_Command_Definition_t xCommandList[] = {
    {
        .pcCommand = "cls", /* The command string to type. */
        .pcHelpString = "cls:\r\n Clears screen\r\n\r\n",
        .pxCommandInterpreter = cmd_clearScreen, /* The function to run. */
        .cExpectedNumberOfParameters = 0 /* No parameters are expected. */
    },
    {
        .pcCommand = "toggleled", /* The command string to type. */
        .pcHelpString = "toggleled n:\r\n toggles led n amount of times\r\n\r\n",
        .pxCommandInterpreter = cmd_toggle_led, /* The function to run. */
        .cExpectedNumberOfParameters = 1 /* No parameters are expected. */
    },
    {
        .pcCommand = "add", /* The command string to type. */
        .pcHelpString = "add n:\r\n add two numbers\r\n\r\n",
        .pxCommandInterpreter = cmd_add, /* The function to run. */
        .cExpectedNumberOfParameters = 2 /* 2 parameters are expected. */
    },
    {
        .pcCommand = NULL /* simply used as delimeter for end of array*/
    }
};

// Static list items for registration
static CLI_Definition_List_Item_t xCommandListItems[sizeof(xCommandList) / sizeof(xCommandList[0])];

void RegisterCommands(void) {
    for (size_t i = 0; i < sizeof(xCommandList) / sizeof(xCommandList[0]); i++) {
        CLIRegisterCommandStatic(&xCommandList[i], &xCommandListItems[i]);
    }
}

void vRegisterCLICommands(void){
    //itterate thourgh the list of commands and register them
    tx_mutex_create(&cli_mutex, NULL, TX_INHERIT);
    tx_queue_create(&cli_queue, "Queue", TX_1_ULONG, cli_queue_storage, sizeof(cli_queue_storage));

    for (size_t i = 0; i < sizeof(xCommandList) / sizeof(xCommandList[0]); i++) {
        CLIRegisterCommandStatic(&xCommandList[i], &xCommandListItems[i]);
    }
}

/*************************************************************************************************/
void cliWrite(const char *str)
{
   printf("%s", str);
   // flush stdout
   fflush(stdout);
}
/*************************************************************************************************/
void handleNewline(const char *const InputBuffer, char *OutputBuffer, uint8_t *cInputIndex)
{
    cliWrite("\r\n");

    int xMoreDataToFollow;
    do
    {     
        xMoreDataToFollow = CLIProcessCommand(InputBuffer, OutputBuffer, MAX_OUTPUT_SIZE);
        cliWrite(OutputBuffer);
    } while (xMoreDataToFollow != false);

    cliWrite(cli_prompt);
    *cInputIndex = 0;
    memset((void*)InputBuffer, 0x00, MAX_INPUT_SIZE);
}
/*************************************************************************************************/
void handleBackspace(uint8_t *cInputIndex, char *InputBuffer)
{
    if (*cInputIndex > 0)
    {
        (*cInputIndex)--;
        InputBuffer[*cInputIndex] = '\0';

        cliWrite((char *)backspace_tt);
    }
    else
    {
        uint8_t right[] = "\x1b\x5b\x43";
        cliWrite((char *)right);
    }
}
/*************************************************************************************************/
void handleCharacterInput(uint8_t *cInputIndex, char *InputBuffer)
{
    if (cRxedChar == '\r')
    {
        return;
    }
    else if (cRxedChar == (uint8_t)0x08 || cRxedChar == (uint8_t)0x7F)
    {
        handleBackspace(cInputIndex, InputBuffer);
    }
    else
    {
        if (*cInputIndex < MAX_INPUT_SIZE)
        {
            InputBuffer[*cInputIndex] = cRxedChar;
            (*cInputIndex)++;
        }
    }
}
/*************************************************************************************************/
void vCommandConsoleTask(void *pvParameters)
{
    (void)(pvParameters);
    uint8_t cInputIndex = 0; // simply used to keep track of the index of the input string
    uint32_t receivedValue;  // used to store the received value from the notification
    vRegisterCLICommands();
    
    for (;;)
    {
        // Wait for data from ISR
        tx_queue_receive(&cli_queue, &receivedValue, TX_WAIT_FOREVER);

        //echo recevied char
        cRxedChar = receivedValue & 0xFF;
        cliWrite((char *)&cRxedChar);
        if (cRxedChar == '\r' || cRxedChar == '\n')
        {
            // user pressed enter, process the command
            handleNewline(InputBuffer, OutputBuffer, &cInputIndex);
        }
        else
        {
            // user pressed a character add it to the input string
            handleCharacterInput(&cInputIndex, InputBuffer);
        }
    }
}


#endif /* CLI_COMMANDS_H */
