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
#include "cli_legacy_app.h"
#include "version.h"
#include "main.h"
#include "parameter_manager.h"
#include "rpc_server.h"
#include "command_station.h"
#include "decoder.h"
#include "susi.h"
#include "app_filex.h"

int _write(int file, char *ptr, int len);

typedef struct Command {
    const char *name;
    void (*execute)(const char *arg1, const char *arg2);
    const char *help;
    struct Command *next;
} Command;

typedef struct {
    char command[32];
    char arg1[32];
    char arg2[32];
} ParsedInput;

static osMessageQueueId_t active_command_queue = NULL;
static osMessageQueueId_t standard_command_queue = NULL;

static char InputBuffer[64];
static char OutputBuffer[32];
static unsigned int inputIndex = 0;
static ParsedInput parsed = {0};

static void print_help(void);

void CliApp_SetActiveCommandQueue(osMessageQueueId_t queue) {
    active_command_queue = queue;
}

osMessageQueueId_t CliApp_GetActiveCommandQueue(void) {
    return active_command_queue;
}

void uart_receive_callback(char *input) {
    if (active_command_queue != NULL) {
        osMessageQueuePut(active_command_queue, input, 0, 0);
    }
}

void help_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    printf("Firmware version: %s\n", FW_VERSION_STRING);
    print_help();
}

void susi_slave_command(const char *arg1, const char *arg2) {
    (void)arg2;
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
    (void)arg2;
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
        uint8_t loop = 0;

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
    (void)arg2;
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
    (void)arg2;
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
    (void)arg2;
    if (arg1[0]) {
        uint16_t threshold = (uint16_t)atoi(arg1);
        printf("Setting BiDi threshold to %d ...\n", threshold);
        CommandStation_bidi_Threshold(threshold);
    } else {
        printf("Setting BiDi threshold to default ...\n");
        CommandStation_bidi_Threshold(DEFAULT_BIDIR_THRESHOLD);
    }
}

void trigger_command(const char *arg1, const char *arg2) {
    (void)arg2;
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

void enter_legacy_mode_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    CliLegacyApp_Start();
    osDelay(20);
    printf("Reset to return.\n");
    osThreadTerminate(osThreadGetId());
}

void hello_command(const char *arg1, const char *arg2) {
    (void)arg2;
    printf("Hello, %s!\n", arg1[0] ? arg1 : "ThreadX User");
}

void status_command(const char *arg1, const char *arg2) {
    printf("System Status: %s %s\n", arg1[0] ? arg1 : "OK", arg2[0] ? arg2 : "");
}

void reboot_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    printf("rebooting ...\n");
    osDelay(500);
    HAL_NVIC_SystemReset();
}

void reset_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    parameter_manager_factory_reset();
    printf("System reset complete.\n");
}

void date_time_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    printf("Current Date: 20%02d-%02d-%02d  Time: %02d:%02d:%02d\n",
           sDate.Year, sDate.Month, sDate.Date,
           sTime.Hours, sTime.Minutes, sTime.Seconds);
}

void sd_eject_command(const char *arg1, const char *arg2) {
    (void)arg1;
    (void)arg2;

    UINT status = AppFileX_CloseMediaIfIdleOnSd();
    if (status == FX_SUCCESS) {
        printf("SD media closed. Card can be removed safely.\n");
    } else {
        printf("SD eject failed (FileX status=%u).\n", (unsigned int)status);
    }
}

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
Command cmd_cms = {
    .name = "cms",
    .execute = command_station_command,
    .help = "Command Station: cms <start|stop> [0|1|2|3|loop|loop1|loop2|loop3]",
    .next = &cmd_rpcs
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
Command cmd_enter_legacy = {
    .name = "legacy",
    .execute = enter_legacy_mode_command,
    .help = "Enter legacy mode CLI (no return except reset)",
    .next = &cmd_reset
};
Command cmd_sd_eject = {
    .name = "sd_eject",
    .execute = sd_eject_command,
    .help = "Flush/close SD media for safe card removal",
    .next = &cmd_enter_legacy
};

Command *command_list = &cmd_sd_eject;

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
    memset(out_parsed, 0, sizeof(*out_parsed));
    sscanf(input, "%31s %31s %31s", out_parsed->command, out_parsed->arg1, out_parsed->arg2);
}

void vCommandConsoleTask(void *pvParameters)
{
    (void)(pvParameters);
    uint32_t receivedChar;
    char N_char = '\n';

    standard_command_queue = osMessageQueueNew(5, sizeof(uint32_t), NULL);
    CliApp_SetActiveCommandQueue(standard_command_queue);

    osDelay(2000);

    print_help();

    for (;;)
    {
        osMessageQueueGet(standard_command_queue, &receivedChar, NULL, osWaitForever);
        if (receivedChar == '\b' || receivedChar == 0x7F) {
            if (inputIndex > 0) {
                inputIndex--;
                InputBuffer[inputIndex] = '\0';
                sprintf(OutputBuffer,"\b \b");
                _write(0, OutputBuffer, (int)strlen(OutputBuffer));
            }
        }
        else if (inputIndex < sizeof(InputBuffer) - 1) {
            InputBuffer[inputIndex++] = (char)receivedChar;
            InputBuffer[inputIndex] = '\0';
            _write(0, (char *)&receivedChar, 1);
            if (receivedChar == '\r' || receivedChar == '\n') {
                InputBuffer[inputIndex - 1] = '\0';
                inputIndex = 0;
                _write(0, &N_char, 1);

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
                }
                memset(InputBuffer, 0, sizeof(InputBuffer));
                memset(&parsed, 0, sizeof(parsed));
                inputIndex = 0;
            }
        }
        else if (inputIndex >= sizeof(InputBuffer) - 1) {
        }
    }
}
