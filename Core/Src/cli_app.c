#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <sys/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "cli_app.h"
#include "version.h"
#include "main.h"
#include "command_station.h"
#include "decoder.h"

#include "lwip/netif.h"
#include "lwip/dhcp.h"
#include "lwip/ip_addr.h"
#include "lwip/inet.h"

extern struct netif gnetif; // Your network interface

// Declare _write prototype to avoid implicit declaration error
int _write(int file, char *ptr, int len);

typedef struct Command {
    const char *name;
    void (*execute)(const char *arg1, const char *arg2);
    const char *help; // Optional help text
    struct Command *next;
} Command;

typedef struct {
    char command[32];
    char arg1[32];
    char arg2[32];
} ParsedInput;

osMessageQueueId_t commandQueue;

static char InputBuffer[64];
static char OutputBuffer[32];
static unsigned int inputIndex = 0;
static ParsedInput parsed = {0};

static void print_help(void);

void uart_receive_callback(char *input) {
    osMessageQueuePut(commandQueue, input, 0, 0);
}


// Command implementations
void help_command(const char *arg1, const char *arg2) {
    (void)arg1; // Unused
    (void)arg2; // Unused
    printf("Firmware version: %s\n", FW_VERSION_STRING);
    print_help();
}

void command_station_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (stricmp(arg1,"start") == 0) {
        if (stricmp(arg2, "bidi") == 0) {
            CommandStation_Start(true);
        } else {
            CommandStation_Start(false);
        }
        printf("Start Command Station ...\n");
    }
    else if (stricmp(arg1,"stop") == 0) {
        printf("Stop Command Station ...\n");
        CommandStation_Stop();
    }
    else {
        printf("Unknown command station command: %s\n", arg1);
    }
}

void decoder_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (stricmp(arg1,"start") == 0) {
        printf("Start Decoder ...\n");
        Decoder_Start();
    }
    else if (stricmp(arg1,"stop") == 0) {
        printf("Stop Decoder ...\n");
        Decoder_Stop();
    }
    else {
        printf("Unknown decoder command: %s\n", arg1);
    }
}

void bidi_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (arg1[0]) {
//TODO: add range check
        uint16_t threshold = (uint16_t)atoi(arg1);
        printf("Setting BiDi threshold to %d ...\n", threshold);
        CommandStation_bidi_Threshold(threshold);
    } else {
        printf("Setting BiDi threshold to default ...\n");
        CommandStation_bidi_Threshold(DEFAULT_BIDIR_THRESHOLD);
    }
}



void hello_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    printf("Hello, %s!\n", arg1[0] ? arg1 : "ThreadX User");
}

void status_command(const char *arg1, const char *arg2) {
    printf("System Status: %s %s\n", arg1[0] ? arg1 : "OK", arg2[0] ? arg2 : "");
    if (dhcp_supplied_address(&gnetif)) {
        printf("DHCP assigned IP: %s\n", ipaddr_ntoa(&gnetif.ip_addr));
    } else {
        printf("DHCP has not assigned an IP yet.\n");
    }
}

void set_command(const char *arg1, const char *arg2) {
    printf("Setting %s to %s\n", arg1[0] ? arg1 : "default", arg2[0] ? arg2 : "value");
}

void date_time_command(const char *arg1, const char *arg2) {
    (void)arg1; // Unused
    (void)arg2; // Unused
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    printf("Current Date: 20%02d-%02d-%02d  Time: %02d:%02d:%02d\n",
           sDate.Year, sDate.Month, sDate.Date,
           sTime.Hours, sTime.Minutes, sTime.Seconds);
}

// Statically Register commands in a linked list!
Command cmd_help = {
    .name = "help", 
    .execute = help_command,
    .help = NULL, 
    .next = NULL
};
Command cmd_bidi = {
    .name = "bidi", 
    .execute = bidi_command,
    .help = "BiDi Threshold: bidi <value>",
    .next = &cmd_help

};
Command cmd_cms = {
    .name = "cms", 
    .execute = command_station_command,
    .help = "Command Station: cms <start|stop> [bidi]",
    .next = &cmd_bidi
};
Command cmd_dec = {
    .name = "dec", 
    .execute = decoder_command,
    .help = "Decoder: dec <start|stop>",
    .next = &cmd_cms
};
Command cmd_hello = {
    .name ="hello", 
    .execute = hello_command,
    .help = NULL,
    .next = &cmd_dec
};
Command cmd_status = {
    .name = "status",
    .execute = status_command,
    .help = NULL,
    .next = &cmd_hello
};
Command cmd_date_time = {
    .name = "date_time",
    .execute = date_time_command,
    .help = "Get current date and time",
    .next = &cmd_status
};
Command cmd_set = {
    .name = "set",
    .execute = set_command,
    .help = NULL,
    .next = &cmd_date_time
};

Command *command_list = &cmd_set;

static void print_help(void) {
    Command *current = command_list;
    printf("Available commands:\n");
    while (current != NULL) {
        printf("  %s ", current->name);
        if (current->help) {
            printf("\t%s\n", current->help);
        }
        else {
            printf("\n");
        }
        current = current->next;
    }
    printf("Type 'help' for this message.\n");
}

static void parse_input(const char *input, ParsedInput *out_parsed) {
    sscanf(input, "%s %31s %31s", out_parsed->command, out_parsed->arg1, out_parsed->arg2);
}

void vCommandConsoleTask(void *pvParameters)
{
    (void)(pvParameters);
    char receivedChar;  // used to store the received value from the notification
    char N_char = '\n';

    commandQueue = osMessageQueueNew(6, sizeof(char), NULL);

    osDelay(2000); // Wait for system to initialize
    
    print_help(); // Print help on startup

    for (;;)
    {
        // Wait for data from ISR
        osMessageQueueGet(commandQueue, &receivedChar, NULL, osWaitForever);
        if (receivedChar == '\b' || receivedChar == 0x7F) {
            // user pressed backspace, remove last character from input string
            if (inputIndex > 0) {
                inputIndex--;
                InputBuffer[inputIndex] = '\0'; // Null terminate the string
                sprintf(OutputBuffer,"\b \b"); // Move cursor back, print space to overwrite, and move back again
                _write(0, OutputBuffer, (int)strlen(OutputBuffer)); // Echo backspace to console
            }
        }
        else if (inputIndex < sizeof(InputBuffer) - 1) {
            // user pressed a character add it to the input string
            InputBuffer[inputIndex++] = receivedChar;
            InputBuffer[inputIndex] = '\0'; // Null terminate the string
            _write(0, &receivedChar, 1); // Echo the character to the console
            if (receivedChar == '\r' || receivedChar == '\n') {
                // Process the command when Enter is pressed
                InputBuffer[inputIndex - 1] = '\0'; // Null terminate the string
                inputIndex = 0; // Reset input index for next command
                _write(0, &N_char, 1); // Echo the character to the console
                // Here you can add code to parse and execute the command

                parse_input(InputBuffer, &parsed);
    
                Command *current = command_list;
                bool command_found = false;
                while (current != NULL) {
                    if (strcmp(parsed.command, current->name) == 0) {
                        current->execute(parsed.arg1, parsed.arg2);
                        command_found = true;
                        break;
                    }
                    current = current->next;
                }
                if (!command_found) {
//                    printf("Unknown command: %s\n", parsed.command);
                }
                memset(InputBuffer, 0, sizeof(InputBuffer)); // Clear the input buffer
                memset(&parsed, 0, sizeof(parsed)); // Reset parsed input
                inputIndex = 0; // Reset input index for next command
            }   
        }
        else if (inputIndex >= sizeof(InputBuffer) - 1) {
            // input buffer is full, ignore further input
        }
    }
}


#endif /* CLI_COMMANDS_H */
