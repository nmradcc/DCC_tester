#include <sys/unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>

#include "cmsis_os2.h"
#include "stm32h5xx_hal.h"
#include "legacy_mode.h"
#include "app_filex.h"
#include "cli_app.h"
#include "cli_legacy_app.h"
#include "command_station.h"
#include "version.h"
#include "main.h"

int _write(int file, char *ptr, int len);

typedef struct {
    char command[32];
    char arg1[32];
    char arg2[96];
} ParsedInput;

typedef struct {
    bool active;
    uint8_t step;
    bool collecting_comments;
    char start_args[128];
    char log_base[32];
    char answers[6][64];
    uint8_t comment_count;
    char comments[16][96];
} StartPromptState;

static osThreadId_t legacy_cli_task_handle = NULL;
static osMessageQueueId_t legacy_command_queue = NULL;
static char input_buffer[192];
static unsigned int input_index = 0;
static ParsedInput parsed = {0};
static StartPromptState start_prompt = {0};

static const char *start_questions[] = {
    "Enter Manufacturer > ",
    "Enter Model number > ",
    "Enter Version number > ",
    "Enter Modified CVs > ",
    "Enter Booster type > ",
    "Enter Track Direction > "
};

static const char *start_log_labels[] = {
    "Manufacturer",
    "Model number",
    "Version number",
    "Modified CVs",
    "Booster type",
    "Track Direction"
};

static const osThreadAttr_t legacy_cli_task_attributes = {
    .name = "legacyCliTask",
    .priority = (osPriority_t) osPriorityNormal,
    .stack_size = 1536 * 4
};

static void parse_input(const char *input, ParsedInput *out_parsed) {
    int consumed = 0;

    memset(out_parsed, 0, sizeof(*out_parsed));

    if (sscanf(input, " %31s %31s %n", out_parsed->command, out_parsed->arg1, &consumed) >= 2) {
        const char* tail = input + consumed;
        while (*tail == ' ' || *tail == '\t') {
            ++tail;
        }

        (void)snprintf(out_parsed->arg2, sizeof(out_parsed->arg2), "%s", tail);
    } else {
        (void)sscanf(input, " %31s", out_parsed->command);
    }
}

static bool start_args_has_option(const char *args, const char *option) {
    const char *p;
    size_t opt_len;

    if ((args == NULL) || (option == NULL) || (option[0] == '\0')) {
        return false;
    }

    opt_len = strlen(option);
    p = args;
    while (*p != '\0') {
        while ((*p == ' ') || (*p == '\t')) {
            ++p;
        }

        if (*p == '\0') {
            break;
        }

        if ((strncmp(p, option, opt_len) == 0) &&
            ((p[opt_len] == '\0') || (p[opt_len] == ' ') || (p[opt_len] == '\t'))) {
            return true;
        }

        while ((*p != '\0') && (*p != ' ') && (*p != '\t')) {
            ++p;
        }
    }

    return false;
}

static void print_legacy_help(void) {
    printf("Legacy CLI commands:\n");
    printf("  help\n");
    printf("  start\n");
    printf("    Sender args: -m -a <addr> -d <l|f|a|s> -n <pre> -N <trig> -l -p <port>\n");
    printf("                 -f -x -r -t <mask> -c <mask> -g <mask> -E <pre> -T\n");
    printf("                 -F <fill> -R <reps> -P -A -s -S -o <pair> -L -k -D -u -e <trg>\n");
    printf("  stop\n");
    printf("  status\n");
    printf("  cfg\n");
    printf("  sd_eject\n");
    printf("  send <reset|idle|hard|base>\n");
    printf("  profile <sender_v3|sender_v3_test>\n");
    printf("  key <r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|E|f|k|q|u|s|t|z|g|h>\n");
    printf("  reset\n");
    printf("Only reset returns to standard CLI.\n");
}

static void sanitize_log_base(char *name) {
    size_t i;

    for (i = 0; name[i] != '\0'; ++i) {
        const char c = name[i];
        if ((c == ' ') || (c == '\\') || (c == '/') || (c == ':') || (c == '*') ||
            (c == '?') || (c == '"') || (c == '<') || (c == '>') || (c == '|')) {
            name[i] = '_';
        }
    }
}

static void format_status_timestamped_line(char *out, size_t out_size, const char *status_payload) {
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    static const char * const weekday_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    static const char * const month_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    const char *weekday = "Mon";
    const char *month = "Jan";

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    if ((sDate.WeekDay >= RTC_WEEKDAY_MONDAY) && (sDate.WeekDay <= RTC_WEEKDAY_SUNDAY)) {
        weekday = weekday_names[sDate.WeekDay - RTC_WEEKDAY_MONDAY];
    }
    if ((sDate.Month >= 1U) && (sDate.Month <= 12U)) {
        month = month_names[sDate.Month - 1U];
    }

    (void)snprintf(out,
                   out_size,
                   "<%s %s %02u %02u:%02u:%02u 20%02u> STATUS  %s\n",
                   weekday,
                   month,
                   (unsigned int)sDate.Date,
                   (unsigned int)sTime.Hours,
                   (unsigned int)sTime.Minutes,
                   (unsigned int)sTime.Seconds,
                   (unsigned int)sDate.Year,
                   status_payload);
}

static bool write_start_questionnaire_files(void) {
    char log_name[48];
    char sum_name[48];
    char line[160];
    UINT status;
    uint8_t i;

    (void)snprintf(log_name, sizeof(log_name), "%s.log", start_prompt.log_base);
    (void)snprintf(sum_name, sizeof(sum_name), "%s.sum", start_prompt.log_base);

    format_status_timestamped_line(line, sizeof(line), "BEGINNING decoder test log");
    status = AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);
    if (status != FX_SUCCESS) {
        printf("Failed to write log file '%s' (status=%u)\n", log_name, (unsigned int)status);
        return false;
    }

    format_status_timestamped_line(line, sizeof(line), "Test software version " FW_VERSION_STRING);
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);
    format_status_timestamped_line(line, sizeof(line), "File <DCC_tester firmware>");
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);
    format_status_timestamped_line(line, sizeof(line), "CRC 0, Length 0");
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);

    format_status_timestamped_line(line, sizeof(line), "BEGINNING decoder test log");
    status = AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);
    if (status != FX_SUCCESS) {
        printf("Failed to write summary file '%s' (status=%u)\n", sum_name, (unsigned int)status);
        return false;
    }

    format_status_timestamped_line(line, sizeof(line), "Test software version " FW_VERSION_STRING);
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);
    format_status_timestamped_line(line, sizeof(line), "File <DCC_tester firmware>");
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);
    format_status_timestamped_line(line, sizeof(line), "CRC 0, Length 0");
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);

    for (i = 0U; i < (uint8_t)(sizeof(start_log_labels) / sizeof(start_log_labels[0])); ++i) {
        (void)snprintf(line, sizeof(line), "%s: %s\n", start_log_labels[i], start_prompt.answers[i]);
        (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);
        (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);
    }

    (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)"--------------------------------\n", 0U);
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)"--------------------------------\n", 0U);
    for (i = 0U; i < start_prompt.comment_count; ++i) {
        (void)snprintf(line, sizeof(line), "%s\n", start_prompt.comments[i]);
        (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)line, 0U);
        (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)line, 0U);
    }
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)log_name, (const CHAR *)"--------------------------------\n", 0U);
    (void)AppFileX_AppendTextFileOnSd((const CHAR *)sum_name, (const CHAR *)"--------------------------------\n", 0U);

    if (!LegacyMode_AppendStartupSummaryToLogs(log_name, sum_name)) {
        printf("Warning: failed to append startup summary block to log/sum files.\n");
    }

    return true;
}

static void begin_start_questionnaire(const char *start_args) {
    memset(&start_prompt, 0, sizeof(start_prompt));
    start_prompt.active = true;
    start_prompt.step = 0U;
    (void)snprintf(start_prompt.start_args, sizeof(start_prompt.start_args), "%s", start_args);

    printf("Enter base of log and statistics file name > ");
    (void)fflush(stdout);
}

static void finish_start_sequence(void) {
    char start_arg_error[128];

    start_prompt.active = false;

    start_arg_error[0] = '\0';
    if (!LegacyMode_SetStartArgs(start_prompt.start_args, start_arg_error, sizeof(start_arg_error))) {
        printf("Invalid start args: %s\n", start_arg_error[0] != '\0' ? start_arg_error : "parse error");
        return;
    }

    if (!write_start_questionnaire_files()) {
        printf("Warning: could not write startup questionnaire files, continuing to start tests.\n");
    }

    if (!LegacyMode_SetLogBaseName(start_prompt.log_base)) {
        printf("Warning: invalid log base for runtime logs, using default filenames.\n");
    }

    if (LegacyMode_Start()) {
        printf("Legacy mode started (packet %s, startup cfg %s, manual %s, type %c, log_pkts %s)\n",
               LegacyMode_GetSelectedPacketName(),
               LegacyMode_GetStartupConfigName(),
               LegacyMode_GetStartupManual() ? "true" : "false",
               LegacyMode_GetStartupDecoderType(),
               LegacyMode_GetStartupLogPkts() ? "true" : "false");
    } else {
        printf("Legacy mode failed to start or already running\n");
    }
}

static bool handle_start_questionnaire_line(const char *line) {
    char line_copy[192];
    char *trimmed;
    size_t len;

    if (!start_prompt.active) {
        return false;
    }

    (void)snprintf(line_copy, sizeof(line_copy), "%s", line);
    trimmed = line_copy;
    while ((*trimmed == ' ') || (*trimmed == '\t')) {
        ++trimmed;
    }
    len = strlen(trimmed);
    while ((len > 0U) && ((trimmed[len - 1U] == ' ') || (trimmed[len - 1U] == '\t'))) {
        trimmed[len - 1U] = '\0';
        --len;
    }

    if (start_prompt.step == 0U) {
        if (trimmed[0] == '\0') {
            printf("Base log name cannot be empty. Enter base of log and statistics file name > ");
            (void)fflush(stdout);
            return true;
        }

        (void)snprintf(start_prompt.log_base, sizeof(start_prompt.log_base), "%.31s", trimmed);
        sanitize_log_base(start_prompt.log_base);
        start_prompt.step = 1U;
        printf("%s", start_questions[0]);
        (void)fflush(stdout);
        return true;
    }

    if (start_prompt.step <= (uint8_t)(sizeof(start_questions) / sizeof(start_questions[0]))) {
        const uint8_t idx = (uint8_t)(start_prompt.step - 1U);
        (void)snprintf(start_prompt.answers[idx], sizeof(start_prompt.answers[idx]), "%.63s", trimmed);
        start_prompt.step++;

        if (start_prompt.step <= (uint8_t)(sizeof(start_questions) / sizeof(start_questions[0]))) {
            printf("%s", start_questions[start_prompt.step - 1U]);
            (void)fflush(stdout);
        } else {
            start_prompt.collecting_comments = true;
            puts("Enter comments.  Begin line with '.' to end input");
            puts("--------------------------------");
            printf("comments> ");
            (void)fflush(stdout);
        }
        return true;
    }

    if (start_prompt.collecting_comments) {
        if (strcmp(trimmed, ".") == 0) {
            puts("--------------------------------");
            (void)fflush(stdout);
            finish_start_sequence();
            return true;
        }

        if (start_prompt.comment_count < (uint8_t)(sizeof(start_prompt.comments) / sizeof(start_prompt.comments[0]))) {
            (void)snprintf(start_prompt.comments[start_prompt.comment_count],
                           sizeof(start_prompt.comments[start_prompt.comment_count]),
                           "%.95s",
                           trimmed);
            start_prompt.comment_count++;
        }
        printf("comments> ");
        (void)fflush(stdout);
        return true;
    }

    return true;
}

static void execute_legacy_command(const char *command, const char *arg1, const char *arg2) {
    if (strcasecmp(command, "start") == 0) {
        char start_args[128];

        start_args[0] = '\0';
        if (arg1[0] != '\0' && arg2[0] != '\0') {
            (void)snprintf(start_args, sizeof(start_args), "%s %s", arg1, arg2);
        } else if (arg1[0] != '\0') {
            (void)snprintf(start_args, sizeof(start_args), "%s", arg1);
        } else if (arg2[0] != '\0') {
            (void)snprintf(start_args, sizeof(start_args), "%s", arg2);
        }

        if (start_args_has_option(start_args, "-u")) {
            if (LegacyMode_WriteUserDocsToSd()) {
                printf("Wrote sender user documentation to s_user.txt\n");
            } else {
                printf("Failed to write s_user.txt\n");
            }
            return;
        }

        begin_start_questionnaire(start_args);
    }
    else if (strcasecmp(command, "stop") == 0) {
        if (LegacyMode_Stop()) {
            printf("Legacy mode stopped\n");
        } else {
            printf("Legacy mode not running\n");
        }
    }
    else if (strcasecmp(command, "status") == 0 || command[0] == '\0') {
           printf("Legacy mode: %s, manual: %s\n",
               LegacyMode_IsRunning() ? "running" : "stopped",
               LegacyMode_GetStartupManual() ? "true" : "false");
    }
    else if (strcasecmp(command, "cfg") == 0) {
        LegacyMode_RefreshStartupConfigFromSd();
        LegacyMode_PrintStartupConfigStub();
        if ((strcmp(LegacyMode_GetStartupConfigName(), "none") != 0) && !LegacyMode_IsStartupConfigLoaded()) {
            printf("Warning: %s found on SD but could not be loaded/parsed. Check SD mount and file readability.\n",
                   LegacyMode_GetStartupConfigName());
        }
    }
    else if (strcasecmp(command, "sd_eject") == 0) {
        UINT status = AppFileX_CloseMediaIfIdleOnSd();
        if (status == FX_SUCCESS) {
            printf("SD media closed. Card can be removed safely.\n");
        } else {
            printf("SD eject failed (FileX status=%u).\n", (unsigned int)status);
        }
    }
    else if (strcasecmp(command, "send") == 0) {
        uint8_t packet_id;
        if (strcasecmp(arg1, "reset") == 0) {
            packet_id = LEGACY_PACKET_RESET;
        } else if (strcasecmp(arg1, "idle") == 0) {
            packet_id = LEGACY_PACKET_IDLE;
        } else if (strcasecmp(arg1, "hard") == 0) {
            packet_id = LEGACY_PACKET_HARD;
        } else if (strcasecmp(arg1, "base") == 0) {
            packet_id = LEGACY_PACKET_BASE;
        } else {
            printf("Unknown legacy packet '%s'. Use reset|idle|hard|base\n", arg1);
            return;
        }

        if (LegacyMode_SelectPacket(packet_id)) {
            printf("Legacy packet selected: %s\n", LegacyMode_GetSelectedPacketName());
        } else {
            printf("Failed to select legacy packet\n");
        }
    }
    else if (strcasecmp(command, "profile") == 0) {
        if (strcasecmp(arg1, "sender_v3") == 0) {
            (void)LegacyMode_Stop();

            if (!LegacyMode_SelectPacket(LEGACY_PACKET_IDLE)) {
                printf("Failed to apply sender_v3 profile (packet)\n");
                return;
            }

                 printf("Applied profile sender_v3: mode %s, packet %s, state stopped\n",
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName());
        } else if (strcasecmp(arg1, "sender_v3_test") == 0) {
            (void)LegacyMode_Stop();

            if (!LegacyMode_ApplyCompatKey('w')) {
                printf("Failed to apply sender_v3_test profile (warble start)\n");
                return;
            }

                 printf("Applied profile sender_v3_test: mode %s, packet %s, state %s\n",
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName(),
                   LegacyMode_IsRunning() ? "running" : "stopped");
        } else {
            printf("Unknown profile '%s'. Available: sender_v3|sender_v3_test\n", arg1);
        }
    }
    else if (strcasecmp(command, "key") == 0) {
        const char key_cmd = arg1[0];
        if (key_cmd == '\0') {
            printf("Usage: key <r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|E|f|k|q|u|s|t|z|g|h>\n");
            return;
        }

        if (key_cmd == 'h') {
            printf("Legacy key map:\n");
            printf("  r=reset, R=hard reset, i=idle, d=base(dcc), D=stretched dcc, w=warble\n");
            printf("  a=scopeA, b=scopeB, o=scope timing, 0=zeros, 1=ones\n");
            printf("  c/C=clock, S=stretched zeros, e/E=estop-like, f=fw toggle, k=kickstart-like\n");
            printf("  u=clear underflow(no-op), s=speed/output step, t=self test(run), z=decoder test(run)\n");
            printf("  g=generic io(run), q=stop\n");
            return;
        }

        if (LegacyMode_ApplyCompatKey(key_cmd)) {
            printf("Legacy key '%c' applied. State: %s, mode: %s, packet: %s\n",
                   key_cmd,
                   LegacyMode_IsRunning() ? "running" : "stopped",
                   LegacyMode_GetModeName(),
                   LegacyMode_GetSelectedPacketName());
        } else {
            printf("Unsupported legacy key '%c'. Use r|R|i|d|D|a|b|o|w|0|1|c|C|S|e|E|f|k|q|u|s|t|z|g|h\n", key_cmd);
        }
    }
    else {
        printf("Unknown command: %s\n", command);
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

                if (handle_start_questionnaire_line(input_buffer)) {
                    memset(input_buffer, 0, sizeof(input_buffer));
                    memset(&parsed, 0, sizeof(parsed));
                    input_index = 0;
                    continue;
                }

                parse_input(input_buffer, &parsed);

                if (strcmp(parsed.command, "help") == 0) {
                    print_legacy_help();
                } else if (strcmp(parsed.command, "reset") == 0) {
                    printf("Resetting system...\n");
                    osDelay(100);
                    HAL_NVIC_SystemReset();
                } else if (parsed.command[0] != '\0') {
                    execute_legacy_command(parsed.command, parsed.arg1, parsed.arg2);
                }

                memset(input_buffer, 0, sizeof(input_buffer));
                memset(&parsed, 0, sizeof(parsed));
                input_index = 0;
            }
        }
    }
}

void CliLegacyApp_Start(void) {
    // Entering legacy mode should immediately stop command station activity.
    (void)CommandStation_Stop();

    if (legacy_command_queue == NULL) {
        legacy_command_queue = osMessageQueueNew(5, sizeof(uint32_t), NULL);
    }

    CliApp_SetActiveCommandQueue(legacy_command_queue);

    if (legacy_cli_task_handle == NULL) {
        legacy_cli_task_handle = osThreadNew(vLegacyCommandConsoleTask, NULL, &legacy_cli_task_attributes);
    }
}
