#include "psnavigator_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static int g_psnavigator_initialized = 0;
#if defined(_MSC_VER)
__declspec(thread) static char g_last_error[512] = "";
#elif defined(__GNUC__)
static __thread char g_last_error[512] = "";
#else
static char g_last_error[512] = "";
#endif

static void psnavResetInputState(PSNavigator *nav)
{
    if (!nav) {
        return;
    }

    nav->buttons = 0;
    nav->prev_buttons = 0;
    nav->stick_x_raw = 128;
    nav->stick_y_raw = 128;
    nav->trigger_raw = 0;

    nav->stick_x = 0;
    nav->stick_y = 0;
    nav->trigger = 0;

    nav->battery_raw = -1;
    nav->battery_percent = -1;

    nav->has_last_report = 0;
    memset(nav->last_report, 0, sizeof(nav->last_report));
}

static void psnavSetConnectionState(PSNavigator *nav, int connected)
{
    if (!nav) {
        return;
    }

    nav->connected = connected ? 1 : 0;
    psnavResetInputState(nav);
}

static int psnavHasOpenDevice(const PSNavigator *nav)
{
    if (!nav) {
        return 0;
    }

    if (!nav->winusb_handle) {
        return 0;
    }

    if (nav->device_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    return 1;
}

static PSNavigator *psnavFailConnect(PSNavigator *nav, const char *fallback_error)
{
    if (!nav) {
        return NULL;
    }

    if (fallback_error && !nav->last_error[0]) {
        psnavSetError(nav, "%s", fallback_error);
    }

    psnavigatorDisconnect(nav);
    return NULL;
}

static PSNavResult psnavFailPoll(
    PSNavigator *nav,
    PSNavResult result,
    int close_device,
    const char *fallback_error
)
{
    if (!nav) {
        return result;
    }

    if (fallback_error && !nav->last_error[0]) {
        psnavSetError(nav, "%s", fallback_error);
    }

    if (close_device) {
        psnavBackendCloseDevice(nav);
    }

    psnavSetConnectionState(nav, 0);
    return result;
}

static PSNavigator *psnavConnectFromPath(const char *path, const char *fallback_error)
{
    PSNavigator *nav;

    if (!g_psnavigator_initialized) {
        psnavSetError(NULL, "psnavigatorInit must be called before connect");
        return NULL;
    }

    if (!path || !path[0]) {
        psnavSetError(NULL, "Invalid device path");
        return NULL;
    }

    nav = (PSNavigator *)calloc(1, sizeof(*nav));
    if (!nav) {
        return NULL;
    }

    nav->device_handle = INVALID_HANDLE_VALUE;
    nav->connection_type = PSNAV_CONNECTION_USB;
    nav->battery_raw = -1;
    nav->battery_percent = -1;
    nav->last_error[0] = '\0';

    psnavSetConnectionState(nav, 0);

    if (!psnavBackendOpenDevice(nav, path)) {
        return psnavFailConnect(nav, fallback_error);
    }

    if (!psnavBackendSendMagic(nav)) {
        return psnavFailConnect(nav, "Failed to initialize device");
    }

    nav->connected = 1;
    return nav;
}

void psnavSetError(PSNavigator *nav, const char *fmt, ...)
{
    va_list args;
    va_list args_copy;

    if (!fmt) {
        return;
    }

    va_start(args, fmt);
    va_copy(args_copy, args);

    if (nav) {
        vsnprintf(nav->last_error, sizeof(nav->last_error), fmt, args);
        nav->last_error[sizeof(nav->last_error) - 1] = '\0';
    }

    vsnprintf(g_last_error, sizeof(g_last_error), fmt, args_copy);
    g_last_error[sizeof(g_last_error) - 1] = '\0';

    va_end(args_copy);
    va_end(args);

    if (nav) {
        psnavLog(nav, 1, "%s", nav->last_error);
    }
}

void psnavLog(PSNavigator *nav, int level, const char *fmt, ...)
{
    char buffer[512];
    va_list args;

    if (!nav || !fmt || !nav->log_callback) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    buffer[sizeof(buffer) - 1] = '\0';
    nav->log_callback(level, buffer, nav->log_user_data);
}

PSNAV_API PSNavResult PSNAV_CALL psnavigatorInit(int api_version)
{
    if (api_version != PSNAVIGATOR_API_VERSION) {
        return PSNAV_RESULT_UNSUPPORTED;
    }

    g_psnavigator_initialized = 1;
    g_last_error[0] = '\0';
    return PSNAV_RESULT_OK;
}

PSNAV_API void PSNAV_CALL psnavigatorShutdown(void)
{
    g_psnavigator_initialized = 0;
}

PSNAV_API int PSNAV_CALL psnavigatorGetApiVersion(void)
{
    return PSNAVIGATOR_API_VERSION;
}

PSNAV_API const char *PSNAV_CALL psnavigatorResultToString(PSNavResult result)
{
    switch (result) {
        case PSNAV_RESULT_OK:
            return "OK";
        case PSNAV_RESULT_ERROR:
            return "ERROR";
        case PSNAV_RESULT_INVALID_ARGUMENT:
            return "INVALID_ARGUMENT";
        case PSNAV_RESULT_NOT_FOUND:
            return "NOT_FOUND";
        case PSNAV_RESULT_OPEN_FAILED:
            return "OPEN_FAILED";
        case PSNAV_RESULT_INIT_FAILED:
            return "INIT_FAILED";
        case PSNAV_RESULT_READ_FAILED:
            return "READ_FAILED";
        case PSNAV_RESULT_NOT_CONNECTED:
            return "NOT_CONNECTED";
        case PSNAV_RESULT_UNSUPPORTED:
            return "UNSUPPORTED";
        default:
            return "UNKNOWN_RESULT";
    }
}

PSNAV_API int PSNAV_CALL psnavigatorGetDeviceCount(void)
{
    if (!g_psnavigator_initialized) {
        return 0;
    }

    return psnavBackendGetDeviceCount();
}

PSNAV_API PSNavResult PSNAV_CALL psnavigatorGetDevicePathById(
    int id,
    char *buffer,
    int buffer_size
)
{
    if (!g_psnavigator_initialized) {
        psnavSetError(NULL, "psnavigatorInit must be called before device enumeration");
        return PSNAV_RESULT_INIT_FAILED;
    }

    if (id < 0 || !buffer || buffer_size <= 0) {
        psnavSetError(NULL, "Invalid arguments for psnavigatorGetDevicePathById");
        return PSNAV_RESULT_INVALID_ARGUMENT;
    }

    if (!psnavBackendGetDevicePathByIndex(id, buffer, (size_t)buffer_size)) {
        buffer[0] = '\0';
        psnavSetError(
            NULL,
            "PS Navigator with id %d not found in current enumeration snapshot",
            id
        );
        return PSNAV_RESULT_NOT_FOUND;
    }

    return PSNAV_RESULT_OK;
}

PSNAV_API PSNavigator *PSNAV_CALL psnavigatorConnectById(int id)
{
    char path[MAX_PATH * 4];

    if (!g_psnavigator_initialized) {
        psnavSetError(NULL, "psnavigatorInit must be called before connect");
        return NULL;
    }

    if (id < 0) {
        psnavSetError(NULL, "Invalid device id");
        return NULL;
    }

    if (!psnavBackendGetDevicePathByIndex(id, path, sizeof(path))) {
        psnavSetError(
            NULL,
            "PS Navigator with id %d not found in current enumeration snapshot",
            id
        );
        return NULL;
    }

    return psnavConnectFromPath(path, "Failed to open device");
}

PSNAV_API PSNavigator *PSNAV_CALL psnavigatorConnectByPath(const char *path)
{
    return psnavConnectFromPath(path, "Failed to open device");
}

PSNAV_API void PSNAV_CALL psnavigatorDisconnect(PSNavigator *nav)
{
    if (!nav) {
        return;
    }

    psnavBackendCloseDevice(nav);
    free(nav);
}

PSNAV_API int PSNAV_CALL psnavigatorIsConnected(PSNavigator *nav)
{
    if (!nav) {
        return 0;
    }

    return nav->connected && psnavHasOpenDevice(nav);
}

PSNAV_API PSNavResult PSNAV_CALL psnavigatorPoll(PSNavigator *nav)
{
    uint8_t buffer[PSNAV_INPUT_REPORT_SIZE];
    uint32_t transferred = 0;

    if (!nav) {
        return PSNAV_RESULT_INVALID_ARGUMENT;
    }

    if (!psnavigatorIsConnected(nav)) {
        return psnavFailPoll(
            nav,
            PSNAV_RESULT_NOT_CONNECTED,
            0,
            "Device is not connected"
        );
    }

    if (!psnavBackendReadReport(nav, buffer, &transferred)) {
        return psnavFailPoll(
            nav,
            PSNAV_RESULT_NOT_CONNECTED,
            1,
            "Navigator disconnected during read"
        );
    }

    if (transferred < PSNAV_INPUT_REPORT_SIZE) {
        psnavSetError(nav, "Short report: got %u bytes", (unsigned)transferred);
        return psnavFailPoll(nav, PSNAV_RESULT_READ_FAILED, 1, NULL);
    }

    nav->prev_buttons = nav->buttons;

    memcpy(nav->last_report, buffer, PSNAV_INPUT_REPORT_SIZE);
    nav->has_last_report = 1;

    psnavDecodeReport(nav, buffer, transferred);
    return PSNAV_RESULT_OK;
}

PSNAV_API const char *PSNAV_CALL psnavigatorGetDevicePath(PSNavigator *nav)
{
    if (!nav) {
        return "";
    }

    return nav->device_path;
}

PSNAV_API int PSNAV_CALL psnavigatorGetConnectionType(PSNavigator *nav)
{
    if (!nav) {
        return PSNAV_CONNECTION_UNKNOWN;
    }

    return nav->connection_type;
}

PSNAV_API int PSNAV_CALL psnavigatorGetAxis(PSNavigator *nav, PSNavAxis axis)
{
    if (!nav) {
        return 0;
    }

    switch (axis) {
        case PSNAV_AXIS_STICK_X: return nav->stick_x;
        case PSNAV_AXIS_STICK_Y: return nav->stick_y;
        case PSNAV_AXIS_TRIGGER: return nav->trigger;
        default: return 0;
    }
}

PSNAV_API int PSNAV_CALL psnavigatorGetAxisRaw(PSNavigator *nav, PSNavAxis axis)
{
    if (!nav) {
        return 0;
    }

    switch (axis) {
        case PSNAV_AXIS_STICK_X: return nav->stick_x_raw;
        case PSNAV_AXIS_STICK_Y: return nav->stick_y_raw;
        case PSNAV_AXIS_TRIGGER: return nav->trigger_raw;
        default: return 0;
    }
}

PSNAV_API int PSNAV_CALL psnavigatorGetButton(PSNavigator *nav, PSNavButton button)
{
    uint32_t mask;

    if (!nav || button < 0 || button >= PSNAV_BUTTON_COUNT) {
        return 0;
    }

    mask = (1u << (uint32_t)button);
    return (nav->buttons & mask) ? 1 : 0;
}

PSNAV_API uint32_t PSNAV_CALL psnavigatorGetButtons(PSNavigator *nav)
{
    return nav ? nav->buttons : 0;
}

PSNAV_API uint32_t PSNAV_CALL psnavigatorGetButtonsPressed(PSNavigator *nav)
{
    if (!nav) {
        return 0;
    }

    return nav->buttons & ~nav->prev_buttons;
}

PSNAV_API uint32_t PSNAV_CALL psnavigatorGetButtonsReleased(PSNavigator *nav)
{
    if (!nav) {
        return 0;
    }

    return ~nav->buttons & nav->prev_buttons;
}

PSNAV_API int PSNAV_CALL psnavigatorWasPressed(PSNavigator *nav, PSNavButton button)
{
    uint32_t mask;

    if (!nav || button < 0 || button >= PSNAV_BUTTON_COUNT) {
        return 0;
    }

    mask = (1u << (uint32_t)button);
    return (psnavigatorGetButtonsPressed(nav) & mask) ? 1 : 0;
}

PSNAV_API int PSNAV_CALL psnavigatorWasReleased(PSNavigator *nav, PSNavButton button)
{
    uint32_t mask;

    if (!nav || button < 0 || button >= PSNAV_BUTTON_COUNT) {
        return 0;
    }

    mask = (1u << (uint32_t)button);
    return (psnavigatorGetButtonsReleased(nav) & mask) ? 1 : 0;
}

PSNAV_API int PSNAV_CALL psnavigatorGetBattery(PSNavigator *nav)
{
    return nav ? nav->battery_percent : -1;
}

PSNAV_API int PSNAV_CALL psnavigatorGetBatteryRaw(PSNavigator *nav)
{
    return nav ? nav->battery_raw : -1;
}

PSNAV_API const char *PSNAV_CALL psnavigatorGetLastError(PSNavigator *nav)
{
    if (nav) {
        return nav->last_error;
    }

    return g_last_error[0] ? g_last_error : "No error";
}

PSNAV_API void PSNAV_CALL psnavigatorSetLogCallback(
    PSNavigator *nav,
    PSNavLogCallback callback,
    void *user_data
)
{
    if (!nav) {
        return;
    }

    nav->log_callback = callback;
    nav->log_user_data = user_data;

}
