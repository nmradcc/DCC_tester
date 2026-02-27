#include "legacy_mode.h"

static bool legacy_mode_running = false;
static uint8_t legacy_reserved_timer = 14;

void LegacyMode_Init(void)
{
    legacy_mode_running = false;
    legacy_reserved_timer = 14;
}

bool LegacyMode_Start(void)
{
    if (legacy_mode_running) {
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
