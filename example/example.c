#include <stdio.h>
#include <stdint.h>
#include <windows.h>
#include <conio.h>

#include "psnavigator.h"

static void sleepMs(unsigned int ms)
{
    Sleep(ms);
}

static const char *connectionTypeToString(int connection_type)
{
    switch (connection_type) {
        case PSNAV_CONNECTION_USB:
            return "USB";
        case PSNAV_CONNECTION_BLUETOOTH:
            return "Bluetooth";
        default:
            return "Unknown";
    }
}

static void appendButtonName(char *buffer, size_t buffer_size, int *first, const char *name)
{
    size_t len;

    if (!buffer || !first || !name || buffer_size == 0) {
        return;
    }

    len = strlen(buffer);

    if (len >= buffer_size - 1) {
        return;
    }

    if (!*first) {
        _snprintf(buffer + len, buffer_size - len, " %s", name);
    } else {
        _snprintf(buffer + len, buffer_size - len, "%s", name);
        *first = 0;
    }

    buffer[buffer_size - 1] = '\0';
}

static void buildButtonsText(uint32_t buttons, char *buffer, size_t buffer_size)
{
    int first = 1;

    if (!buffer || buffer_size == 0) {
        return;
    }

    buffer[0] = '\0';

    if (buttons & (1u << PSNAV_BUTTON_PS)) {
        appendButtonName(buffer, buffer_size, &first, "PS");
    }
    if (buttons & (1u << PSNAV_BUTTON_CROSS)) {
        appendButtonName(buffer, buffer_size, &first, "CROSS");
    }
    if (buttons & (1u << PSNAV_BUTTON_CIRCLE)) {
        appendButtonName(buffer, buffer_size, &first, "CIRCLE");
    }
    if (buttons & (1u << PSNAV_BUTTON_L1)) {
        appendButtonName(buffer, buffer_size, &first, "L1");
    }
    if (buttons & (1u << PSNAV_BUTTON_L2)) {
        appendButtonName(buffer, buffer_size, &first, "L2");
    }
    if (buttons & (1u << PSNAV_BUTTON_L3)) {
        appendButtonName(buffer, buffer_size, &first, "L3");
    }
    if (buttons & (1u << PSNAV_BUTTON_UP)) {
        appendButtonName(buffer, buffer_size, &first, "UP");
    }
    if (buttons & (1u << PSNAV_BUTTON_RIGHT)) {
        appendButtonName(buffer, buffer_size, &first, "RIGHT");
    }
    if (buttons & (1u << PSNAV_BUTTON_DOWN)) {
        appendButtonName(buffer, buffer_size, &first, "DOWN");
    }
    if (buttons & (1u << PSNAV_BUTTON_LEFT)) {
        appendButtonName(buffer, buffer_size, &first, "LEFT");
    }

    if (first) {
        _snprintf(buffer, buffer_size, "none");
        buffer[buffer_size - 1] = '\0';
    }
}

int main(void)
{
    PSNavigator *nav = NULL;
    PSNavResult result;

    result = psnavigatorInit(PSNAVIGATOR_API_VERSION);
    if (result != PSNAV_RESULT_OK) {
        printf("psnavigatorInit failed: %s\n", psnavigatorResultToString(result));
        return 1;
    }

    printf("API version: %d\n", psnavigatorGetApiVersion());
    printf("Device count: %d\n", psnavigatorGetDeviceCount());

    nav = psnavigatorConnectById(0);
    if (!nav) {
        printf("psnavigatorConnectById failed: %s\n", psnavigatorGetLastError(NULL));
        psnavigatorShutdown();
        return 1;
    }

    printf("Connected: yes\n");
    printf("Connection type: %s\n", connectionTypeToString(psnavigatorGetConnectionType(nav)));
    printf("Device path: %s\n", psnavigatorGetDevicePath(nav));
    printf("Press ESC to quit.\n\n");

    while (psnavigatorIsConnected(nav)) {
        int stick_x;
        int stick_y;
        int trigger;
        int battery;
        int battery_raw;
        uint32_t buttons;
        uint32_t pressed;
        uint32_t released;
        char buttons_text[256];

        result = psnavigatorPoll(nav);
        if (result != PSNAV_RESULT_OK) {
            printf("\nPoll failed: %s | %s\n",
                psnavigatorResultToString(result),
                psnavigatorGetLastError(nav));
            break;
        }

        stick_x = psnavigatorGetAxis(nav, PSNAV_AXIS_STICK_X);
        stick_y = psnavigatorGetAxis(nav, PSNAV_AXIS_STICK_Y);
        trigger = psnavigatorGetAxis(nav, PSNAV_AXIS_TRIGGER);

        battery = psnavigatorGetBattery(nav);
        battery_raw = psnavigatorGetBatteryRaw(nav);

        buttons = psnavigatorGetButtons(nav);
        pressed = psnavigatorGetButtonsPressed(nav);
        released = psnavigatorGetButtonsReleased(nav);

        buildButtonsText(buttons, buttons_text, sizeof(buttons_text));

        printf("\rX=%4d Y=%4d TR=%3d  BTN=%-60s", stick_x, stick_y, trigger, buttons_text);

        if (battery < 0 || battery_raw < 0) {
            printf(" BAT=USB   ");
        } else {
            printf(" BAT=%3d%% ", battery);
        }

        fflush(stdout);

        if (pressed != 0 || released != 0) {
            printf("\npressed=0x%08X released=0x%08X\n",
                (unsigned int)pressed,
                (unsigned int)released);
        }

        if (_kbhit()) {
            int ch = _getch();
            if (ch == 27) {
                printf("\nESC pressed, exiting...\n");
                break;
            }
        }

        sleepMs(5);
    }

    if (nav) {
        psnavigatorDisconnect(nav);
        nav = NULL;
    }

    psnavigatorShutdown();
    return 0;
}
