#include <sys/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <strings.h>

#include "cmsis_os2.h"
#include "stm32h5xx_nucleo.h"
#include "stm32h5xx_hal.h"
#include "cli_app.h"
#include "version.h"
#include "main.h"
#include "parameter_manager.h"
#include "rpc_server.h"
#include "command_station.h"
#include "decoder.h"
#include "susi.h"
#include "legacy_mode.h"

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
void susi_slave_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (strcasecmp(arg1,"start") == 0) {
        printf("Start SUSI Slave ...\n");
        SUSI_Slave_Start();
    }
    else if (strcasecmp(arg1,"stop") == 0) {
        printf("Stop SUSI Slave ...\n");
        printf("Stop SUSI Slave ...\n");
        SUSI_Slave_Stop();
    }
    else {
        printf("Unknown SUSI command: %s\n", arg1);
    }
}
void susi_master_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (strcasecmp(arg1,"start") == 0) {
        printf("Start SUSI Master ...\n");
        SUSI_Master_Start();
    }
    else if (strcasecmp(arg1,"stop") == 0) {
        printf("Stop SUSI Master ...\n");
        printf("Stop SUSI Master ...\n");
        SUSI_Master_Stop();
    }
    else {
        printf("Unknown SUSI command: %s\n", arg1);
    }
}
void command_station_command(const char *arg1, const char *arg2) {
    if (strcasecmp(arg1,"start") == 0) {
        uint8_t loop = 0;  // Default: no loop
        
        // Parse loop parameter
        if (arg2 != NULL && strlen(arg2) > 0) {
            if (strcasecmp(arg2, "loop") == 0 || strcasecmp(arg2, "loop1") == 0 || strcasecmp(arg2, "1") == 0) {
                loop = 1;
            } else if (strcasecmp(arg2, "loop2") == 0 || strcasecmp(arg2, "2") == 0) {
                loop = 2;
            } else if (strcasecmp(arg2, "loop3") == 0 || strcasecmp(arg2, "3") == 0) {
                loop = 3;
            } else if (strcasecmp(arg2, "0") == 0) {
                loop = 0;
            } else {
                printf("Unknown loop mode: %s (use 0, 1, 2, 3, loop, loop1, loop2, or loop3)\n", arg2);
                return;
            }
        }
        
        if (CommandStation_Start(loop)) {
            const char* loop_names[] = {"no loop", "loop1 (basic)", "loop2 (functions)", "loop3 (speed ramp)"};
            printf("Start Command Station with %s ...\n", loop_names[loop]);
        }
    }
    else if (strcasecmp(arg1,"stop") == 0) {
        if (CommandStation_Stop()) {
            printf("Command station stopped\n");
        }
    }
    else {
        printf("Unknown command station command: %s\n", arg1);
    }
}
void decoder_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (strcasecmp(arg1,"start") == 0) {
        printf("Start Decoder ...\n");
        Decoder_Start();
    }
    else if (strcasecmp(arg1,"stop") == 0) {
        printf("Stop Decoder ...\n");
        printf("Stop Decoder ...\n");
        Decoder_Stop();
    }
    else {
        printf("Unknown decoder command: %s\n", arg1);
    }
}
void rpc_server_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (strcasecmp(arg1,"start") == 0) {
        printf("Start RPC Server ...\n");
        RpcServer_Start(false);
    }
    else if (strcasecmp(arg1,"stop") == 0) {
        printf("Stop RPC Server ...\n");
        printf("Stop RPC Server ...\n");
        RpcServer_Stop();
    }
    else {
        printf("Unknown RPC Server command: %s\n", arg1);
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

void trigger_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    if (arg1[0]) {
        if (strcasecmp(arg1, "on") == 0 || strcasecmp(arg1, "1") == 0 || strcasecmp(arg1, "true") == 0) {
            printf("Enabling trigger on first bit ...\n");
            set_dcc_trigger_first_bit(1);
        } else if (strcasecmp(arg1, "off") == 0 || strcasecmp(arg1, "0") == 0 || strcasecmp(arg1, "false") == 0) {
            printf("Disabling trigger on first bit ...\n");
            set_dcc_trigger_first_bit(0);
        } else {
            printf("Invalid argument. Use: on/off, 1/0, or true/false\n");
        }
    } else {
        uint8_t trigger;
        if (get_dcc_trigger_first_bit(&trigger) == 0) {
            printf("Trigger on first bit: %s\n", trigger ? "enabled" : "disabled");
        } else {
            printf("Failed to read trigger setting\n");
        }
    }
}

void legacy_command(const char *arg1, const char *arg2) {
    if (strcasecmp(arg1, "start") == 0) {
        if (LegacyMode_Start()) {
            printf("Legacy mode started (reserved timer TIM%u, packet %s)\n",
                   LegacyMode_GetReservedTimer(),
                   LegacyMode_GetSelectedPacketName());
        } else {
            printf("Legacy mode failed to start or already running\n");
        }
    }
    else if (strcasecmp(arg1, "stop") == 0) {
        if (LegacyMode_Stop()) {
            printf("Legacy mode stopped\n");
        } else {
            printf("Legacy mode not running\n");
        }
    }
    else if (strcasecmp(arg1, "status") == 0 || arg1[0] == '\0') {
         printf("Legacy mode: %s (reserved timer TIM%u, mode %s, packet %s)\n",
               LegacyMode_IsRunning() ? "running" : "stopped",
               LegacyMode_GetReservedTimer(),
             LegacyMode_GetModeName(),
               LegacyMode_GetSelectedPacketName());
    }
    else if (strcasecmp(arg1, "timer") == 0) {
        uint8_t timer_id = (uint8_t)atoi(arg2);
        if (LegacyMode_SetReservedTimer(timer_id)) {
            printf("Legacy mode reserved timer set to TIM%u\n", LegacyMode_GetReservedTimer());
        } else {
            printf("Unsupported timer TIM%u. Allowed for now: TIM14\n", timer_id);
        }
    }
    else if (strcasecmp(arg1, "send") == 0) {
        uint8_t packet_id;
        if (strcasecmp(arg2, "reset") == 0) {
            packet_id = LEGACY_PACKET_RESET;
        } else if (strcasecmp(arg2, "idle") == 0) {
            packet_id = LEGACY_PACKET_IDLE;
        } else if (strcasecmp(arg2, "hard") == 0) {
            packet_id = LEGACY_PACKET_HARD;
        } else if (strcasecmp(arg2, "base") == 0) {
            packet_id = LEGACY_PACKET_BASE;
        } else {
            printf("Unknown legacy packet '%s'. Use reset|idle|hard|base\n", arg2);
            return;
        }

        if (LegacyMode_SelectPacket(packet_id)) {
            printf("Legacy packet selected: %s\n", LegacyMode_GetSelectedPacketName());
        } else {
            printf("Failed to select legacy packet\n");
        }
    }
    else if (strcasecmp(arg1, "profile") == 0) {
        if (strcasecmp(arg2, "sender_v3") == 0) {
            (void)LegacyMode_Stop();

            if (!LegacyMode_SetReservedTimer(14)) {
                printf("Failed to apply sender_v3 profile (timer)\n");
                return;
            }

            if (!LegacyMode_SelectPacket(LEGACY_PACKET_IDLE)) {
                printf("Failed to apply sender_v3 profile (packet)\n");
                return;
            }

            printf("Applied profile sender_v3: timer TIM%u, mode %s, packet %s, state stopped\n",
                   LegacyMode_GetReservedTimer(),
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName());
        } else if (strcasecmp(arg2, "sender_v3_test") == 0) {
            (void)LegacyMode_Stop();

            if (!LegacyMode_SetReservedTimer(14)) {
                printf("Failed to apply sender_v3_test profile (timer)\n");
                return;
            }

            if (!LegacyMode_ApplyCompatKey('w')) {
                printf("Failed to apply sender_v3_test profile (warble start)\n");
                return;
            }

            printf("Applied profile sender_v3_test: timer TIM%u, mode %s, packet %s, state %s\n",
                   LegacyMode_GetReservedTimer(),
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName(),
                   LegacyMode_IsRunning() ? "running" : "stopped");
        } else {
            printf("Unknown profile '%s'. Available: sender_v3|sender_v3_test\n", arg2);
        }
    }
    else if (strcasecmp(arg1, "key") == 0) {
        const char key_cmd = arg2[0];
        if (key_cmd == '\0') {
            printf("Usage: legacy key <r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h>\n");
            return;
        }

        if (key_cmd == 'h') {
            printf("Legacy key map:\n");
            printf("  r=reset, R=hard reset, i=idle, d=base(dcc), D=stretched dcc, w=warble\n");
            printf("  a=scopeA, b=scopeB, o=scope timing, 0=zeros, 1=ones\n");
            printf("  c/C=clock, S=stretched zeros, e=estop-like, f=fw toggle, k=kickstart-like, q=stop\n");
            return;
        }

        if (LegacyMode_ApplyCompatKey(key_cmd)) {
            printf("Legacy key '%c' applied. State: %s, mode: %s, packet: %s\n",
                   key_cmd,
                   LegacyMode_IsRunning() ? "running" : "stopped",
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName());
        } else {
            printf("Unsupported legacy key '%c'. Use r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h\n", key_cmd);
        }
    }
    else {
        printf("Unknown legacy command: %s\n", arg1);
        printf("Usage: legacy <start|stop|status|timer|send|profile|key> [14|reset|idle|hard|base|sender_v3|sender_v3_test|r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h]\n");
    }
}



void hello_command(const char *arg1, const char *arg2) {
    (void)arg2; // Unused
    printf("Hello, %s!\n", arg1[0] ? arg1 : "ThreadX User");
}

void status_command(const char *arg1, const char *arg2) {
    printf("System Status: %s %s\n", arg1[0] ? arg1 : "OK", arg2[0] ? arg2 : "");
}

void reboot_command(const char *arg1, const char *arg2) {
    (void)arg1; // Unused
    (void)arg2; // Unused
    printf("rebooting ...\n");
    osDelay(500);
    // Software reset
    HAL_NVIC_SystemReset();    
}

void reset_command(const char *arg1, const char *arg2) {
    (void)arg1; // Unused
    (void)arg2; // Unused
    parameter_manager_factory_reset();
    printf("System reset complete.\n");
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
Command cmd_trigger = {
    .name = "trigger",
    .execute = trigger_command,
    .help = "Trigger First Packet Bit: trigger <on|off|1|0>",
    .next = &cmd_bidi
};
Command cmd_rpcs = {
    .name = "rpc_server", 
    .execute = rpc_server_command,
    .help = "RPC Server: rpc_server <start|stop>",
    .next = &cmd_trigger
};
Command cmd_legacy = {
    .name = "legacy",
    .execute = legacy_command,
    .help = "Legacy sender mode: legacy <start|stop|status|timer|send|profile|key> [14|reset|idle|hard|base|sender_v3|sender_v3_test|r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h]",
    .next = &cmd_rpcs
};
Command cmd_cms = {
    .name = "cms", 
    .execute = command_station_command,
    .help = "Command Station: cms <start|stop> [0|1|2|3|loop|loop1|loop2|loop3]",
    .next = &cmd_legacy
};
Command cmd_dec = {
    .name = "dec", 
    .execute = decoder_command,
    .help = "Decoder: dec <start|stop>",
    .next = &cmd_cms
};
Command cmd_susi_slave = {
    .name = "susi_slave", 
    .execute = susi_slave_command,
    .help = "SUSI Slave start/stop",
    .next = &cmd_dec
};
Command cmd_susi_master = {
    .name = "susi_master", 
    .execute = susi_master_command,
    .help = "SUSI Master start/stop",
    .next = &cmd_susi_slave
};
Command cmd_hello = {
    .name ="hello", 
    .execute = hello_command,
    .help = NULL,
    .next = &cmd_susi_master
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
Command cmd_reboot = {
    .name = "reboot",
    .execute = reboot_command,
    .help = "Reboot system",
    .next = &cmd_date_time
};
Command cmd_reset = {
    .name = "reset",
    .execute = reset_command,
    .help = "Factory reset",
    .next = &cmd_reboot
};

Command *command_list = &cmd_reset;

static void print_help(void) {
    Command *current = command_list;
    printf("Available commands:\n");
    while (current != NULL) {
        printf("  %s\n", current->name);
        if (current->help) {
            printf("    %s\n", current->help);
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
    uint32_t receivedChar;  // used to store the received value from the notification
    char N_char = '\n';
    commandQueue = osMessageQueueNew(5, sizeof(uint32_t), NULL);

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
            InputBuffer[inputIndex++] = (char)receivedChar;
            InputBuffer[inputIndex] = '\0'; // Null terminate the string
            _write(0, (char *)&receivedChar, 1); // Echo the character to the console
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
