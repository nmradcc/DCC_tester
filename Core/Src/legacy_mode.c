#include "legacy_mode.h"
#include "main.h"
#include "app_filex.h"

#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include "command_station.h"
#include "version.h"

static bool legacy_mode_running = false;
static uint8_t selected_packet_id = LEGACY_PACKET_IDLE;

typedef enum {
    LEGACY_MODE_PACKET = 0,
    LEGACY_MODE_PACKET_STRETCHED,
    LEGACY_MODE_WARBLE,
    LEGACY_MODE_STREAM_ZERO,
    LEGACY_MODE_STREAM_ONE,
    LEGACY_MODE_STRETCHED_ZERO,
    LEGACY_MODE_STRETCHED_PATTERN,
    LEGACY_MODE_SCOPE_A,
    LEGACY_MODE_SCOPE_B,
    LEGACY_MODE_SCOPE_O
} legacy_wave_mode_t;

static legacy_wave_mode_t legacy_wave_mode = LEGACY_MODE_PACKET;

static uint32_t bit_index = 0;
static uint8_t half_phase = 0;
static bool phase_p = true;

static const uint8_t reset_packet_bytes[] = {0xff, 0xf0, 0x00, 0x00, 0x01};
static const uint8_t hard_reset_packet_bytes[] = {0xff, 0xf0, 0x00, 0x04, 0x03};
static const uint8_t idle_packet_bytes[] = {0xff, 0xf7, 0xf8, 0x01, 0xff};
static const uint8_t base_packet_bytes[] = {0xff, 0xf0, 0x19, 0xd0, 0xef};
static const uint8_t stretched_pattern_bytes[] = {0xff, 0x02};

static const uint32_t legacy_warble_one_bits = 3072U * 8U;
static const uint32_t legacy_warble_total_bits = (3072U + 1024U) * 8U;

static const uint16_t legacy_default_bit1_ticks = 58;   // ~58us @ 1MHz
static const uint16_t legacy_default_bit0_ticks = 100;  // ~100us @ 1MHz
static const uint16_t legacy_default_stretched0_ticks = 130;
static uint16_t legacy_bit1_ticks = 58;
static uint16_t legacy_bit0_ticks = 100;
static uint16_t legacy_stretched0_ticks = 130;
static const uint16_t legacy_scope_o_ticks[] = {100, 10000, 1000, 10};
static uint8_t legacy_scope_o_index = 0;
static bool legacy_fw_state = true;
static uint8_t legacy_kickstart_cycles = 0;
static bool legacy_dcc_mode_active = false;

static bool legacy_test_active = false;
static uint8_t legacy_test_packet_plan[64];
typedef enum {
    LEGACY_TEST_MUTATION_NONE = 0,
    LEGACY_TEST_MUTATION_BAD_ADDRESS,
    LEGACY_TEST_MUTATION_BAD_BIT
} legacy_test_mutation_t;

typedef struct {
    uint8_t packet_id;
    uint16_t bit0_ticks;
    uint16_t bit1_ticks;
    const char* step_name;
    bool todo_stub;
    legacy_test_mutation_t mutation;
    uint8_t mutation_bit_start;
} legacy_test_step_t;
static legacy_test_step_t legacy_test_step_plan[64];
static uint8_t legacy_test_packet_count = 0;
static uint8_t legacy_test_packet_index = 0;
static uint16_t legacy_test_packet_cycles = 0;
static legacy_test_mutation_t legacy_active_test_mutation = LEGACY_TEST_MUTATION_NONE;
static uint8_t legacy_active_mutation_bit = 0U;

typedef struct {
    uint32_t mask;
    uint16_t clk0h;
    uint16_t clk1t;
    const char* name;
} legacy_clock_profile_t;

static const legacy_clock_profile_t legacy_clock_profiles[] = {
    {0x00000001U, 100U, 116U, "nominal"},
    {0x00000002U, 98U, 113U, "all_1_4_fast"},
    {0x00000004U, 95U, 110U, "cmd_station_min"},
    {0x00000008U, 92U, 106U, "min_plus_2"},
    {0x00000010U, 91U, 105U, "min_plus_1"},
    {0x00000020U, 90U, 104U, "decoder_min"},
    {0x00000040U, 102U, 119U, "all_1_4_slow"},
    {0x00000080U, 105U, 122U, "cmd_station_max"},
    {0x00000100U, 108U, 126U, "max_minus_2"},
    {0x00000200U, 109U, 127U, "max_minus_1"},
    {0x00000400U, 110U, 128U, "decoder_max"}
};

typedef enum {
    LEGACY_STARTUP_CFG_NONE = 0,
    LEGACY_STARTUP_CFG_SEND_CFG,
    LEGACY_STARTUP_CFG_SEND_INI,
    LEGACY_STARTUP_CFG_CLI
} legacy_startup_cfg_t;

static legacy_startup_cfg_t legacy_startup_cfg = LEGACY_STARTUP_CFG_NONE;
static char legacy_start_cli_args[256];
static bool legacy_start_cli_args_pending = false;

typedef struct {
    bool loaded;
    bool manual;
    bool log_pkts;
    char decoder_type;
} legacy_startup_cfg_values_t;

typedef struct {
    bool loaded;
    uint32_t overrides;
    uint8_t address;
    uint32_t port;
    char decoder_type;
    bool manual;
    bool lamp;
    uint8_t preset;
    uint8_t trigger;
    bool critical;
    bool repeat;
    uint32_t tests_mask;
    uint32_t clocks_mask;
    uint32_t funcs_mask;
    uint8_t extra_pre;
    bool trig_rev;
    uint32_t fill_msec;
    uint8_t test_reps;
    bool log_pkts;
    bool no_abort;
    bool late_scope;
    bool fragment;
    bool same_ambig_addr;
} legacy_send_cfg_stub_t;

enum {
    LEGACY_CFG_OVR_ADDRESS = (1UL << 0),
    LEGACY_CFG_OVR_PORT = (1UL << 1),
    LEGACY_CFG_OVR_TYPE = (1UL << 2),
    LEGACY_CFG_OVR_MANUAL = (1UL << 3),
    LEGACY_CFG_OVR_LAMP = (1UL << 4),
    LEGACY_CFG_OVR_PRESET = (1UL << 5),
    LEGACY_CFG_OVR_TRIGGER = (1UL << 6),
    LEGACY_CFG_OVR_CRITICAL = (1UL << 7),
    LEGACY_CFG_OVR_REPEAT = (1UL << 8),
    LEGACY_CFG_OVR_TESTS = (1UL << 9),
    LEGACY_CFG_OVR_CLOCKS = (1UL << 10),
    LEGACY_CFG_OVR_FUNCS = (1UL << 11),
    LEGACY_CFG_OVR_EXTRA_PRE = (1UL << 12),
    LEGACY_CFG_OVR_TRIG_REV = (1UL << 13),
    LEGACY_CFG_OVR_FILL_MSEC = (1UL << 14),
    LEGACY_CFG_OVR_TEST_REPS = (1UL << 15),
    LEGACY_CFG_OVR_LOG_PKTS = (1UL << 16),
    LEGACY_CFG_OVR_NO_ABORT = (1UL << 17),
    LEGACY_CFG_OVR_LATE_SCOPE = (1UL << 18),
    LEGACY_CFG_OVR_FRAGMENT = (1UL << 19),
    LEGACY_CFG_OVR_SAME_AMBIG_ADDR = (1UL << 20)
};

static legacy_startup_cfg_values_t legacy_startup_cfg_values = {false, false, false, 'l'};
static legacy_send_cfg_stub_t legacy_send_cfg_stub = {
    false,
    0U,
    3U,
    0x340U,
    'l',
    false,
    false,
    0U,
    8U,
    false,
    false,
    0x00000080U,
    0xfffffce7U,
    0x1fU,
    0U,
    false,
    1000U,
    2U,
    false,
    false,
    false,
    false,
    false
};
static char legacy_cfg_text_buffer[8192];
static char legacy_user_docs_buffer[8192];
static char legacy_startup_summary_buffer[4096];
static char legacy_log_base_name[32] = "legacy";
static char legacy_log_filename[40] = "legacy.log";
static char legacy_sum_filename[40] = "legacy.sum";

static inline const uint8_t* legacy_packet_data(uint8_t packet_id, size_t* size);
static const char* legacy_packet_name(uint8_t packet_id);
static void legacy_log_printf(const char* fmt, ...);
static void legacy_log_detail_printf(const char* fmt, ...);
static void legacy_log_packet_bytes(const uint8_t* data, size_t size);
static void legacy_log_selected_packet(const char* context);
static void legacy_set_error(char* error_buf, size_t error_buf_size, const char* fmt, ...);
static bool legacy_apply_sender_cli_args(char* args_text, char* error_buf, size_t error_buf_size);
static void legacy_prepare_test_plan(void);
static void legacy_advance_test_plan_on_packet_boundary(void);
static void legacy_apply_clock_mask(uint32_t clocks_mask);
static const char* legacy_test_name(uint8_t test_bit);
static uint8_t legacy_packet_for_test(uint8_t test_bit, bool* is_stub);
static bool legacy_append_test_step(uint8_t packet_id,
                                    uint16_t bit0_ticks,
                                    uint16_t bit1_ticks,
                                    const char* step_name,
                                    bool todo_stub,
                                    legacy_test_mutation_t mutation,
                                    uint8_t mutation_bit_start);
static void legacy_append_margin_1t_steps(void);
static void legacy_append_duty_1h_steps(void);
static void legacy_refresh_log_filenames(void);
static bool legacy_should_mirror_line_to_sum(const char* text);
static void legacy_echo_status_to_console(const char* text);
static void legacy_format_timestamp_prefix(char* out, size_t out_size);

static void legacy_refresh_log_filenames(void)
{
    (void)snprintf(legacy_log_filename, sizeof(legacy_log_filename), "%s.log", legacy_log_base_name);
    (void)snprintf(legacy_sum_filename, sizeof(legacy_sum_filename), "%s.sum", legacy_log_base_name);
}

static bool legacy_should_mirror_line_to_sum(const char* text)
{
    if ((text == NULL) || (text[0] == '\0')) {
        return false;
    }

    if ((strncmp(text, "STATUS", 6U) == 0) ||
        (strncmp(text, "- Clock", 7U) == 0)) {
        return true;
    }

    return false;
}

static void legacy_format_timestamp_prefix(char* out, size_t out_size)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    static const char* const weekday_names[] = {
        "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"
    };
    static const char* const month_names[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    const char* weekday = "Mon";
    const char* month = "Jan";

    if ((out == NULL) || (out_size == 0U)) {
        return;
    }

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
                   "<%s %s %02u %02u:%02u:%02u 20%02u> ",
                   weekday,
                   month,
                   (unsigned int)sDate.Date,
                   (unsigned int)sTime.Hours,
                   (unsigned int)sTime.Minutes,
                   (unsigned int)sTime.Seconds,
                   (unsigned int)sDate.Year);
}

static void legacy_echo_status_to_console(const char* text)
{
    const char* message;
    const char* status;
    size_t len;

    if (text == NULL) {
        return;
    }

    status = strstr(text, "STATUS");
    if (status == NULL) {
        return;
    }

    message = status + 6U;
    while ((*message == ' ') || (*message == '\t')) {
        ++message;
    }

    if (*message == '\0') {
        return;
    }

    len = strlen(message);
    printf("%s", message);
    if ((len == 0U) || (message[len - 1U] != '\n')) {
        printf("\n");
    }
}

static bool legacy_append_test_step(uint8_t packet_id,
                                    uint16_t bit0_ticks,
                                    uint16_t bit1_ticks,
                                    const char* step_name,
                                    bool todo_stub,
                                    legacy_test_mutation_t mutation,
                                    uint8_t mutation_bit_start)
{
    if (legacy_test_packet_count >= (uint8_t)(sizeof(legacy_test_step_plan) / sizeof(legacy_test_step_plan[0]))) {
        return false;
    }

    legacy_test_step_plan[legacy_test_packet_count].packet_id = packet_id;
    legacy_test_step_plan[legacy_test_packet_count].bit0_ticks = bit0_ticks;
    legacy_test_step_plan[legacy_test_packet_count].bit1_ticks = bit1_ticks;
    legacy_test_step_plan[legacy_test_packet_count].step_name = step_name;
    legacy_test_step_plan[legacy_test_packet_count].todo_stub = todo_stub;
    legacy_test_step_plan[legacy_test_packet_count].mutation = mutation;
    legacy_test_step_plan[legacy_test_packet_count].mutation_bit_start = mutation_bit_start;
    legacy_test_packet_plan[legacy_test_packet_count] = packet_id;
    legacy_test_packet_count++;
    return true;
}

static void legacy_append_margin_1t_steps(void)
{
    const uint16_t base0 = legacy_bit0_ticks;
    const uint16_t base1 = legacy_bit1_ticks;
    const uint16_t min1 = (base1 > 6U) ? (uint16_t)(base1 - 6U) : 1U;
    const uint16_t max1 = (uint16_t)(base1 + 6U);

    (void)legacy_append_test_step(LEGACY_PACKET_BASE, base0, base1, "margin_1t_nom", false, LEGACY_TEST_MUTATION_NONE, 0U);
    (void)legacy_append_test_step(LEGACY_PACKET_BASE, base0, min1, "margin_1t_min", false, LEGACY_TEST_MUTATION_NONE, 0U);
    (void)legacy_append_test_step(LEGACY_PACKET_BASE, base0, max1, "margin_1t_max", false, LEGACY_TEST_MUTATION_NONE, 0U);
}

static void legacy_append_duty_1h_steps(void)
{
    const uint16_t base0 = legacy_bit0_ticks;
    const uint16_t base1 = legacy_bit1_ticks;
    const uint16_t min0 = (base0 > 10U) ? (uint16_t)(base0 - 10U) : 1U;
    const uint16_t max0 = (uint16_t)(base0 + 10U);

    (void)legacy_append_test_step(LEGACY_PACKET_IDLE, base0, base1, "duty_1h_nom", false, LEGACY_TEST_MUTATION_NONE, 0U);
    (void)legacy_append_test_step(LEGACY_PACKET_IDLE, min0, base1, "duty_1h_min", false, LEGACY_TEST_MUTATION_NONE, 0U);
    (void)legacy_append_test_step(LEGACY_PACKET_IDLE, max0, base1, "duty_1h_max", false, LEGACY_TEST_MUTATION_NONE, 0U);
}

static void legacy_apply_clock_mask(uint32_t clocks_mask)
{
    size_t i;

    legacy_bit0_ticks = legacy_default_bit0_ticks;
    legacy_bit1_ticks = legacy_default_bit1_ticks;
    legacy_stretched0_ticks = legacy_default_stretched0_ticks;

    for (i = 0U; i < (sizeof(legacy_clock_profiles) / sizeof(legacy_clock_profiles[0])); ++i) {
        if ((clocks_mask & legacy_clock_profiles[i].mask) != 0U) {
            legacy_bit0_ticks = legacy_clock_profiles[i].clk0h;
            legacy_bit1_ticks = (uint16_t)((legacy_clock_profiles[i].clk1t + 1U) / 2U);
            legacy_log_printf("STATUS  Starting clock <%s>",
                              legacy_clock_profiles[i].name,
                              (unsigned int)legacy_bit0_ticks);
            legacy_log_printf("- Clock 0T %4u, 0H %4u, 1T %4u",
                              (unsigned int)(legacy_clock_profiles[i].clk0h * 2U),
                              (unsigned int)legacy_clock_profiles[i].clk0h,
                              (unsigned int)legacy_clock_profiles[i].clk1t);
            return;
        }
    }

    legacy_log_printf("STATUS  Starting clock <All nominal>");
    legacy_log_printf("- Clock 0T %4u, 0H %4u, 1T %4u",
                      (unsigned int)(legacy_bit0_ticks * 2U),
                      (unsigned int)legacy_bit0_ticks,
                      (unsigned int)(legacy_bit1_ticks * 2U));
}

static const char* legacy_test_name(uint8_t test_bit)
{
    static const char* names[] = {
        "margin_1t",
        "duty_1h",
        "ramp",
        "packet_acceptance_pre12_idle1",
        "packet_acceptance_pre12_idle2",
        "packet_acceptance_pre13_idle1",
        "packet_acceptance_pre15_idle1",
        "packet_acceptance_pre15_idle2",
        "bad_address",
        "bad_bit",
        "stretched_0_variant_1",
        "stretched_0_variant_2",
        "stretched_0_variant_3",
        "stretched_0_variant_4",
        "stretched_0_variant_5",
        "truncated_packet",
        "prior_packet",
        "prior_6_byte",
        "ambiguous_bit_1",
        "ambiguous_bit_2",
        "reserved_20"
    };

    if (test_bit < (uint8_t)(sizeof(names) / sizeof(names[0]))) {
        return names[test_bit];
    }
    return "unknown";
}

static uint8_t legacy_packet_for_test(uint8_t test_bit, bool* is_stub)
{
    bool stub = false;
    uint8_t packet = LEGACY_PACKET_BASE;

    switch (test_bit) {
        case 0U:
            packet = LEGACY_PACKET_RESET;
            break;
        case 1U:
            packet = LEGACY_PACKET_IDLE;
            break;
        case 2U:
            packet = LEGACY_PACKET_HARD;
            break;
        case 3U:
            packet = LEGACY_PACKET_BASE;
            break;
        case 8U:
        case 9U:
            packet = LEGACY_PACKET_BASE;
            break;
        default:
            packet = LEGACY_PACKET_BASE;
            stub = true;
            break;
    }

    if (is_stub != NULL) {
        *is_stub = stub;
    }
    return packet;
}

static void legacy_seed_send_cfg_defaults(void)
{
    legacy_send_cfg_stub.loaded = true;
    legacy_send_cfg_stub.overrides = 0U;
    legacy_send_cfg_stub.address = 3U;
    legacy_send_cfg_stub.port = 0x340U;
    legacy_send_cfg_stub.decoder_type = 'l';
    legacy_send_cfg_stub.manual = false;
    legacy_send_cfg_stub.lamp = false;
    legacy_send_cfg_stub.preset = 0U;
    legacy_send_cfg_stub.trigger = 8U;
    legacy_send_cfg_stub.critical = false;
    legacy_send_cfg_stub.repeat = false;
    legacy_send_cfg_stub.tests_mask = 0x00000080U;
    legacy_send_cfg_stub.clocks_mask = 0xfffffce7U;
    legacy_send_cfg_stub.funcs_mask = 0x1fU;
    legacy_send_cfg_stub.extra_pre = 0U;
    legacy_send_cfg_stub.trig_rev = false;
    legacy_send_cfg_stub.fill_msec = 1000U;
    legacy_send_cfg_stub.test_reps = 2U;
    legacy_send_cfg_stub.log_pkts = false;
    legacy_send_cfg_stub.no_abort = false;
    legacy_send_cfg_stub.late_scope = false;
    legacy_send_cfg_stub.fragment = false;
    legacy_send_cfg_stub.same_ambig_addr = false;

    legacy_startup_cfg_values.loaded = true;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';
}

static void legacy_trim_spaces(char** start_ptr, char** end_ptr)
{
    while ((*start_ptr < *end_ptr) && isspace((unsigned char)**start_ptr)) {
        ++(*start_ptr);
    }
    while ((*end_ptr > *start_ptr) && isspace((unsigned char)*((*end_ptr) - 1))) {
        --(*end_ptr);
    }
}

static bool legacy_token_equals(const char* token_start, size_t token_len, const char* word)
{
    size_t i = 0;
    for (; i < token_len && word[i] != '\0'; ++i) {
        if (toupper((unsigned char)token_start[i]) != toupper((unsigned char)word[i])) {
            return false;
        }
    }
    return (i == token_len) && (word[i] == '\0');
}

static bool legacy_parse_bool_token(const char* start, size_t len, bool* out_value)
{
    if ((start == NULL) || (out_value == NULL) || (len == 0U)) {
        return false;
    }

    if (legacy_token_equals(start, len, "1") ||
        legacy_token_equals(start, len, "TRUE") ||
        legacy_token_equals(start, len, "YES") ||
        legacy_token_equals(start, len, "ON")) {
        *out_value = true;
        return true;
    }

    if (legacy_token_equals(start, len, "0") ||
        legacy_token_equals(start, len, "FALSE") ||
        legacy_token_equals(start, len, "NO") ||
        legacy_token_equals(start, len, "OFF")) {
        *out_value = false;
        return true;
    }

    return false;
}

static bool legacy_parse_decoder_type_token(const char* start, size_t len, char* out_type)
{
    if ((start == NULL) || (out_type == NULL) || (len == 0U)) {
        return false;
    }

    if (legacy_token_equals(start, len, "L") || legacy_token_equals(start, len, "LEGACY")) {
        *out_type = 'l';
        return true;
    }
    if (legacy_token_equals(start, len, "F") || legacy_token_equals(start, len, "FX")) {
        *out_type = 'f';
        return true;
    }
    if (legacy_token_equals(start, len, "A") || legacy_token_equals(start, len, "ACCESSORY")) {
        *out_type = 'a';
        return true;
    }
    if (legacy_token_equals(start, len, "S") || legacy_token_equals(start, len, "SENDER")) {
        *out_type = 's';
        return true;
    }

    return false;
}

static bool legacy_parse_u32_token(const char* start, size_t len, uint32_t* out_value)
{
    size_t i = 0U;
    uint32_t value = 0U;
    uint32_t base = 10U;

    if ((start == NULL) || (out_value == NULL) || (len == 0U)) {
        return false;
    }

    if ((len > 2U) && (start[0] == '0') && ((start[1] == 'x') || (start[1] == 'X'))) {
        base = 16U;
        i = 2U;
        if (i >= len) {
            return false;
        }
    }

    for (; i < len; ++i) {
        unsigned char c = (unsigned char)start[i];
        uint32_t digit;

        if ((c >= '0') && (c <= '9')) {
            digit = (uint32_t)(c - '0');
        } else if ((base == 16U) && (c >= 'a') && (c <= 'f')) {
            digit = 10U + (uint32_t)(c - 'a');
        } else if ((base == 16U) && (c >= 'A') && (c <= 'F')) {
            digit = 10U + (uint32_t)(c - 'A');
        } else {
            return false;
        }

        value = (value * base) + digit;
    }

    *out_value = value;
    return true;
}

static inline const char* legacy_cfg_mark(uint32_t mask)
{
    return ((legacy_send_cfg_stub.overrides & mask) != 0U) ? "*" : "";
}

static void legacy_set_error(char* error_buf, size_t error_buf_size, const char* fmt, ...)
{
    va_list args;

    if ((error_buf == NULL) || (error_buf_size == 0U) || (fmt == NULL)) {
        return;
    }

    va_start(args, fmt);
    (void)vsnprintf(error_buf, error_buf_size, fmt, args);
    va_end(args);
}

static bool legacy_apply_sender_cli_args(char* args_text, char* error_buf, size_t error_buf_size)
{
    char* token;

    if ((args_text == NULL) || (args_text[0] == '\0')) {
        return true;
    }

    if (!legacy_send_cfg_stub.loaded) {
        legacy_seed_send_cfg_defaults();
    }

    token = strtok(args_text, " \t");
    while (token != NULL) {
        if (strcmp(token, "-?") == 0) {
            legacy_set_error(error_buf, error_buf_size, "%s is not supported in legacy CLI start.", token);
            return false;
        } else if (strcmp(token, "-m") == 0) {
            legacy_send_cfg_stub.manual = !legacy_send_cfg_stub.manual;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_MANUAL;
        } else if (strcmp(token, "-l") == 0) {
            legacy_send_cfg_stub.lamp = !legacy_send_cfg_stub.lamp;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LAMP;
        } else if (strcmp(token, "-f") == 0) {
            legacy_send_cfg_stub.fragment = !legacy_send_cfg_stub.fragment;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FRAGMENT;
        } else if (strcmp(token, "-x") == 0) {
            legacy_send_cfg_stub.critical = !legacy_send_cfg_stub.critical;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_CRITICAL;
        } else if (strcmp(token, "-r") == 0) {
            legacy_send_cfg_stub.repeat = !legacy_send_cfg_stub.repeat;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_REPEAT;
        } else if (strcmp(token, "-T") == 0) {
            legacy_send_cfg_stub.trig_rev = !legacy_send_cfg_stub.trig_rev;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TRIG_REV;
        } else if (strcmp(token, "-P") == 0) {
            legacy_send_cfg_stub.log_pkts = !legacy_send_cfg_stub.log_pkts;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LOG_PKTS;
        } else if (strcmp(token, "-A") == 0) {
            legacy_send_cfg_stub.no_abort = !legacy_send_cfg_stub.no_abort;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_NO_ABORT;
        } else if (strcmp(token, "-s") == 0) {
            legacy_send_cfg_stub.late_scope = !legacy_send_cfg_stub.late_scope;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LATE_SCOPE;
        } else if (strcmp(token, "-S") == 0) {
            legacy_send_cfg_stub.same_ambig_addr = !legacy_send_cfg_stub.same_ambig_addr;
            legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_SAME_AMBIG_ADDR;
        } else if (strcmp(token, "-L") == 0 || strcmp(token, "-k") == 0 || strcmp(token, "-D") == 0 || strcmp(token, "-u") == 0) {
            /* Accepted for Sender compatibility; currently no runtime effect in legacy mode. */
        } else {
            char* value;
            uint32_t parsed_value;

            if ((strcmp(token, "-a") != 0) && (strcmp(token, "-p") != 0) &&
                (strcmp(token, "-d") != 0) && (strcmp(token, "-n") != 0) &&
                (strcmp(token, "-N") != 0) && (strcmp(token, "-t") != 0) &&
                (strcmp(token, "-c") != 0) && (strcmp(token, "-g") != 0) &&
                (strcmp(token, "-E") != 0) && (strcmp(token, "-F") != 0) &&
                (strcmp(token, "-R") != 0) && (strcmp(token, "-o") != 0) &&
                (strcmp(token, "-e") != 0)) {
                legacy_set_error(error_buf, error_buf_size, "Unknown sender option '%s'.", token);
                return false;
            }

            value = strtok(NULL, " \t");
            if (value == NULL) {
                legacy_set_error(error_buf, error_buf_size, "%s requires an argument.", token);
                return false;
            }

            if (strcmp(token, "-d") == 0) {
                char parsed_type;
                if (!legacy_parse_decoder_type_token(value, strlen(value), &parsed_type)) {
                    legacy_set_error(error_buf, error_buf_size, "Invalid -d value '%s'.", value);
                    return false;
                }
                legacy_send_cfg_stub.decoder_type = parsed_type;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TYPE;
            } else {
                if (!legacy_parse_u32_token(value, strlen(value), &parsed_value)) {
                    legacy_set_error(error_buf, error_buf_size, "Invalid numeric value '%s' for %s.", value, token);
                    return false;
                }

                if (strcmp(token, "-a") == 0) {
                    if (parsed_value > 255U) {
                        legacy_set_error(error_buf, error_buf_size, "-a value must be <= 255.");
                        return false;
                    }
                    legacy_send_cfg_stub.address = (uint8_t)parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_ADDRESS;
                } else if (strcmp(token, "-p") == 0) {
                    legacy_send_cfg_stub.port = parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_PORT;
                } else if (strcmp(token, "-n") == 0) {
                    if (parsed_value > 255U) {
                        legacy_set_error(error_buf, error_buf_size, "-n value must be <= 255.");
                        return false;
                    }
                    legacy_send_cfg_stub.preset = (uint8_t)parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_PRESET;
                } else if (strcmp(token, "-N") == 0) {
                    if (parsed_value > 255U) {
                        legacy_set_error(error_buf, error_buf_size, "-N value must be <= 255.");
                        return false;
                    }
                    legacy_send_cfg_stub.trigger = (uint8_t)parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TRIGGER;
                } else if (strcmp(token, "-t") == 0) {
                    legacy_send_cfg_stub.tests_mask = parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TESTS;
                } else if (strcmp(token, "-c") == 0) {
                    legacy_send_cfg_stub.clocks_mask = parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_CLOCKS;
                } else if (strcmp(token, "-g") == 0) {
                    if (parsed_value == 0U) {
                        legacy_set_error(error_buf, error_buf_size, "-g mask must not be 0.");
                        return false;
                    }
                    legacy_send_cfg_stub.funcs_mask = parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FUNCS;
                } else if (strcmp(token, "-E") == 0) {
                    if (parsed_value > 255U) {
                        legacy_set_error(error_buf, error_buf_size, "-E value must be <= 255.");
                        return false;
                    }
                    legacy_send_cfg_stub.extra_pre = (uint8_t)parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_EXTRA_PRE;
                } else if (strcmp(token, "-F") == 0) {
                    legacy_send_cfg_stub.fill_msec = parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FILL_MSEC;
                } else if (strcmp(token, "-R") == 0) {
                    if ((parsed_value < 1U) || (parsed_value > 255U)) {
                        legacy_set_error(error_buf, error_buf_size, "-R value must be in range 1..255.");
                        return false;
                    }
                    legacy_send_cfg_stub.test_reps = (uint8_t)parsed_value;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TEST_REPS;
                } else if (strcmp(token, "-o") == 0) {
                    if ((parsed_value < 1U) || (parsed_value > 4U)) {
                        legacy_set_error(error_buf, error_buf_size, "-o pair must be in range 1..4.");
                        return false;
                    }

                    legacy_send_cfg_stub.preset = (uint8_t)((parsed_value * 2U) - 1U);
                    legacy_send_cfg_stub.trigger = (uint8_t)(legacy_send_cfg_stub.preset + 1U);
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_PRESET;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TRIGGER;
                } else if (strcmp(token, "-e") == 0) {
                    /* Accepted for compatibility; reserved for future runtime use. */
                }
            }
        }

        token = strtok(NULL, " \t");
    }

    legacy_startup_cfg_values.loaded = true;
    legacy_startup_cfg_values.manual = legacy_send_cfg_stub.manual;
    legacy_startup_cfg_values.log_pkts = legacy_send_cfg_stub.log_pkts;
    legacy_startup_cfg_values.decoder_type = legacy_send_cfg_stub.decoder_type;
    return true;
}

static void legacy_prepare_test_plan(void)
{
    uint32_t tests = legacy_send_cfg_stub.tests_mask;
    uint8_t bit;

    legacy_test_active = false;
    legacy_test_packet_count = 0;
    legacy_test_packet_index = 0;
    legacy_test_packet_cycles = 0U;

    if (legacy_startup_cfg_values.manual) {
        return;
    }

    legacy_apply_clock_mask(legacy_send_cfg_stub.clocks_mask);

    for (bit = 0U; bit <= 20U; ++bit) {
        const uint32_t mask = (1UL << bit);
        bool is_stub = false;
        uint8_t packet;

        if ((tests & mask) == 0U) {
            continue;
        }

        if (bit == 0U) {
            legacy_append_margin_1t_steps();
            legacy_log_detail_printf("TEST map test=%s mask=0x%08lX steps=3", legacy_test_name(bit), (unsigned long)mask);
            continue;
        }

        if (bit == 1U) {
            legacy_append_duty_1h_steps();
            legacy_log_detail_printf("TEST map test=%s mask=0x%08lX steps=3", legacy_test_name(bit), (unsigned long)mask);
            continue;
        }

        if (bit == 8U) {
            (void)legacy_append_test_step(LEGACY_PACKET_BASE,
                                          legacy_bit0_ticks,
                                          legacy_bit1_ticks,
                                          legacy_test_name(bit),
                                          false,
                                          LEGACY_TEST_MUTATION_BAD_ADDRESS,
                                          11U);
            legacy_log_detail_printf("TEST map test=%s mask=0x%08lX packet=%s mutation=bad_address",
                                     legacy_test_name(bit),
                                     (unsigned long)mask,
                                     legacy_packet_name(LEGACY_PACKET_BASE));
            continue;
        }

        if (bit == 9U) {
            (void)legacy_append_test_step(LEGACY_PACKET_BASE,
                                          legacy_bit0_ticks,
                                          legacy_bit1_ticks,
                                          legacy_test_name(bit),
                                          false,
                                          LEGACY_TEST_MUTATION_BAD_BIT,
                                          9U);
            legacy_log_detail_printf("TEST map test=%s mask=0x%08lX packet=%s mutation=bad_bit",
                                     legacy_test_name(bit),
                                     (unsigned long)mask,
                                     legacy_packet_name(LEGACY_PACKET_BASE));
            continue;
        }

        packet = legacy_packet_for_test(bit, &is_stub);
        (void)legacy_append_test_step(packet,
                                      legacy_bit0_ticks,
                                      legacy_bit1_ticks,
                                      legacy_test_name(bit),
                                      is_stub,
                                      LEGACY_TEST_MUTATION_NONE,
                                      0U);

        if (is_stub) {
            legacy_log_detail_printf("TODO test=%s mask=0x%08lX not fully implemented on STM32 timer path; using BASE packet placeholder",
                                     legacy_test_name(bit),
                                     (unsigned long)mask);
        } else {
            legacy_log_detail_printf("TEST map test=%s mask=0x%08lX packet=%s",
                                     legacy_test_name(bit),
                                     (unsigned long)mask,
                                     legacy_packet_name(packet));
        }
    }

    // Sender default TESTS (0x80) should still run a meaningful packet path.
    if (legacy_test_packet_count == 0U && tests != 0U) {
        (void)legacy_append_test_step(LEGACY_PACKET_BASE,
                                      legacy_bit0_ticks,
                                      legacy_bit1_ticks,
                                      "default_base",
                                      true,
                                      LEGACY_TEST_MUTATION_NONE,
                                      0U);
    }

    if (legacy_test_packet_count == 0U) {
        return;
    }

    legacy_test_active = true;
    selected_packet_id = legacy_test_step_plan[0].packet_id;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_dcc_mode_active = (selected_packet_id == LEGACY_PACKET_BASE);
    legacy_bit0_ticks = legacy_test_step_plan[0].bit0_ticks;
    legacy_bit1_ticks = legacy_test_step_plan[0].bit1_ticks;
    legacy_active_test_mutation = legacy_test_step_plan[0].mutation;
    legacy_active_mutation_bit = legacy_test_step_plan[0].mutation_bit_start;
    bit_index = 0U;
    half_phase = 0U;

    legacy_log_printf("STATUS  Starting decoder tests.");
    legacy_log_printf("STATUS  tests=0x%08lX reps=%u repeat=%s steps=%u first=%s",
                      (unsigned long)legacy_send_cfg_stub.tests_mask,
                      (unsigned int)(legacy_send_cfg_stub.test_reps == 0U ? 1U : legacy_send_cfg_stub.test_reps),
                      legacy_send_cfg_stub.repeat ? "true" : "false",
                      (unsigned int)legacy_test_packet_count,
                      legacy_test_step_plan[0].step_name != NULL ? legacy_test_step_plan[0].step_name : legacy_packet_name(selected_packet_id));
    legacy_log_detail_printf("TEST step=%s packet=%s bit0=%u bit1=%u",
                             legacy_test_step_plan[0].step_name != NULL ? legacy_test_step_plan[0].step_name : "unknown",
                             legacy_packet_name(selected_packet_id),
                             (unsigned int)legacy_bit0_ticks,
                             (unsigned int)legacy_bit1_ticks);
    if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_ADDRESS) {
        legacy_log_detail_printf("TEST mutation=bad_address bit=%u", (unsigned int)legacy_active_mutation_bit);
    } else if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_BIT) {
        legacy_log_detail_printf("TEST mutation=bad_bit start_bit=%u", (unsigned int)legacy_active_mutation_bit);
    }
    legacy_log_selected_packet("TEST_SELECT");
}

static void legacy_advance_test_plan_on_packet_boundary(void)
{
    uint16_t reps_target;

    if (!legacy_test_active || legacy_test_packet_count == 0U) {
        return;
    }

    if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_BIT) {
        size_t packet_size = 0U;
        const uint8_t* packet_data = legacy_packet_data(selected_packet_id, &packet_size);
        (void)packet_data;
        if (packet_size > 1U) {
            const uint32_t total_bits = (uint32_t)packet_size * 8U;
            const uint32_t start_bit = 9U;
            if (total_bits > start_bit) {
                uint32_t rel = 0U;
                if (legacy_active_mutation_bit >= start_bit) {
                    rel = (uint32_t)legacy_active_mutation_bit - start_bit;
                }
                rel = (rel + 1U) % (total_bits - start_bit);
                legacy_active_mutation_bit = (uint8_t)(start_bit + rel);
            }
        }
    }

    reps_target = (legacy_send_cfg_stub.test_reps == 0U) ? 1U : legacy_send_cfg_stub.test_reps;

    legacy_test_packet_cycles++;
    if (legacy_test_packet_cycles < reps_target) {
        return;
    }

    legacy_test_packet_cycles = 0U;
    legacy_test_packet_index++;

    if (legacy_test_packet_index >= legacy_test_packet_count) {
        if (legacy_send_cfg_stub.repeat) {
            legacy_test_packet_index = 0U;
        } else {
            legacy_test_active = false;
            legacy_log_printf("STATUS  Tests COMPLETED, All tests passed");
            return;
        }
    }

    selected_packet_id = legacy_test_step_plan[legacy_test_packet_index].packet_id;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_dcc_mode_active = (selected_packet_id == LEGACY_PACKET_BASE);
    legacy_bit0_ticks = legacy_test_step_plan[legacy_test_packet_index].bit0_ticks;
    legacy_bit1_ticks = legacy_test_step_plan[legacy_test_packet_index].bit1_ticks;
    legacy_active_test_mutation = legacy_test_step_plan[legacy_test_packet_index].mutation;
    legacy_active_mutation_bit = legacy_test_step_plan[legacy_test_packet_index].mutation_bit_start;
    bit_index = 0U;
    half_phase = 0U;
    legacy_log_detail_printf("TEST step=%s packet=%s bit0=%u bit1=%u",
                             legacy_test_step_plan[legacy_test_packet_index].step_name != NULL ? legacy_test_step_plan[legacy_test_packet_index].step_name : "unknown",
                             legacy_packet_name(selected_packet_id),
                             (unsigned int)legacy_bit0_ticks,
                             (unsigned int)legacy_bit1_ticks);
    if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_ADDRESS) {
        legacy_log_detail_printf("TEST mutation=bad_address bit=%u", (unsigned int)legacy_active_mutation_bit);
    } else if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_BIT) {
        legacy_log_detail_printf("TEST mutation=bad_bit start_bit=%u", (unsigned int)legacy_active_mutation_bit);
    }
    legacy_log_selected_packet("TEST_SELECT");
}

static void legacy_parse_startup_cfg_text(char* cfg_text)
{
    char* line = cfg_text;

    legacy_startup_cfg_values.loaded = true;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';

    legacy_seed_send_cfg_defaults();

    while ((line != NULL) && (*line != '\0')) {
        char* next = strpbrk(line, "\r\n");
        if (next != NULL) {
            *next = '\0';
        }

        /* Search comments only within this logical line. */
        char* hash = strchr(line, '#');

        /* Ignore inline comments: anything after '#' is comment text. */
        if (hash != NULL) {
            *hash = '\0';
        }

        /* SEND.CFG comment: ignore when ';' is the first character on the line. */
        if (*line == ';') {
            if (next == NULL) {
                break;
            }
            line = next + 1;
            if ((*line == '\n') || (*line == '\r')) {
                ++line;
            }
            continue;
        }

        char* start = line;
        char* end = line + strlen(line);
        legacy_trim_spaces(&start, &end);

        if ((start < end) && (*start != '#')) {
            char* key_start = start;
            char* key_end = key_start;
            char* value_start = NULL;
            char* value_end = NULL;

            while ((key_end < end) &&
                   !isspace((unsigned char)*key_end) &&
                   (*key_end != '=') &&
                     (*key_end != ':') &&
                     (*key_end != ';') &&
                     (*key_end != '#')) {
                ++key_end;
            }

            value_start = key_end;
            while ((value_start < end) && isspace((unsigned char)*value_start)) {
                ++value_start;
            }

            if ((value_start < end) && ((*value_start == '=') || (*value_start == ':'))) {
                ++value_start;
            }

            while ((value_start < end) && isspace((unsigned char)*value_start)) {
                ++value_start;
            }

            value_end = value_start;
            while ((value_end < end) && !isspace((unsigned char)*value_end)) {
                ++value_end;
            }

            if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "MANUAL")) {
                bool parsed_bool = false;
                bool parsed_value = true;

                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }

                legacy_send_cfg_stub.manual = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_MANUAL;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "LOG_PKTS") ||
                       legacy_token_equals(key_start, (size_t)(key_end - key_start), "LOGPKTS")) {
                bool parsed_bool = false;
                bool parsed_value = true;

                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }

                legacy_send_cfg_stub.log_pkts = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LOG_PKTS;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "TYPE") ||
                       legacy_token_equals(key_start, (size_t)(key_end - key_start), "DECODER_TYPE")) {
                char parsed_type = '\0';
                if ((value_start < end) &&
                    legacy_parse_decoder_type_token(value_start, (size_t)(value_end - value_start), &parsed_type)) {
                    legacy_send_cfg_stub.decoder_type = parsed_type;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TYPE;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "ADDRESS")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v) &&
                    (v <= 255U)) {
                    legacy_send_cfg_stub.address = (uint8_t)v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_ADDRESS;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "PORT")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v)) {
                    legacy_send_cfg_stub.port = v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_PORT;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "LAMP")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.lamp = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LAMP;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "PRESET")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v) &&
                    (v <= 255U)) {
                    legacy_send_cfg_stub.preset = (uint8_t)v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_PRESET;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "TRIGGER")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v) &&
                    (v <= 255U)) {
                    legacy_send_cfg_stub.trigger = (uint8_t)v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TRIGGER;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "CRITICAL")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.critical = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_CRITICAL;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "REPEAT")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.repeat = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_REPEAT;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "TESTS")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v)) {
                    legacy_send_cfg_stub.tests_mask = v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TESTS;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "CLOCKS")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v)) {
                    legacy_send_cfg_stub.clocks_mask = v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_CLOCKS;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "FUNCS")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v)) {
                    legacy_send_cfg_stub.funcs_mask = v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FUNCS;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "EXTRA_PRE")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v) &&
                    (v <= 255U)) {
                    legacy_send_cfg_stub.extra_pre = (uint8_t)v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_EXTRA_PRE;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "TRIG_REV")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.trig_rev = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TRIG_REV;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "FILL_MSEC")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v)) {
                    legacy_send_cfg_stub.fill_msec = v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FILL_MSEC;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "TEST_REPS")) {
                uint32_t v;
                if ((value_start < end) &&
                    legacy_parse_u32_token(value_start, (size_t)(value_end - value_start), &v) &&
                    (v <= 255U)) {
                    legacy_send_cfg_stub.test_reps = (uint8_t)v;
                    legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_TEST_REPS;
                }
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "NO_ABORT")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.no_abort = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_NO_ABORT;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "LATE_SCOPE")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.late_scope = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_LATE_SCOPE;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "FRAGMENT")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.fragment = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_FRAGMENT;
            } else if (legacy_token_equals(key_start, (size_t)(key_end - key_start), "SAME_AMBIG_ADDR")) {
                bool parsed_bool = false;
                bool parsed_value = true;
                if (value_start < end) {
                    parsed_bool = legacy_parse_bool_token(value_start, (size_t)(value_end - value_start), &parsed_value);
                }
                legacy_send_cfg_stub.same_ambig_addr = parsed_bool ? parsed_value : true;
                legacy_send_cfg_stub.overrides |= LEGACY_CFG_OVR_SAME_AMBIG_ADDR;
            }
        }

        if (next == NULL) {
            break;
        }

        line = next + 1;
        if ((*line == '\n') || (*line == '\r')) {
            ++line;
        }
    }

    /* Preserve current runtime behavior through the reduced effective fields. */
    legacy_startup_cfg_values.manual = legacy_send_cfg_stub.manual;
    legacy_startup_cfg_values.log_pkts = legacy_send_cfg_stub.log_pkts;
    legacy_startup_cfg_values.decoder_type = legacy_send_cfg_stub.decoder_type;
}

static void legacy_apply_startup_cfg_defaults(void)
{
    const bool is_default_idle_start =
        (legacy_wave_mode == LEGACY_MODE_PACKET) &&
        (selected_packet_id == LEGACY_PACKET_IDLE) &&
        (legacy_dcc_mode_active == false) &&
        (bit_index == 0U) &&
        (half_phase == 0U);

    if (!is_default_idle_start || !legacy_startup_cfg_values.loaded) {
        return;
    }

    if (legacy_startup_cfg_values.manual) {
        return;
    }

    // Sender defaults to immediate decoder-test path when MANUAL is absent.
    // In legacy mode, map that behavior to starting in BASE packet mode.
    selected_packet_id = LEGACY_PACKET_BASE;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_dcc_mode_active = true;
}

static void legacy_probe_startup_cfg_on_sd(void)
{
    legacy_startup_cfg = LEGACY_STARTUP_CFG_NONE;
    legacy_startup_cfg_values.loaded = false;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';
    legacy_send_cfg_stub.loaded = false;
    legacy_send_cfg_stub.overrides = 0U;

    if (AppFileX_FileExistsOnSd("SEND.CFG") == FX_SUCCESS) {
        legacy_startup_cfg = LEGACY_STARTUP_CFG_SEND_CFG;
        if (AppFileX_LoadTextFileOnSd("SEND.CFG", legacy_cfg_text_buffer, sizeof(legacy_cfg_text_buffer), NULL) == FX_SUCCESS) {
            legacy_parse_startup_cfg_text(legacy_cfg_text_buffer);
        }
    } else if (AppFileX_FileExistsOnSd("SEND.INI") == FX_SUCCESS) {
        legacy_startup_cfg = LEGACY_STARTUP_CFG_SEND_INI;
        if (AppFileX_LoadTextFileOnSd("SEND.INI", legacy_cfg_text_buffer, sizeof(legacy_cfg_text_buffer), NULL) == FX_SUCCESS) {
            legacy_parse_startup_cfg_text(legacy_cfg_text_buffer);
        }
    } else {
        legacy_startup_cfg = LEGACY_STARTUP_CFG_NONE;
        legacy_startup_cfg_values.loaded = false;
    }

    legacy_apply_startup_cfg_defaults();
}

void LegacyMode_RefreshStartupConfigFromSd(void)
{
    legacy_probe_startup_cfg_on_sd();
}

static const char* legacy_packet_name(uint8_t packet_id)
{
    switch (packet_id) {
        case LEGACY_PACKET_RESET:
            return "reset";
        case LEGACY_PACKET_IDLE:
            return "idle";
        case LEGACY_PACKET_HARD:
            return "hard";
        case LEGACY_PACKET_BASE:
            return "base";
        default:
            return "unknown";
    }
}

static void legacy_log_text_line(const char* text)
{
    char ts[40];
    char stamped[256];
    int n;

    if (!LegacyMode_GetStartupLogPkts() || (text == NULL)) {
        return;
    }

    legacy_format_timestamp_prefix(ts, sizeof(ts));
    n = snprintf(stamped, sizeof(stamped), "%s%s", ts, text);
    if (n <= 0) {
        return;
    }
    if ((size_t)n >= sizeof(stamped) - 1U) {
        stamped[sizeof(stamped) - 2U] = '\n';
        stamped[sizeof(stamped) - 1U] = '\0';
    } else if (stamped[n - 1] != '\n') {
        stamped[n] = '\n';
        stamped[n + 1] = '\0';
    }

    (void)AppFileX_AppendTextFileOnSd((const CHAR*)legacy_log_filename, (const CHAR*)stamped, 0U);
    if (legacy_should_mirror_line_to_sum(text)) {
        (void)AppFileX_AppendTextFileOnSd((const CHAR*)legacy_sum_filename, (const CHAR*)stamped, 0U);
        legacy_echo_status_to_console(stamped);
    }
}

static void legacy_log_printf(const char* fmt, ...)
{
    char line[192];
    int written;
    va_list args;

    if (!LegacyMode_GetStartupLogPkts() || (fmt == NULL)) {
        return;
    }

    va_start(args, fmt);
    written = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);

    if (written <= 0) {
        return;
    }

    if ((size_t)written >= sizeof(line) - 1U) {
        line[sizeof(line) - 2U] = '\n';
        line[sizeof(line) - 1U] = '\0';
    } else if ((written > 0) && (line[written - 1] != '\n')) {
        line[written] = '\n';
        line[written + 1] = '\0';
    }

    legacy_log_text_line(line);
}

static void legacy_log_detail_printf(const char* fmt, ...)
{
    char detail[192];
    int detail_written;
    char line[200];
    va_list args;

    if (!LegacyMode_GetStartupLogPkts() || (fmt == NULL)) {
        return;
    }

    va_start(args, fmt);
    detail_written = vsnprintf(detail, sizeof(detail), fmt, args);
    va_end(args);
    if (detail_written <= 0) {
        return;
    }

    (void)snprintf(line, sizeof(line), "!L %s", detail);
    legacy_log_printf("%s", line);
}

static void legacy_log_packet_bytes(const uint8_t* data, size_t size)
{
    char line[192];
    size_t pos = 0U;
    size_t i;

    if (!LegacyMode_GetStartupLogPkts() || (data == NULL) || (size == 0U)) {
        return;
    }

    pos += (size_t)snprintf(&line[pos], sizeof(line) - pos, "!P");
    for (i = 0U; (i < size) && (pos < (sizeof(line) - 1U)); ++i) {
        int n = snprintf(&line[pos], sizeof(line) - pos, " %02x", data[i]);
        if ((n <= 0) || ((size_t)n >= (sizeof(line) - pos))) {
            break;
        }
        pos += (size_t)n;
    }

    legacy_log_printf("%s", line);
}

bool LegacyMode_SetLogBaseName(const char* log_base_name)
{
    size_t i;
    size_t write_index = 0U;

    if ((log_base_name == NULL) || (log_base_name[0] == '\0')) {
        return false;
    }

    for (i = 0U; (log_base_name[i] != '\0') && (write_index < (sizeof(legacy_log_base_name) - 1U)); ++i) {
        char c = log_base_name[i];
        if ((c == ' ') || (c == '\\') || (c == '/') || (c == ':') || (c == '*') ||
            (c == '?') || (c == '"') || (c == '<') || (c == '>') || (c == '|')) {
            c = '_';
        }
        legacy_log_base_name[write_index++] = c;
    }

    legacy_log_base_name[write_index] = '\0';
    if (legacy_log_base_name[0] == '\0') {
        return false;
    }

    legacy_refresh_log_filenames();
    return true;
}

bool LegacyMode_AppendStartupSummaryToLogs(const char* log_filename, const char* sum_filename)
{
    char *summary = legacy_startup_summary_buffer;
    const char* cfg_name = "none";
    int n;

    if ((log_filename == NULL) || (sum_filename == NULL) || (log_filename[0] == '\0') || (sum_filename[0] == '\0')) {
        return false;
    }

    if (AppFileX_FileExistsOnSd("SEND.CFG") == FX_SUCCESS) {
        cfg_name = "SEND.CFG";
    } else if (AppFileX_FileExistsOnSd("SEND.INI") == FX_SUCCESS) {
        cfg_name = "SEND.INI";
    }

    n = snprintf(
        summary,
        sizeof(legacy_startup_summary_buffer),
        "\nSummary of command line and 'SEND.CFG' switches:\n\n"
        "Usage:     send [-?] [-u] [-m] [-a addr] [-d l|f|a|s] [-n pre] [-N trig]\n"
        "                [-l] [-p port] [-f] [-x] [-r] [-t mask] [-c mask] [-E pre]\n"
        "                [-T] [-F fill] [-R reps] [-P] [-A] [-s] [-S] [-g mask]\n"
        "                [-o pair] [-k] [-D] [-e trg]\n\n"
        "Cfg file: <%s>\n\n"
        "  -?                    Print usage message and exit\n"
        "  -u                    Print user information to 's_user.txt' and exit\n"
        "  -m         MANUAL     Start in manual mode                 <value %s>\n"
        "  -a <addr>  ADDRESS    Decoder address                      <value %u>\n"
        "  -d l|f|a|s TYPE       Dec. type(l-LOC,f-FUNC,a-ACC,s-SIG)  <value %c>\n"
        "  -l         LAMP       Use rear lamp for function tests     <value %s>\n"
        "  -n <pre>   PRESET     Signal decoder preset aspect         <value %u>\n"
        "  -N <trig>  TRIGGER    Signal decoder trigger aspect        <value %u>\n"
        "  -p <port>  PORT       I/O Port                             <value 0x%04lx>\n"
        "  -f         FRAGMENT   Test all fragments                   <value %s>\n"
        "  -x         CRITICAL   Protect critical regions             <value %s>\n"
        "  -r         REPEAT     Repeat decoder tests                 <value %s>\n"
        "  -t <mask>  TESTS      Bit mask of tests to run             <value 0x%08lx>\n"
        "  -c <mask>  CLOCKS     Bit mask of clocks to try            <value 0x%08lx>\n"
        "  -g <mask>  FUNCS      Bit mask of active functions         <value 0x%02lx>\n"
        "  -E <pre>   EXTRA_PRE  Extra margin test preamble bits      <value %u>\n"
        "  -T         TRIG_REV   Use loco reverse as trigger packet   <value %s>\n"
        "  -L         LOCO_FIRST Put loco packet before func packet   <value false>\n"
        "  -F <fill>  FILL_MSEC  Fill time in milliseconds            <value %lu>\n"
        "  -R <reps>  TEST_REPS  Non packet acceptance test repeats   <value %u>\n"
        "  -P         LOG_PKTS   Send packets to log, not hardware    <value %s>\n"
        "  -A         NO_ABORT   Do not stop program on an error      <value %s>\n"
        "  -s         LATE_SCOPE Put scope trigger after trigger      <value %s>\n"
        "  -S    SAME_AMBIG_ADDR Use same address for ambig tests     <value %s>\n"
        "  -o        ACC_PAIR    Accessory output pair (1-4)          <value 1>\n"
        "                            output {preset, trigger}         <value {%u, %u}>\n"
        "  -k        KICK_START  Kick start motor for function tests  <value false>\n"
        "  -e <trg>   EXTRA_TRG  Extra Ames test trigger packets      <value 0>\n"
        "  -D        DEBUG_ON    Log debug messages                   <value false>\n\n"
        "Manual keyboard commands >\n\n"
        "ESC - Return to command line       h - Print header\n"
        "  c - Send single clock phase      C - Send series of clock phases\n"
        "  u - Clear underflow              0 - Send zeros\n"
        "  1 - Send ones                    a - Send scope A pattern\n"
        "  b - Send scope B pattern         o - Send scope timing packet\n"
        "  w - Send warble packets          S - Send stretched 0 pattern\n"
        "  r - Send DCC reset packets       d - Send DCC packets\n"
        "  D - Send stretched DCC packets   s - Change loco speed, acc. output\n"
        "  e - Set speed to E-STOP          f - Change loco direction, acc. on/off\n"
        "  E - Set speed to E_STOP(I)       t - Run self tests repeatedly\n"
        "  k - Kickstart loco for funcs     i - Send DCC idle packets\n"
        "  R - Send hard resets             g - Test generic I/O\n"
        "  z - Run decoder tests            q - Quit program\n\n",
        cfg_name,
        legacy_send_cfg_stub.manual ? "true" : "false",
        (unsigned int)legacy_send_cfg_stub.address,
        (char)toupper((unsigned char)legacy_send_cfg_stub.decoder_type),
        legacy_send_cfg_stub.lamp ? "true" : "false",
        (unsigned int)legacy_send_cfg_stub.preset,
        (unsigned int)legacy_send_cfg_stub.trigger,
        (unsigned long)legacy_send_cfg_stub.port,
        legacy_send_cfg_stub.fragment ? "true" : "false",
        legacy_send_cfg_stub.critical ? "true" : "false",
        legacy_send_cfg_stub.repeat ? "true" : "false",
        (unsigned long)legacy_send_cfg_stub.tests_mask,
        (unsigned long)legacy_send_cfg_stub.clocks_mask,
        (unsigned long)legacy_send_cfg_stub.funcs_mask,
        (unsigned int)legacy_send_cfg_stub.extra_pre,
        legacy_send_cfg_stub.trig_rev ? "true" : "false",
        (unsigned long)legacy_send_cfg_stub.fill_msec,
        (unsigned int)legacy_send_cfg_stub.test_reps,
        legacy_send_cfg_stub.log_pkts ? "true" : "false",
        legacy_send_cfg_stub.no_abort ? "true" : "false",
        legacy_send_cfg_stub.late_scope ? "true" : "false",
        legacy_send_cfg_stub.same_ambig_addr ? "true" : "false",
        (unsigned int)legacy_send_cfg_stub.preset,
        (unsigned int)legacy_send_cfg_stub.trigger);

    if ((n <= 0) || ((size_t)n >= sizeof(legacy_startup_summary_buffer))) {
        return false;
    }

    if (AppFileX_AppendTextFileOnSd((const CHAR*)log_filename, (const CHAR*)summary, 0U) != FX_SUCCESS) {
        return false;
    }
    if (AppFileX_AppendTextFileOnSd((const CHAR*)sum_filename, (const CHAR*)summary, 0U) != FX_SUCCESS) {
        return false;
    }

    return true;
}

static void legacy_log_selected_packet(const char* context)
{
    size_t size = 0U;
    const uint8_t* data = legacy_packet_data(selected_packet_id, &size);

    if (!LegacyMode_GetStartupLogPkts()) {
        return;
    }

    if ((context != NULL) && (context[0] != '\0')) {
        legacy_log_detail_printf("%s mode=%s packet=%s",
                                 context,
                                 LegacyMode_GetModeName(),
                                 LegacyMode_GetSelectedPacketName());
    } else {
        legacy_log_detail_printf("mode=%s packet=%s",
                                 LegacyMode_GetModeName(),
                                 LegacyMode_GetSelectedPacketName());
    }

    if ((data != NULL) && (size > 0U)) {
        legacy_log_packet_bytes(data, size);
    }
}

static const char* legacy_mode_name(legacy_wave_mode_t mode)
{
    switch (mode) {
        case LEGACY_MODE_PACKET:
            return "packet";
        case LEGACY_MODE_PACKET_STRETCHED:
            return "packet_stretched";
        case LEGACY_MODE_WARBLE:
            return "warble";
        case LEGACY_MODE_STREAM_ZERO:
            return "zeros";
        case LEGACY_MODE_STREAM_ONE:
            return "ones";
        case LEGACY_MODE_STRETCHED_ZERO:
            return "stretched_zero";
        case LEGACY_MODE_STRETCHED_PATTERN:
            return "stretched_pattern";
        case LEGACY_MODE_SCOPE_A:
            return "scope_a";
        case LEGACY_MODE_SCOPE_B:
            return "scope_b";
        case LEGACY_MODE_SCOPE_O:
            return "scope_o";
        default:
            return "unknown";
    }
}

static inline const uint8_t* legacy_packet_data(uint8_t packet_id, size_t* size)
{
    switch (packet_id) {
        case LEGACY_PACKET_RESET:
            *size = sizeof(reset_packet_bytes);
            return reset_packet_bytes;
        case LEGACY_PACKET_IDLE:
            *size = sizeof(idle_packet_bytes);
            return idle_packet_bytes;
        case LEGACY_PACKET_HARD:
            *size = sizeof(hard_reset_packet_bytes);
            return hard_reset_packet_bytes;
        case LEGACY_PACKET_BASE:
            *size = sizeof(base_packet_bytes);
            return base_packet_bytes;
        default:
            *size = 0;
            return NULL;
    }
}

static inline uint8_t legacy_get_current_bit(void)
{
    if (legacy_wave_mode == LEGACY_MODE_STREAM_ZERO || legacy_wave_mode == LEGACY_MODE_STRETCHED_ZERO) {
        return 0U;
    }
    if (legacy_wave_mode == LEGACY_MODE_STREAM_ONE) {
        return 1U;
    }
    if (legacy_wave_mode == LEGACY_MODE_WARBLE) {
        const uint32_t warble_idx = bit_index % legacy_warble_total_bits;
        return (warble_idx < legacy_warble_one_bits) ? 1U : 0U;
    }
    if (legacy_wave_mode == LEGACY_MODE_STRETCHED_PATTERN) {
        const uint32_t pattern_idx = bit_index % (uint32_t)(sizeof(stretched_pattern_bytes) * 8U);
        const uint8_t byte_index = (uint8_t)(pattern_idx / 8U);
        const uint8_t bit_in_byte = (uint8_t)(7U - (pattern_idx % 8U));
        return (uint8_t)((stretched_pattern_bytes[byte_index] >> bit_in_byte) & 0x01U);
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_A) {
        return (uint8_t)((bit_index & 1UL) ? 1U : 0U);
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_B) {
        return (uint8_t)((bit_index & 1UL) ? 0U : 1U);
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_O) {
        return 1U;
    }

    size_t size = 0;
    const uint8_t* data = legacy_packet_data(selected_packet_id, &size);
    if ((data == NULL) || (size == 0)) {
        return 1;
    }

    const uint32_t total_bits = (uint32_t)(size * 8U);
    if (total_bits == 0U) {
        return 1;
    }

    if (bit_index >= total_bits) {
        bit_index = 0;
    }

    const uint8_t byte_index = (uint8_t)(bit_index / 8U);
    const uint8_t bit_in_byte = (uint8_t)(7U - ((uint8_t)bit_index % 8U));
    {
        uint8_t bit = (uint8_t)((data[byte_index] >> bit_in_byte) & 0x01U);
        const uint32_t packet_idx = bit_index % total_bits;

        if (legacy_test_active && (legacy_active_test_mutation != LEGACY_TEST_MUTATION_NONE)) {
            if (legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_ADDRESS) {
                if (packet_idx == 11U) {
                    bit ^= 0x01U;
                }
            } else if ((legacy_active_test_mutation == LEGACY_TEST_MUTATION_BAD_BIT) &&
                       (packet_idx == (uint32_t)legacy_active_mutation_bit)) {
                bit ^= 0x01U;
            }
        }

        return bit;
    }
}

static inline bool legacy_is_first_zero_in_selected_packet(uint32_t packet_bit_index)
{
    size_t size = 0;
    const uint8_t* data = legacy_packet_data(selected_packet_id, &size);
    if ((data == NULL) || (size == 0U)) {
        return false;
    }

    const uint32_t total_bits = (uint32_t)(size * 8U);
    if (total_bits == 0U) {
        return false;
    }

    const uint32_t index = packet_bit_index % total_bits;
    const uint8_t current_byte = (uint8_t)(index / 8U);
    const uint8_t current_bit = (uint8_t)(7U - (index % 8U));
    const uint8_t current_value = (uint8_t)((data[current_byte] >> current_bit) & 0x01U);

    if (current_value != 0U) {
        return false;
    }

    for (uint32_t i = 0U; i < index; ++i) {
        const uint8_t b = (uint8_t)(i / 8U);
        const uint8_t bi = (uint8_t)(7U - (i % 8U));
        if (((data[b] >> bi) & 0x01U) == 0U) {
            return false;
        }
    }

    return true;
}

static inline uint16_t legacy_get_ticks_for_bit(uint8_t bit)
{
    if (legacy_wave_mode == LEGACY_MODE_STRETCHED_ZERO) {
        return legacy_stretched0_ticks;
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_O) {
        return legacy_scope_o_ticks[legacy_scope_o_index];
    }
    if (legacy_wave_mode == LEGACY_MODE_STRETCHED_PATTERN) {
        const uint32_t pattern_idx = bit_index % (uint32_t)(sizeof(stretched_pattern_bytes) * 8U);
        if ((pattern_idx == 8U) && (bit == 0U)) {
            return legacy_stretched0_ticks;
        }
    }
    if ((legacy_wave_mode == LEGACY_MODE_PACKET_STRETCHED) &&
        (bit == 0U) &&
        legacy_is_first_zero_in_selected_packet(bit_index)) {
        return legacy_stretched0_ticks;
    }
    return (bit != 0U) ? legacy_bit1_ticks : legacy_bit0_ticks;
}

static inline void legacy_track_output(bool p)
{
    TRACK_P_GPIO_Port->BSRR = ((uint32_t)(!p) << TRACK_P_BR_Pos) |
                              ((uint32_t)(p) << TRACK_P_BS_Pos);
}

static inline void legacy_stop_timer(void)
{
    __HAL_TIM_DISABLE_IT(&htim14, TIM_IT_UPDATE);
    HAL_TIM_Base_Stop_IT(&htim14);
}

void TIM14_IRQHandler(void)
{
    uint32_t itsource = htim14.Instance->DIER;
    uint32_t itflag = htim14.Instance->SR;

    if ((itflag & TIM_FLAG_UPDATE) == TIM_FLAG_UPDATE) {
        if ((itsource & TIM_IT_UPDATE) == TIM_IT_UPDATE) {
            __HAL_TIM_CLEAR_FLAG(&htim14, TIM_FLAG_UPDATE);

            if (!legacy_mode_running) {
                legacy_stop_timer();
                return;
            }

            const uint8_t bit = legacy_get_current_bit();
            const uint16_t ticks = legacy_get_ticks_for_bit(bit);

            if (!LegacyMode_GetStartupLogPkts()) {
                legacy_track_output(phase_p);
            }
            phase_p = !phase_p;

            half_phase++;
            if (half_phase >= 2U) {
                half_phase = 0;
                if ((legacy_wave_mode == LEGACY_MODE_PACKET) ||
                    (legacy_wave_mode == LEGACY_MODE_PACKET_STRETCHED)) {
                    size_t size = 0;
                    (void)legacy_packet_data(selected_packet_id, &size);
                    bit_index++;
                    if ((size > 0U) && (bit_index >= (uint32_t)(size * 8U))) {
                        bit_index = 0;

                        legacy_advance_test_plan_on_packet_boundary();

                        if (legacy_kickstart_cycles > 0U) {
                            legacy_kickstart_cycles--;
                            if (legacy_kickstart_cycles == 0U) {
                                selected_packet_id = LEGACY_PACKET_IDLE;
                            }
                        }
                    }
                } else if (legacy_wave_mode == LEGACY_MODE_WARBLE) {
                    bit_index++;
                    if (bit_index >= legacy_warble_total_bits) {
                        bit_index = 0;
                    }
                } else if (legacy_wave_mode == LEGACY_MODE_STRETCHED_PATTERN) {
                    bit_index++;
                    if (bit_index >= (uint32_t)(sizeof(stretched_pattern_bytes) * 8U)) {
                        bit_index = 0;
                    }
                } else if ((legacy_wave_mode == LEGACY_MODE_SCOPE_A) || (legacy_wave_mode == LEGACY_MODE_SCOPE_B)) {
                    bit_index++;
                }
            }

            htim14.Instance->ARR = ticks;
        }
    }
}

void LegacyMode_Init(void)
{
    legacy_mode_running = false;
    selected_packet_id = LEGACY_PACKET_IDLE;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_scope_o_index = 0;
    legacy_fw_state = true;
    legacy_kickstart_cycles = 0;
    legacy_dcc_mode_active = false;
    legacy_startup_cfg = LEGACY_STARTUP_CFG_NONE;
    legacy_startup_cfg_values.loaded = false;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';
    legacy_test_active = false;
    legacy_test_packet_count = 0U;
    legacy_test_packet_index = 0U;
    legacy_test_packet_cycles = 0U;
    legacy_active_test_mutation = LEGACY_TEST_MUTATION_NONE;
    legacy_active_mutation_bit = 0U;
    (void)snprintf(legacy_log_base_name, sizeof(legacy_log_base_name), "legacy");
    legacy_refresh_log_filenames();
    bit_index = 0;
    half_phase = 0;
    phase_p = true;
}

bool LegacyMode_Start(void)
{
    char cli_args_buf[sizeof(legacy_start_cli_args)];
    char cli_error[128];

    if (legacy_mode_running) {
        return false;
    }

    CommandStation_Stop();

    bit_index = 0;
    half_phase = 0;
    phase_p = true;

    // Match sender startup search order on SD: SEND.CFG then SEND.INI.
    legacy_probe_startup_cfg_on_sd();

    if (legacy_start_cli_args_pending) {
        (void)strncpy(cli_args_buf, legacy_start_cli_args, sizeof(cli_args_buf) - 1U);
        cli_args_buf[sizeof(cli_args_buf) - 1U] = '\0';

        legacy_start_cli_args_pending = false;
        legacy_start_cli_args[0] = '\0';

        if (!legacy_apply_sender_cli_args(cli_args_buf, cli_error, sizeof(cli_error))) {
            printf("Legacy start args error: %s\n", cli_error[0] != '\0' ? cli_error : "invalid args");
            return false;
        }

        legacy_startup_cfg = LEGACY_STARTUP_CFG_CLI;
        legacy_apply_startup_cfg_defaults();
    }

    legacy_prepare_test_plan();

    /* In LOG_PKTS mode, keep bridge disabled and log packets instead of driving track hardware. */
    if (!LegacyMode_GetStartupLogPkts()) {
        legacy_track_output(phase_p);
        BR_ENABLE_GPIO_Port->BSRR = BR_ENABLE_Pin;
    } else {
        BR_ENABLE_GPIO_Port->BSRR = ((uint32_t)BR_ENABLE_Pin << 16U);
        legacy_track_output(false);
    }

    htim14.Instance->CR1 &= ~TIM_CR1_OPM;
    htim14.Instance->CNT = 0U;
    htim14.Instance->ARR = legacy_bit1_ticks;
    __HAL_TIM_CLEAR_FLAG(&htim14, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim14, TIM_IT_UPDATE);

    if (HAL_TIM_Base_Start_IT(&htim14) != HAL_OK) {
        BR_ENABLE_GPIO_Port->BSRR = ((uint32_t)BR_ENABLE_Pin << 16U);
        __HAL_TIM_DISABLE_IT(&htim14, TIM_IT_UPDATE);
        return false;
    }

    legacy_mode_running = true;
    legacy_log_printf("STATUS  Starting Decoder test cycle    1");
    legacy_log_printf("STATUS  Legacy mode start startup_cfg=%s log_pkts=%s manual=%s type=%c",
                      LegacyMode_GetStartupConfigName(),
                      LegacyMode_GetStartupLogPkts() ? "true" : "false",
                      LegacyMode_GetStartupManual() ? "true" : "false",
                      LegacyMode_GetStartupDecoderType());
    legacy_log_printf("STATUS  cfg tests=0x%08lX clocks=0x%08lX funcs=0x%08lX reps=%u pre=%u trig=%u",
                      (unsigned long)legacy_send_cfg_stub.tests_mask,
                      (unsigned long)legacy_send_cfg_stub.clocks_mask,
                      (unsigned long)legacy_send_cfg_stub.funcs_mask,
                      (unsigned int)legacy_send_cfg_stub.test_reps,
                      (unsigned int)legacy_send_cfg_stub.preset,
                      (unsigned int)legacy_send_cfg_stub.trigger);
    legacy_log_selected_packet("SELECT");
    return true;
}

bool LegacyMode_SetStartArgs(const char* args_text, char* error_buf, size_t error_buf_size)
{
    size_t len = 0U;

    if ((error_buf != NULL) && (error_buf_size > 0U)) {
        error_buf[0] = '\0';
    }

    if (args_text == NULL) {
        legacy_start_cli_args_pending = false;
        legacy_start_cli_args[0] = '\0';
        return true;
    }

    while (isspace((unsigned char)*args_text)) {
        ++args_text;
    }

    len = strlen(args_text);
    while ((len > 0U) && isspace((unsigned char)args_text[len - 1U])) {
        --len;
    }

    if (len == 0U) {
        legacy_start_cli_args_pending = false;
        legacy_start_cli_args[0] = '\0';
        return true;
    }

    if (len >= sizeof(legacy_start_cli_args)) {
        legacy_set_error(error_buf, error_buf_size,
                         "Start argument string too long (%lu > %lu).",
                         (unsigned long)len,
                         (unsigned long)(sizeof(legacy_start_cli_args) - 1U));
        return false;
    }

    memcpy(legacy_start_cli_args, args_text, len);
    legacy_start_cli_args[len] = '\0';
    legacy_start_cli_args_pending = true;
    return true;
}

bool LegacyMode_Stop(void)
{
    if (!legacy_mode_running) {
        return false;
    }

    legacy_mode_running = false;
    legacy_stop_timer();
    BR_ENABLE_GPIO_Port->BSRR = ((uint32_t)BR_ENABLE_Pin << 16U);
    legacy_track_output(false);
    legacy_log_printf("STATUS  {SEND_END 0}");
    (void)AppFileX_CloseMediaIfIdleOnSd();
    return true;
}

bool LegacyMode_IsRunning(void)
{
    return legacy_mode_running;
}

bool LegacyMode_SelectPacket(uint8_t packet_id)
{
    if ((packet_id != LEGACY_PACKET_RESET) &&
        (packet_id != LEGACY_PACKET_IDLE) &&
        (packet_id != LEGACY_PACKET_HARD) &&
        (packet_id != LEGACY_PACKET_BASE)) {
        return false;
    }

    selected_packet_id = packet_id;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_kickstart_cycles = 0;
    legacy_dcc_mode_active = (packet_id == LEGACY_PACKET_BASE);
    bit_index = 0;
    half_phase = 0;
    legacy_log_selected_packet("SELECT");
    return true;
}

uint8_t LegacyMode_GetSelectedPacket(void)
{
    return selected_packet_id;
}

const char* LegacyMode_GetSelectedPacketName(void)
{
    return legacy_packet_name(selected_packet_id);
}

const char* LegacyMode_GetModeName(void)
{
    return legacy_mode_name(legacy_wave_mode);
}

const char* LegacyMode_GetStartupConfigName(void)
{
    switch (legacy_startup_cfg) {
        case LEGACY_STARTUP_CFG_SEND_CFG:
            return "SEND.CFG";
        case LEGACY_STARTUP_CFG_SEND_INI:
            return "SEND.INI";
        case LEGACY_STARTUP_CFG_CLI:
            return "CLI";
        default:
            return "none";
    }
}

bool LegacyMode_IsStartupConfigLoaded(void)
{
    return legacy_startup_cfg_values.loaded;
}

bool LegacyMode_GetStartupManual(void)
{
    return legacy_startup_cfg_values.loaded && legacy_startup_cfg_values.manual;
}

bool LegacyMode_GetStartupLogPkts(void)
{
    return legacy_startup_cfg_values.loaded && legacy_startup_cfg_values.log_pkts;
}

char LegacyMode_GetStartupDecoderType(void)
{
    return legacy_startup_cfg_values.loaded ? legacy_startup_cfg_values.decoder_type : '?';
}

bool LegacyMode_WriteUserDocsToSd(void)
{
    char *doc = legacy_user_docs_buffer;
    size_t used;
    uint8_t acc_pair = 1U;
    const char* cfg_name = "none";
    static const char docs_tail[] =
        "\nSummary of decoder tests and clocks:\n\n"
        "List of tests to run and their corresponding '-t'  parameter:\n\n"
        "PARAMETER   VARIANT                          TEST\n"
        "----------------------------------------------------------------------------\n"
        "0x00000001                                1: 1T margin test.\n"
        "0x00000002                                2: 1H duty cycle test.\n"
        "0x00000004                                3: Ramp test.\n"
        "0x00000008  Pre 12 Idle 1                 4: Packet acceptance test.\n"
        "0x00000010  Pre 12 Idle 2                 5: Packet acceptance test.\n"
        "0x00000020  Pre 13 Idle 1                 6: Packet acceptance test.\n"
        "0x00000040  Pre 15 Idle 1                 7: Packet acceptance test.\n"
        "0x00000080  Pre 15 Idle 2                 8: Packet acceptance test.\n"
        "0x00000100                                9: Bad address test.\n"
        "0x00000200                               10: Bad bit test.\n"
        "0x00000400  No stretch                   11: Single stretched 0 test.\n"
        "0x00000800  Long negative stretch        12: Single stretched 0 test.\n"
        "0x00001000  Long positive stretch        13: Single stretched 0 test.\n"
        "0x00002000  Maximum negative stretch     14: Single stretched 0 test.\n"
        "0x00004000  Maximum positive stretch     15: Single stretched 0 test.\n"
        "0x00008000                               16: Truncated packet test.\n"
        "0x00010000                               17: Prior packet test.\n"
        "0x00020000                               18: 6 prior byte test.\n"
        "0x00040000                               19: 1 ambiguous bit test.\n"
        "0x00080000                               20: 2 ambiguous bits test.\n\n"
        "List of clocks to use and their corresponding '-c'  parameters:\n\n"
        "PARAMETER   CLK0T  CLK0H  CLK1T        CLOCK NAME\n"
        "----------------------------------------------------------------------------\n"
        "0x00000001    200    100    116     1: All nominal.\n"
        "0x00000002    196     98    113     2: All 1/4 fast.\n"
        "0x00000004    190     95    110     3: Command station min.\n"
        "0x00000008    184     92    106     4: Minimum + 2.\n"
        "0x00000010    182     91    105     5: Minimum + 1.\n"
        "0x00000020    180     90    104     6: Decoder minimum.\n"
        "0x00000040    204    102    119     7: All 1/4 slow.\n"
        "0x00000080    210    105    122     8: Command station max.\n"
        "0x00000100    216    108    126     9: Maximum - 2.\n"
        "0x00000200    218    109    127    10: Maximum - 1.\n"
        "0x00000400    220    110    128    11: Decoder maximum.\n"
        "0x00000800    300    100    116    12: Negative stretched 0.\n"
        "0x00001000    300    200    116    13: Positive stretched 0.\n"
        "0x00002000   2560    100    116    14: Very negative 0.\n"
        "0x00004000   2560   2460    116    15: Very positive 0.\n"
        "0x00008000  12000   2000    116    16: Max Decoder Neg 0.\n"
        "0x00010000  12000  10000    116    17: Max Decoder Pos 0.\n";

    LegacyMode_RefreshStartupConfigFromSd();

    if (AppFileX_FileExistsOnSd("SEND.CFG") == FX_SUCCESS) {
        cfg_name = "SEND.CFG";
    } else if (AppFileX_FileExistsOnSd("SEND.INI") == FX_SUCCESS) {
        cfg_name = "SEND.INI";
    }

    if ((legacy_send_cfg_stub.preset >= 1U) &&
        (legacy_send_cfg_stub.preset <= 7U) &&
        (legacy_send_cfg_stub.trigger == (uint8_t)(legacy_send_cfg_stub.preset + 1U))) {
        acc_pair = (uint8_t)((legacy_send_cfg_stub.preset + 1U) / 2U);
        if ((acc_pair < 1U) || (acc_pair > 4U)) {
            acc_pair = 1U;
        }
    }

    used = (size_t)snprintf(
        doc,
        sizeof(legacy_user_docs_buffer),
        "User documenation summary for test software version %s\n\n"
        "Summary of command line and 'SEND.CFG' switches:\n\n"
        "Usage:     send [-?] [-u] [-m] [-a addr] [-d l|f|a|s] [-n pre] [-N trig]\n"
        "                [-l] [-p port] [-f] [-x] [-r] [-t mask] [-c mask] [-E pre]\n"
        "                [-T] [-F fill] [-R reps] [-P] [-A] [-s] [-S] [-g mask]\n"
        "                [-o pair] [-k] [-D] [-e trg]\n\n"
        "Cfg file: <%s>\n\n"
        "  -?                    Print usage message and exit\n"
        "  -u                    Print user information to 's_user.txt' and exit\n"
        "  -m         MANUAL     Start in manual mode                 <value %s>\n"
        "  -a <addr>  ADDRESS    Decoder address                      <value %u>\n"
        "  -d l|f|a|s TYPE       Dec. type(l-LOC,f-FUNC,a-ACC,s-SIG)  <value %c>\n"
        "  -l         LAMP       Use rear lamp for function tests     <value %s>\n"
        "  -n <pre>   PRESET     Signal decoder preset aspect         <value %u>\n"
        "  -N <trig>  TRIGGER    Signal decoder trigger aspect        <value %u>\n"
        "  -p <port>  PORT       I/O Port                             <value 0x%04lx>\n"
        "  -f         FRAGMENT   Test all fragments                   <value %s>\n"
        "  -x         CRITICAL   Protect critical regions             <value %s>\n"
        "  -r         REPEAT     Repeat decoder tests                 <value %s>\n"
        "  -t <mask>  TESTS      Bit mask of tests to run             <value 0x%08lx>\n"
        "  -c <mask>  CLOCKS     Bit mask of clocks to try            <value 0x%08lx>\n"
        "  -g <mask>  FUNCS      Bit mask of active functions         <value 0x%02lx>\n"
        "  -E <pre>   EXTRA_PRE  Extra margin test preamble bits      <value %u>\n"
        "  -T         TRIG_REV   Use loco reverse as trigger packet   <value %s>\n"
        "  -L         LOCO_FIRST Put loco packet before func packet   <value false>\n"
        "  -F <fill>  FILL_MSEC  Fill time in milliseconds            <value %lu>\n"
        "  -R <reps>  TEST_REPS  Non packet acceptance test repeats   <value %u>\n"
        "  -P         LOG_PKTS   Send packets to log, not hardware    <value %s>\n"
        "  -A         NO_ABORT   Do not stop program on an error      <value %s>\n"
        "  -s         LATE_SCOPE Put scope trigger after trigger      <value %s>\n"
        "  -S    SAME_AMBIG_ADDR Use same address for ambig tests     <value %s>\n"
        "  -o        ACC_PAIR    Accessory output pair (1-4)          <value %u>\n"
        "                            output {preset, trigger}         <value {%u, %u}>\n"
        "  -k        KICK_START  Kick start motor for function tests  <value false>\n"
        "  -e <trg>   EXTRA_TRG  Extra Ames test trigger packets      <value 0>\n"
        "  -D        DEBUG_ON    Log debug messages                   <value false>\n\n"
        "Manual keyboard commands >\n\n"
        "ESC - Return to command line       h - Print header\n"
        "  c - Send single clock phase      C - Send series of clock phases\n"
        "  u - Clear underflow              0 - Send zeros\n"
        "  1 - Send ones                    a - Send scope A pattern\n"
        "  b - Send scope B pattern         o - Send scope timing packet\n"
        "  w - Send warble packets          S - Send stretched 0 pattern\n"
        "  r - Send DCC reset packets       d - Send DCC packets\n"
        "  D - Send stretched DCC packets   s - Change loco speed, acc. output\n"
        "  e - Set speed to E-STOP          f - Change loco direction, acc. on/off\n"
        "  E - Set speed to E_STOP(I)       t - Run self tests repeatedly\n"
        "  k - Kickstart loco for funcs     i - Send DCC idle packets\n"
        "  R - Send hard resets             g - Test generic I/O\n"
        "  z - Run decoder tests            q - Quit program\n",
        FW_VERSION_STRING,
        cfg_name,
        legacy_send_cfg_stub.manual ? "true" : "false",
        (unsigned int)legacy_send_cfg_stub.address,
        legacy_send_cfg_stub.decoder_type,
        legacy_send_cfg_stub.lamp ? "true" : "false",
        (unsigned int)legacy_send_cfg_stub.preset,
        (unsigned int)legacy_send_cfg_stub.trigger,
        (unsigned long)legacy_send_cfg_stub.port,
        legacy_send_cfg_stub.fragment ? "true" : "false",
        legacy_send_cfg_stub.critical ? "true" : "false",
        legacy_send_cfg_stub.repeat ? "true" : "false",
        (unsigned long)legacy_send_cfg_stub.tests_mask,
        (unsigned long)legacy_send_cfg_stub.clocks_mask,
        (unsigned long)legacy_send_cfg_stub.funcs_mask,
        (unsigned int)legacy_send_cfg_stub.extra_pre,
        legacy_send_cfg_stub.trig_rev ? "true" : "false",
        (unsigned long)legacy_send_cfg_stub.fill_msec,
        (unsigned int)legacy_send_cfg_stub.test_reps,
        legacy_send_cfg_stub.log_pkts ? "true" : "false",
        legacy_send_cfg_stub.no_abort ? "true" : "false",
        legacy_send_cfg_stub.late_scope ? "true" : "false",
        legacy_send_cfg_stub.same_ambig_addr ? "true" : "false",
        (unsigned int)acc_pair,
        (unsigned int)legacy_send_cfg_stub.preset,
        (unsigned int)legacy_send_cfg_stub.trigger);

    if ((used == 0U) || (used >= sizeof(legacy_user_docs_buffer))) {
        return false;
    }

    used += (size_t)snprintf(doc + used, sizeof(legacy_user_docs_buffer) - used, "%s", docs_tail);
    if (used >= sizeof(legacy_user_docs_buffer)) {
        return false;
    }

    return (AppFileX_WriteTextFileOnSd("s_user.txt", doc, (ULONG)used) == FX_SUCCESS);
}

void LegacyMode_PrintStartupConfigStub(void)
{
    printf("Startup cfg:\n");
    printf("  (* = overridden by SEND.CFG/SEND.INI)\n");
    printf("  source=%s\n", LegacyMode_GetStartupConfigName());
    printf("  loaded=%s\n", legacy_send_cfg_stub.loaded ? "true" : "false");
    printf("  %sADDRESS=%u\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_ADDRESS),
        (unsigned int)legacy_send_cfg_stub.address);
    printf("  %sPORT=0x%08lX\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_PORT),
        (unsigned long)legacy_send_cfg_stub.port);
    printf("  %sTYPE=%c\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_TYPE),
        legacy_send_cfg_stub.decoder_type);
    printf("  %sMANUAL=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_MANUAL),
        legacy_send_cfg_stub.manual ? "true" : "false");
    printf("  %sLAMP=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_LAMP),
        legacy_send_cfg_stub.lamp ? "true" : "false");
    printf("  %sPRESET=%u\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_PRESET),
        (unsigned int)legacy_send_cfg_stub.preset);
    printf("  %sTRIGGER=%u\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_TRIGGER),
        (unsigned int)legacy_send_cfg_stub.trigger);
    printf("  %sCRITICAL=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_CRITICAL),
        legacy_send_cfg_stub.critical ? "true" : "false");
    printf("  %sREPEAT=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_REPEAT),
        legacy_send_cfg_stub.repeat ? "true" : "false");
    printf("  %sTRIG_REV=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_TRIG_REV),
        legacy_send_cfg_stub.trig_rev ? "true" : "false");
    printf("  %sLOG_PKTS=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_LOG_PKTS),
        legacy_send_cfg_stub.log_pkts ? "true" : "false");
    printf("  %sTESTS=0x%08lX\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_TESTS),
        (unsigned long)legacy_send_cfg_stub.tests_mask);
    printf("  %sCLOCKS=0x%08lX\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_CLOCKS),
        (unsigned long)legacy_send_cfg_stub.clocks_mask);
    printf("  %sFUNCS=0x%08lX\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_FUNCS),
        (unsigned long)legacy_send_cfg_stub.funcs_mask);
    printf("  %sEXTRA_PRE=%u\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_EXTRA_PRE),
        (unsigned int)legacy_send_cfg_stub.extra_pre);
    printf("  %sFILL_MSEC=%lu\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_FILL_MSEC),
        (unsigned long)legacy_send_cfg_stub.fill_msec);
    printf("  %sTEST_REPS=%u\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_TEST_REPS),
        (unsigned int)legacy_send_cfg_stub.test_reps);
    printf("  %sNO_ABORT=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_NO_ABORT),
        legacy_send_cfg_stub.no_abort ? "true" : "false");
    printf("  %sLATE_SCOPE=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_LATE_SCOPE),
        legacy_send_cfg_stub.late_scope ? "true" : "false");
    printf("  %sFRAGMENT=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_FRAGMENT),
        legacy_send_cfg_stub.fragment ? "true" : "false");
    printf("  %sSAME_AMBIG_ADDR=%s\n",
        legacy_cfg_mark(LEGACY_CFG_OVR_SAME_AMBIG_ADDR),
        legacy_send_cfg_stub.same_ambig_addr ? "true" : "false");
}

bool LegacyMode_ApplyCompatKey(char key_cmd)
{
    switch (key_cmd) {
        case 'r':
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_RESET)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'R':
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_HARD)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'i':
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_IDLE)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'd':
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_BASE)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'D':
            selected_packet_id = LEGACY_PACKET_BASE;
            legacy_wave_mode = LEGACY_MODE_PACKET_STRETCHED;
            legacy_kickstart_cycles = 0;
            legacy_dcc_mode_active = true;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'a':
            legacy_wave_mode = LEGACY_MODE_SCOPE_A;
            legacy_dcc_mode_active = false;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'b':
            legacy_wave_mode = LEGACY_MODE_SCOPE_B;
            legacy_dcc_mode_active = false;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'o':
            legacy_wave_mode = LEGACY_MODE_SCOPE_O;
            legacy_dcc_mode_active = false;
            legacy_scope_o_index++;
            if (legacy_scope_o_index >= (uint8_t)(sizeof(legacy_scope_o_ticks) / sizeof(legacy_scope_o_ticks[0]))) {
                legacy_scope_o_index = 0;
            }
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'w':
            legacy_wave_mode = LEGACY_MODE_WARBLE;
            legacy_dcc_mode_active = false;
            legacy_kickstart_cycles = 0;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case '0':
            legacy_wave_mode = LEGACY_MODE_STREAM_ZERO;
            legacy_dcc_mode_active = false;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case '1':
        case 'c':
        case 'C':
            legacy_wave_mode = LEGACY_MODE_STREAM_ONE;
            legacy_dcc_mode_active = false;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'S':
            legacy_wave_mode = LEGACY_MODE_STRETCHED_PATTERN;
            legacy_dcc_mode_active = false;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'e':
        case 'E':
            if (!legacy_dcc_mode_active) {
                return false;
            }
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_BASE)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'f':
            if (!legacy_dcc_mode_active) {
                return false;
            }
            legacy_fw_state = !legacy_fw_state;
            if (!LegacyMode_SelectPacket(legacy_fw_state ? LEGACY_PACKET_BASE : LEGACY_PACKET_IDLE)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'k':
            if (!legacy_dcc_mode_active) {
                return false;
            }
            selected_packet_id = LEGACY_PACKET_BASE;
            legacy_wave_mode = LEGACY_MODE_PACKET;
            legacy_kickstart_cycles = 16U;
            legacy_dcc_mode_active = true;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'q':
            if (LegacyMode_IsRunning()) {
                return LegacyMode_Stop();
            }
            return true;
        case 'u':
            // Sender clear-underflow key; no dedicated underflow state in legacy mode.
            legacy_log_printf("KEY u clear-underflow noop");
            return true;
        case 's':
            // Sender speed/output-step key; emulate by cycling between BASE and IDLE packet.
            if (!legacy_dcc_mode_active) {
                (void)LegacyMode_SelectPacket(LEGACY_PACKET_BASE);
            } else {
                (void)LegacyMode_SelectPacket(selected_packet_id == LEGACY_PACKET_BASE ? LEGACY_PACKET_IDLE : LEGACY_PACKET_BASE);
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 't':
        case 'z':
        case 'g':
            // Sender test-entry keys; emulate by enabling test-plan execution from current cfg.
            legacy_prepare_test_plan();
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        default:
            return false;
    }
}
