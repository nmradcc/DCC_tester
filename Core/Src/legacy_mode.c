#include "legacy_mode.h"
#include "main.h"

#include <stddef.h>

#include "command_station.h"

static bool legacy_mode_running = false;
static uint8_t legacy_reserved_timer = 14;
static uint8_t selected_packet_id = LEGACY_PACKET_IDLE;

typedef enum {
    LEGACY_MODE_PACKET = 0,
    LEGACY_MODE_PACKET_STRETCHED,
    LEGACY_MODE_WARBLE,
    LEGACY_MODE_STREAM_ZERO,
    LEGACY_MODE_STREAM_ONE,
    LEGACY_MODE_STRETCHED_ZERO,
    LEGACY_MODE_SCOPE_A,
    LEGACY_MODE_SCOPE_B,
    LEGACY_MODE_SCOPE_O
} legacy_wave_mode_t;

static legacy_wave_mode_t legacy_wave_mode = LEGACY_MODE_PACKET;

static uint8_t bit_index = 0;
static uint8_t half_phase = 0;
static bool phase_p = true;

static const uint8_t reset_packet_bytes[] = {0xff, 0xf0, 0x00, 0x00, 0x01};
static const uint8_t hard_reset_packet_bytes[] = {0xff, 0xf0, 0x00, 0x04, 0x03};
static const uint8_t idle_packet_bytes[] = {0xff, 0xf7, 0xf8, 0x01, 0xff};
static const uint8_t base_packet_bytes[] = {0xff, 0xf0, 0x19, 0xd0, 0xef};

static const uint16_t legacy_bit1_ticks = 58;   // ~58us @ 1MHz
static const uint16_t legacy_bit0_ticks = 100;  // ~100us @ 1MHz
static const uint16_t legacy_stretched0_ticks = 130;
static const uint16_t legacy_scope_o_ticks[] = {100, 10000, 1000, 10};
static uint8_t legacy_scope_o_index = 0;
static bool legacy_fw_state = true;
static uint8_t legacy_kickstart_cycles = 0;

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
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_A) {
        return (uint8_t)((bit_index & 1U) ? 1U : 0U);
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_B) {
        return (uint8_t)((bit_index & 1U) ? 0U : 1U);
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_O) {
        return 1U;
    }

    size_t size = 0;
    const uint8_t* data = legacy_packet_data(selected_packet_id, &size);
    if ((data == NULL) || (size == 0)) {
        return 1;
    }

    const uint16_t total_bits = (uint16_t)(size * 8U);
    if (total_bits == 0U) {
        return 1;
    }

    if (bit_index >= total_bits) {
        bit_index = 0;
    }

    const uint8_t byte_index = (uint8_t)(bit_index / 8U);
    const uint8_t bit_in_byte = (uint8_t)(7U - (bit_index % 8U));
    return (uint8_t)((data[byte_index] >> bit_in_byte) & 0x01U);
}

static inline uint16_t legacy_get_ticks_for_bit(uint8_t bit)
{
    if ((legacy_wave_mode == LEGACY_MODE_STRETCHED_ZERO) ||
        (legacy_wave_mode == LEGACY_MODE_PACKET_STRETCHED)) {
        return legacy_stretched0_ticks;
    }
    if (legacy_wave_mode == LEGACY_MODE_SCOPE_O) {
        return legacy_scope_o_ticks[legacy_scope_o_index];
    }
    return (bit != 0U) ? legacy_bit1_ticks : legacy_bit0_ticks;
}

static inline void legacy_track_outputs(bool n, bool p)
{
    TR_P_GPIO_Port->BSRR = ((uint32_t)(!n) << TR_N_BR_Pos) |
                           ((uint32_t)(!p) << TR_P_BR_Pos) |
                           ((uint32_t)(n) << TR_N_BS_Pos) |
                           ((uint32_t)(p) << TR_P_BS_Pos);
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

            legacy_track_outputs(!phase_p, phase_p);
            phase_p = !phase_p;

            half_phase++;
            if (half_phase >= 2U) {
                half_phase = 0;
                if ((legacy_wave_mode == LEGACY_MODE_PACKET) ||
                    (legacy_wave_mode == LEGACY_MODE_PACKET_STRETCHED) ||
                    (legacy_wave_mode == LEGACY_MODE_WARBLE)) {
                    size_t size = 0;
                    (void)legacy_packet_data(selected_packet_id, &size);
                    bit_index++;
                    if ((size > 0U) && (bit_index >= (uint16_t)(size * 8U))) {
                        bit_index = 0;
                        if (legacy_wave_mode == LEGACY_MODE_WARBLE) {
                            selected_packet_id = (selected_packet_id == LEGACY_PACKET_RESET) ?
                                                 LEGACY_PACKET_BASE : LEGACY_PACKET_RESET;
                        } else if (legacy_kickstart_cycles > 0U) {
                            legacy_kickstart_cycles--;
                            if (legacy_kickstart_cycles == 0U) {
                                selected_packet_id = LEGACY_PACKET_IDLE;
                            }
                        }
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
    legacy_reserved_timer = 14;
    selected_packet_id = LEGACY_PACKET_IDLE;
    legacy_wave_mode = LEGACY_MODE_PACKET;
    legacy_scope_o_index = 0;
    legacy_fw_state = true;
    legacy_kickstart_cycles = 0;
    bit_index = 0;
    half_phase = 0;
    phase_p = true;
}

bool LegacyMode_Start(void)
{
    if (legacy_mode_running) {
        return false;
    }

    if (legacy_reserved_timer != 14U) {
        return false;
    }

    CommandStation_Stop();

    bit_index = 0;
    half_phase = 0;
    phase_p = true;

    htim14.Instance->CR1 &= ~TIM_CR1_OPM;
    htim14.Instance->CNT = 0U;
    htim14.Instance->ARR = legacy_bit1_ticks;
    __HAL_TIM_CLEAR_FLAG(&htim14, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim14, TIM_IT_UPDATE);

    if (HAL_TIM_Base_Start_IT(&htim14) != HAL_OK) {
        __HAL_TIM_DISABLE_IT(&htim14, TIM_IT_UPDATE);
        return false;
    }

    legacy_mode_running = true;
    return true;
}

bool LegacyMode_Stop(void)
{
    if (!legacy_mode_running) {
        return false;
    }

    legacy_mode_running = false;
    legacy_stop_timer();
    legacy_track_outputs(false, false);
    return true;
}

bool LegacyMode_IsRunning(void)
{
    return legacy_mode_running;
}

bool LegacyMode_SetReservedTimer(uint8_t timer_id)
{
    if (timer_id != 14) {
        return false;
    }

    legacy_reserved_timer = timer_id;
    return true;
}

uint8_t LegacyMode_GetReservedTimer(void)
{
    return legacy_reserved_timer;
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
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'a':
            legacy_wave_mode = LEGACY_MODE_SCOPE_A;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'b':
            legacy_wave_mode = LEGACY_MODE_SCOPE_B;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'o':
            legacy_wave_mode = LEGACY_MODE_SCOPE_O;
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
            selected_packet_id = LEGACY_PACKET_RESET;
            legacy_kickstart_cycles = 0;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case '0':
            legacy_wave_mode = LEGACY_MODE_STREAM_ZERO;
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
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'S':
            legacy_wave_mode = LEGACY_MODE_STRETCHED_ZERO;
            bit_index = 0;
            half_phase = 0;
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'e':
            if (!LegacyMode_SelectPacket(LEGACY_PACKET_HARD)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'f':
            legacy_fw_state = !legacy_fw_state;
            if (!LegacyMode_SelectPacket(legacy_fw_state ? LEGACY_PACKET_BASE : LEGACY_PACKET_IDLE)) {
                return false;
            }
            if (!LegacyMode_IsRunning()) {
                return LegacyMode_Start();
            }
            return true;
        case 'k':
            selected_packet_id = LEGACY_PACKET_BASE;
            legacy_wave_mode = LEGACY_MODE_PACKET;
            legacy_kickstart_cycles = 16U;
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
