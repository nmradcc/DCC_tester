#include <sys/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "cmsis_os2.h"
#include "stm32h5xx_hal.h"
#include "legacy_mode.h"
#include "cli_app.h"
#include "cli_legacy_app.h"

int _write(int file, char *ptr, int len);

typedef struct {
    char command[32];
    char arg1[32];
    char arg2[32];
} ParsedInput;

static osThreadId_t legacy_cli_task_handle = NULL;
static osMessageQueueId_t legacy_command_queue = NULL;
static char input_buffer[64];
static unsigned int input_index = 0;
static ParsedInput parsed = {0};

static const osThreadAttr_t legacy_cli_task_attributes = {
    .name = "legacyCliTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 1536 * 4
};

static void parse_input(const char *input, ParsedInput *out_parsed) {
    memset(out_parsed, 0, sizeof(*out_parsed));
    sscanf(input, "%31s %31s %31s", out_parsed->command, out_parsed->arg1, out_parsed->arg2);
}

static void print_legacy_help(void) {
    printf("Legacy CLI commands:\n");
    printf("  help\n");
    printf("  start\n");
    printf("  stop\n");
    printf("  status\n");
    printf("  timer <14>\n");
    printf("  send <reset|idle|hard|base>\n");
    printf("  profile <sender_v3|sender_v3_test>\n");
    printf("  key <r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h>\n");
    printf("  reset\n");
    printf("Only reset returns to standard CLI.\n");
}

static void execute_legacy_command(const char *arg1, const char *arg2) {
    if (strcasecmp(arg1, "start") == 0) {
        if (LegacyMode_Start()) {
            printf("Legacy mode started (reserved timer TIM%u, packet %s, startup cfg %s, manual %s, type %c, log_pkts %s)\n",
                   LegacyMode_GetReservedTimer(),
                   LegacyMode_GetSelectedPacketName(),
                   LegacyMode_GetStartupConfigName(),
                   LegacyMode_GetStartupManual() ? "true" : "false",
                   LegacyMode_GetStartupDecoderType(),
                   LegacyMode_GetStartupLogPkts() ? "true" : "false");
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
         printf("Legacy mode: %s (reserved timer TIM%u, mode %s, packet %s, startup cfg %s, manual %s, type %c, log_pkts %s)\n",
               LegacyMode_IsRunning() ? "running" : "stopped",
               LegacyMode_GetReservedTimer(),
               LegacyMode_GetModeName(),
             LegacyMode_GetSelectedPacketName(),
             LegacyMode_GetStartupConfigName(),
             LegacyMode_GetStartupManual() ? "true" : "false",
             LegacyMode_GetStartupDecoderType(),
             LegacyMode_GetStartupLogPkts() ? "true" : "false");
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
            printf("Usage: key <r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|f|k|q|h>\n");
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
        printf("Unknown command: %s\n", arg1);
        print_legacy_help();
    }
}

static void vLegacyCommandConsoleTask(void *pvParameters)
{
    (void)pvParameters;
    uint32_t received_char;
    char n_char = '\n';

    printf("Legacy CLI active. Type 'help' for commands.\n");

    for (;;) {
        osMessageQueueGet(legacy_command_queue, &received_char, NULL, osWaitForever);

        if (received_char == '\b' || received_char == 0x7F) {
            if (input_index > 0) {
                input_index--;
                input_buffer[input_index] = '\0';
                _write(0, "\b \b", 3);
            }
        }
        else if (input_index < sizeof(input_buffer) - 1) {
            input_buffer[input_index++] = (char)received_char;
            input_buffer[input_index] = '\0';
            _write(0, (char *)&received_char, 1);

            if (received_char == '\r' || received_char == '\n') {
                input_buffer[input_index - 1] = '\0';
                input_index = 0;
                _write(0, &n_char, 1);

                parse_input(input_buffer, &parsed);

                if (strcmp(parsed.command, "help") == 0) {
                    print_legacy_help();
                } else if (strcmp(parsed.command, "reset") == 0) {
                    printf("Resetting system...\n");
                    osDelay(100);
                    HAL_NVIC_SystemReset();
                } else if (parsed.command[0] != '\0') {
                    execute_legacy_command(parsed.command, parsed.arg1);
                }

                memset(input_buffer, 0, sizeof(input_buffer));
                memset(&parsed, 0, sizeof(parsed));
                input_index = 0;
            }
        }
    }
}

void CliLegacyApp_Start(void) {
    if (legacy_command_queue == NULL) {
        legacy_command_queue = osMessageQueueNew(5, sizeof(uint32_t), NULL);
    }

    CliApp_SetActiveCommandQueue(legacy_command_queue);

    if (legacy_cli_task_handle == NULL) {
        legacy_cli_task_handle = osThreadNew(vLegacyCommandConsoleTask, NULL, &legacy_cli_task_attributes);
    }
}
