#include "psnavigator_internal.h"

typedef struct PSNavButtonMapping {
    PSNavButton button;
    uint8_t byte_index;
    uint8_t bit_mask;
} PSNavButtonMapping;

enum {
    PSNAV_REPORT_BUTTON_GROUP_0 = 2,
    PSNAV_REPORT_BUTTON_GROUP_1 = 3,
    PSNAV_REPORT_BUTTON_GROUP_2 = 4,
    PSNAV_REPORT_STICK_X = 6,
    PSNAV_REPORT_STICK_Y = 7,
    PSNAV_REPORT_TRIGGER = 18,
    PSNAV_REPORT_BATTERY = 30
};

static const PSNavButtonMapping g_button_mappings[] = {
    { PSNAV_BUTTON_PS,     PSNAV_REPORT_BUTTON_GROUP_2, 0x01 },
    { PSNAV_BUTTON_CROSS,  PSNAV_REPORT_BUTTON_GROUP_1, 0x40 },
    { PSNAV_BUTTON_CIRCLE, PSNAV_REPORT_BUTTON_GROUP_1, 0x20 },
    { PSNAV_BUTTON_L1,     PSNAV_REPORT_BUTTON_GROUP_1, 0x04 },
    { PSNAV_BUTTON_L2,     PSNAV_REPORT_BUTTON_GROUP_1, 0x01 },
    { PSNAV_BUTTON_L3,     PSNAV_REPORT_BUTTON_GROUP_0, 0x02 },
    { PSNAV_BUTTON_UP,     PSNAV_REPORT_BUTTON_GROUP_0, 0x10 },
    { PSNAV_BUTTON_RIGHT,  PSNAV_REPORT_BUTTON_GROUP_0, 0x20 },
    { PSNAV_BUTTON_DOWN,   PSNAV_REPORT_BUTTON_GROUP_0, 0x40 },
    { PSNAV_BUTTON_LEFT,   PSNAV_REPORT_BUTTON_GROUP_0, 0x80 }
};

static uint32_t psnavMakeButtonMask(const uint8_t *buffer)
{
    uint32_t buttons = 0;
    size_t i;

    for (i = 0; i < sizeof(g_button_mappings) / sizeof(g_button_mappings[0]); ++i) {
        const PSNavButtonMapping *mapping = &g_button_mappings[i];

        if (buffer[mapping->byte_index] & mapping->bit_mask) {
            buttons |= (1u << (uint32_t)mapping->button);
        }
    }

    return buttons;
}

int psnavComputeBatteryPercent(int battery_raw)
{
    if (battery_raw < 0) {
        return -1;
    }

    if (battery_raw <= 0) return 0;
    if (battery_raw >= 5) return 100;
    return battery_raw * 25;
}

void psnavDecodeReport(PSNavigator *nav, const uint8_t *buffer, uint32_t size)
{
    if (!nav || !buffer || size < PSNAV_INPUT_REPORT_SIZE) {
        return;
    }

    nav->buttons = psnavMakeButtonMask(buffer);

    nav->stick_x_raw = (int)buffer[PSNAV_REPORT_STICK_X];
    nav->stick_y_raw = (int)buffer[PSNAV_REPORT_STICK_Y];
    nav->trigger_raw = (int)buffer[PSNAV_REPORT_TRIGGER];

    nav->stick_x = nav->stick_x_raw - 128;
    nav->stick_y = nav->stick_y_raw - 128;
    nav->trigger = nav->trigger_raw;

    if (nav->connection_type == PSNAV_CONNECTION_USB) {
        nav->battery_raw = -1;
        nav->battery_percent = -1;
    } else {
        nav->battery_raw = (int)buffer[PSNAV_REPORT_BATTERY];
        nav->battery_percent = psnavComputeBatteryPercent(nav->battery_raw);
    }
}
