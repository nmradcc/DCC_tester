#include "legacy_mode.h"
#include "main.h"
#include "app_filex.h"

#include <stddef.h>
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

static legacy_startup_cfg_values_t legacy_startup_cfg_values = {false, false, false, 'l'};
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

static void legacy_parse_startup_cfg_text(char* cfg_text)
{
    char* line = cfg_text;

    legacy_startup_cfg_values.loaded = true;
    legacy_startup_cfg_values.manual = false;
    legacy_startup_cfg_values.log_pkts = false;
    legacy_startup_cfg_values.decoder_type = 'l';

    while ((line != NULL) && (*line != '\0')) {
        char* next = strpbrk(line, "\r\n");
        if (next != NULL) {
            *next = '\0';
        }

        char* start = line;
        char* end = line + strlen(line);
        legacy_trim_spaces(&start, &end);

        if ((start < end) && (*start != ';') && (*start != '#')) {
            char* token_end = start;
            while ((token_end < end) && !isspace((unsigned char)*token_end)) {
                ++token_end;
            }

            if (legacy_token_equals(start, (size_t)(token_end - start), "MANUAL")) {
                legacy_startup_cfg_values.manual = true;
            } else if (legacy_token_equals(start, (size_t)(token_end - start), "LOG_PKTS")) {
                legacy_startup_cfg_values.log_pkts = true;
            } else if (legacy_token_equals(start, (size_t)(token_end - start), "TYPE")) {
                char* value = token_end;
                while ((value < end) && isspace((unsigned char)*value)) {
                    ++value;
                }
                if (value < end) {
                    const char type_char = (char)tolower((unsigned char)*value);
                    if ((type_char == 'l') || (type_char == 'f') || (type_char == 'a') || (type_char == 's')) {
                        legacy_startup_cfg_values.decoder_type = type_char;
                    }
                }
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
