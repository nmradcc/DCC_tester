#include "legacy_mode.h"
#include "main.h"
#include "app_filex.h"

#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "command_station.h"

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

static const uint16_t legacy_bit1_ticks = 58;   // ~58us @ 1MHz
static const uint16_t legacy_bit0_ticks = 100;  // ~100us @ 1MHz
static const uint16_t legacy_stretched0_ticks = 130;
static const uint16_t legacy_scope_o_ticks[] = {100, 10000, 1000, 10};
static uint8_t legacy_scope_o_index = 0;
static bool legacy_fw_state = true;
static uint8_t legacy_kickstart_cycles = 0;
static bool legacy_dcc_mode_active = false;

typedef enum {
    LEGACY_STARTUP_CFG_NONE = 0,
    LEGACY_STARTUP_CFG_SEND_CFG,
    LEGACY_STARTUP_CFG_SEND_INI
} legacy_startup_cfg_t;

static legacy_startup_cfg_t legacy_startup_cfg = LEGACY_STARTUP_CFG_NONE;

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
    0x0000007fU,
    0x1fU,
    2U,
    false,
    1000U,
    4U,
    false,
    false,
    false,
    false,
    false
};
static char legacy_cfg_text_buffer[2048];

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

static void legacy_parse_startup_cfg_text(char* cfg_text)
{
    char* line = cfg_text;

    legacy_startup_cfg_values.loaded = true;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';

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
    legacy_send_cfg_stub.clocks_mask = 0x0000007fU;
    legacy_send_cfg_stub.funcs_mask = 0x1fU;
    legacy_send_cfg_stub.extra_pre = 2U;
    legacy_send_cfg_stub.trig_rev = false;
    legacy_send_cfg_stub.fill_msec = 1000U;
    legacy_send_cfg_stub.test_reps = 4U;
    legacy_send_cfg_stub.log_pkts = false;
    legacy_send_cfg_stub.no_abort = false;
    legacy_send_cfg_stub.late_scope = false;
    legacy_send_cfg_stub.fragment = false;
    legacy_send_cfg_stub.same_ambig_addr = false;

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
    return (uint8_t)((data[byte_index] >> bit_in_byte) & 0x01U);
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

            legacy_track_output(phase_p);
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
    bit_index = 0;
    half_phase = 0;
    phase_p = true;
}

bool LegacyMode_Start(void)
{
    if (legacy_mode_running) {
        return false;
    }

    CommandStation_Stop();

    bit_index = 0;
    half_phase = 0;
    phase_p = true;

    /* Set initial polarity then enable bridge before starting timer IRQ cadence. */
    legacy_track_output(phase_p);
    BR_ENABLE_GPIO_Port->BSRR = BR_ENABLE_Pin;

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
    // Match sender startup search order on SD: SEND.CFG then SEND.INI.
    legacy_probe_startup_cfg_on_sd();
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
        default:
            return "none";
    }
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
        default:
            return false;
    }
}
