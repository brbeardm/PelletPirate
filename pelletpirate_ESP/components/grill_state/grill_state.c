#include "grill_state.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static grill_state_t s_state;
static SemaphoreHandle_t s_mutex;

static const char *mode_names[] = {
    [GRILL_MODE_OFF]         = "Off",
    [GRILL_MODE_START]       = "Ignite",
    [GRILL_MODE_SMOKE]       = "Smoke",
    [GRILL_MODE_SUPER_SMOKE] = "Super Smoke",
    [GRILL_MODE_COOK]        = "Cook",
    [GRILL_MODE_KEEP_WARM]   = "Keep Warm",
    [GRILL_MODE_SHUTDOWN]    = "Shutdown",
    [GRILL_MODE_REIGNITE]    = "Re-Ignite",
};

void grill_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    memset(&s_state, 0, sizeof(s_state));

    s_state.mode = GRILL_MODE_OFF;
    s_state.grill_temp = 87.0f;   // mock ambient temp
    s_state.grill_target = 225;

    // Mock probe data for UI testing
    s_state.probes[0].enabled = true;
    s_state.probes[0].current_temp = 148.0f;
    s_state.probes[0].target_temp = 203.0f;
    s_state.probes[0].alarm_temp = 165.0f;
    strncpy(s_state.probes[0].food_type, "Brisket", sizeof(s_state.probes[0].food_type));
    strncpy(s_state.probes[0].alarm_type, "Wrap", sizeof(s_state.probes[0].alarm_type));

    s_state.probes[1].enabled = true;
    s_state.probes[1].current_temp = 133.0f;
    s_state.probes[1].target_temp = 203.0f;
    s_state.probes[1].alarm_temp = 165.0f;
    strncpy(s_state.probes[1].food_type, "Pork Butt", sizeof(s_state.probes[1].food_type));
    strncpy(s_state.probes[1].alarm_type, "Wrap", sizeof(s_state.probes[1].alarm_type));

    s_state.probes[2].enabled = true;
    s_state.probes[2].current_temp = 110.0f;
    s_state.probes[2].target_temp = 165.0f;
    s_state.probes[2].alarm_temp = 125.0f;
    strncpy(s_state.probes[2].food_type, "Chicken", sizeof(s_state.probes[2].food_type));
    strncpy(s_state.probes[2].alarm_type, "Beer Me", sizeof(s_state.probes[2].alarm_type));

    s_state.probes[3].enabled = false;
}

grill_state_t *grill_state_get(void)
{
    return &s_state;
}

void grill_state_lock(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
}

void grill_state_unlock(void)
{
    xSemaphoreGive(s_mutex);
}

const char *grill_mode_name(grill_mode_t mode)
{
    if (mode >= 0 && mode < GRILL_MODE_COUNT) {
        return mode_names[mode];
    }
    return "Unknown";
}
