#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "psnavigator_internal.h"

typedef struct TestContext {
    uint8_t report[PSNAV_INPUT_REPORT_SIZE];
    uint32_t transferred;
    int read_ok;
    const char *device_paths[8];
    int device_count;
    int open_should_fail;
    const char *last_opened_path;
} TestContext;

static TestContext g_test_context;

static int g_failures = 0;

static void resetTestContext(void)
{
    memset(&g_test_context, 0, sizeof(g_test_context));
    g_test_context.transferred = PSNAV_INPUT_REPORT_SIZE;
    g_test_context.read_ok = 1;
}

static void expectInt(const char *name, int expected, int actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s expected=%d actual=%d\n", name, expected, actual);
        ++g_failures;
    }
}

static void expectUInt32(const char *name, uint32_t expected, uint32_t actual)
{
    if (expected != actual) {
        fprintf(stderr, "FAIL: %s expected=0x%08X actual=0x%08X\n",
            name,
            (unsigned int)expected,
            (unsigned int)actual);
        ++g_failures;
    }
}

static void prepareConnectedNavigator(PSNavigator *nav)
{
    memset(nav, 0, sizeof(*nav));
    nav->device_handle = (HANDLE)1;
    nav->winusb_handle = (WINUSB_INTERFACE_HANDLE)1;
    nav->connected = 1;
    nav->connection_type = PSNAV_CONNECTION_USB;
}

static void testBatteryMapping(void)
{
    expectInt("battery(-1)", -1, psnavComputeBatteryPercent(-1));
    expectInt("battery(0)", 0, psnavComputeBatteryPercent(0));
    expectInt("battery(1)", 25, psnavComputeBatteryPercent(1));
    expectInt("battery(3)", 75, psnavComputeBatteryPercent(3));
    expectInt("battery(5)", 100, psnavComputeBatteryPercent(5));
    expectInt("battery(9)", 100, psnavComputeBatteryPercent(9));
}

static void testResultToString(void)
{
    expectInt("result string ok", 0, strcmp(psnavigatorResultToString(PSNAV_RESULT_OK), "OK"));
    expectInt("result string not connected", 0, strcmp(psnavigatorResultToString(PSNAV_RESULT_NOT_CONNECTED), "NOT_CONNECTED"));
    expectInt("result string unknown", 0, strcmp(psnavigatorResultToString((PSNavResult)12345), "UNKNOWN_RESULT"));
}

static void testUsbDecode(void)
{
    PSNavigator nav;
    uint8_t report[PSNAV_INPUT_REPORT_SIZE];
    uint32_t expected_buttons;

    memset(&nav, 0, sizeof(nav));
    memset(report, 0, sizeof(report));

    nav.connection_type = PSNAV_CONNECTION_USB;

    report[2] = 0x12;
    report[3] = 0x44;
    report[4] = 0x01;
    report[6] = 200;
    report[7] = 12;
    report[18] = 99;
    report[30] = 3;

    psnavDecodeReport(&nav, report, sizeof(report));

    expected_buttons =
        (1u << PSNAV_BUTTON_PS) |
        (1u << PSNAV_BUTTON_CROSS) |
        (1u << PSNAV_BUTTON_L1) |
        (1u << PSNAV_BUTTON_L3) |
        (1u << PSNAV_BUTTON_UP);

    expectUInt32("usb buttons", expected_buttons, nav.buttons);
    expectInt("usb stick_x_raw", 200, nav.stick_x_raw);
    expectInt("usb stick_y_raw", 12, nav.stick_y_raw);
    expectInt("usb trigger_raw", 99, nav.trigger_raw);
    expectInt("usb stick_x", 72, nav.stick_x);
    expectInt("usb stick_y", -116, nav.stick_y);
    expectInt("usb trigger", 99, nav.trigger);
    expectInt("usb battery_raw", -1, nav.battery_raw);
    expectInt("usb battery_percent", -1, nav.battery_percent);
}

static void testBluetoothDecode(void)
{
    PSNavigator nav;
    uint8_t report[PSNAV_INPUT_REPORT_SIZE];

    memset(&nav, 0, sizeof(nav));
    memset(report, 0, sizeof(report));

    nav.connection_type = PSNAV_CONNECTION_BLUETOOTH;
    report[30] = 3;

    psnavDecodeReport(&nav, report, sizeof(report));

    expectInt("bt battery_raw", 3, nav.battery_raw);
    expectInt("bt battery_percent", 75, nav.battery_percent);
}

static void testButtonTransitionsHelpers(void)
{
    PSNavigator nav;
    uint32_t expected_pressed;
    uint32_t expected_released;

    memset(&nav, 0, sizeof(nav));

    nav.prev_buttons =
        (1u << PSNAV_BUTTON_CROSS) |
        (1u << PSNAV_BUTTON_L1);
    nav.buttons =
        (1u << PSNAV_BUTTON_CROSS) |
        (1u << PSNAV_BUTTON_CIRCLE);

    expected_pressed = (1u << PSNAV_BUTTON_CIRCLE);
    expected_released = (1u << PSNAV_BUTTON_L1);

    expectUInt32("pressed mask", expected_pressed, psnavigatorGetButtonsPressed(&nav));
    expectUInt32("released mask", expected_released, psnavigatorGetButtonsReleased(&nav));
    expectInt("was pressed circle", 1, psnavigatorWasPressed(&nav, PSNAV_BUTTON_CIRCLE));
    expectInt("was pressed cross", 0, psnavigatorWasPressed(&nav, PSNAV_BUTTON_CROSS));
    expectInt("was released l1", 1, psnavigatorWasReleased(&nav, PSNAV_BUTTON_L1));
    expectInt("was released cross", 0, psnavigatorWasReleased(&nav, PSNAV_BUTTON_CROSS));
}

static void testPollDecodesAndTracksTransitions(void)
{
    PSNavigator nav;
    PSNavResult result;

    memset(&nav, 0, sizeof(nav));
    prepareConnectedNavigator(&nav);
    resetTestContext();

    g_test_context.report[3] = 0x40;
    g_test_context.report[6] = 129;
    g_test_context.report[7] = 127;
    g_test_context.report[18] = 7;

    result = psnavigatorPoll(&nav);
    expectInt("poll #1 result", PSNAV_RESULT_OK, result);
    expectUInt32("poll #1 buttons",
        (1u << PSNAV_BUTTON_CROSS),
        psnavigatorGetButtons(&nav));
    expectUInt32("poll #1 pressed",
        (1u << PSNAV_BUTTON_CROSS),
        psnavigatorGetButtonsPressed(&nav));
    expectUInt32("poll #1 released", 0, psnavigatorGetButtonsReleased(&nav));
    expectInt("poll #1 axis x", 1, psnavigatorGetAxis(&nav, PSNAV_AXIS_STICK_X));
    expectInt("poll #1 axis y", -1, psnavigatorGetAxis(&nav, PSNAV_AXIS_STICK_Y));
    expectInt("poll #1 trigger", 7, psnavigatorGetAxis(&nav, PSNAV_AXIS_TRIGGER));

    resetTestContext();
    g_test_context.report[3] = 0x20;

    result = psnavigatorPoll(&nav);
    expectInt("poll #2 result", PSNAV_RESULT_OK, result);
    expectUInt32("poll #2 buttons",
        (1u << PSNAV_BUTTON_CIRCLE),
        psnavigatorGetButtons(&nav));
    expectUInt32("poll #2 pressed",
        (1u << PSNAV_BUTTON_CIRCLE),
        psnavigatorGetButtonsPressed(&nav));
    expectUInt32("poll #2 released",
        (1u << PSNAV_BUTTON_CROSS),
        psnavigatorGetButtonsReleased(&nav));
}

static void testPollReadFailureDisconnects(void)
{
    PSNavigator nav;
    PSNavResult result;

    memset(&nav, 0, sizeof(nav));
    prepareConnectedNavigator(&nav);
    nav.buttons = (1u << PSNAV_BUTTON_CROSS);
    nav.prev_buttons = (1u << PSNAV_BUTTON_L1);
    nav.stick_x_raw = 200;
    nav.stick_y_raw = 50;
    nav.trigger_raw = 12;
    nav.stick_x = 72;
    nav.stick_y = -78;
    nav.trigger = 12;
    nav.battery_raw = 3;
    nav.battery_percent = 75;

    resetTestContext();
    g_test_context.read_ok = 0;

    result = psnavigatorPoll(&nav);

    expectInt("poll read failure result", PSNAV_RESULT_NOT_CONNECTED, result);
    expectInt("poll read failure connected", 0, nav.connected);
    expectInt("poll read failure isConnected", 0, psnavigatorIsConnected(&nav));
    expectUInt32("poll read failure buttons", 0, nav.buttons);
    expectUInt32("poll read failure prev_buttons", 0, nav.prev_buttons);
    expectInt("poll read failure stick_x_raw", 128, nav.stick_x_raw);
    expectInt("poll read failure stick_y_raw", 128, nav.stick_y_raw);
    expectInt("poll read failure trigger_raw", 0, nav.trigger_raw);
    expectInt("poll read failure battery_raw", -1, nav.battery_raw);
    expectInt("poll read failure battery_percent", -1, nav.battery_percent);
    expectInt("poll read failure has_last_report", 0, nav.has_last_report);
}

static void testPollShortReportDisconnects(void)
{
    PSNavigator nav;
    PSNavResult result;

    memset(&nav, 0, sizeof(nav));
    prepareConnectedNavigator(&nav);
    resetTestContext();
    g_test_context.transferred = 8;

    result = psnavigatorPoll(&nav);

    expectInt("poll short report result", PSNAV_RESULT_READ_FAILED, result);
    expectInt("poll short report connected", 0, nav.connected);
    expectInt("poll short report isConnected", 0, psnavigatorIsConnected(&nav));
    expectUInt32("poll short report buttons", 0, nav.buttons);
    expectUInt32("poll short report prev_buttons", 0, nav.prev_buttons);
    expectInt("poll short report stick_x", 0, nav.stick_x);
    expectInt("poll short report stick_y", 0, nav.stick_y);
    expectInt("poll short report trigger", 0, nav.trigger);
    expectInt("poll short report has_last_report", 0, nav.has_last_report);
}

static void testPollOnDisconnectedNavigator(void)
{
    PSNavigator nav;
    PSNavResult result;

    memset(&nav, 0, sizeof(nav));
    nav.device_handle = INVALID_HANDLE_VALUE;
    nav.winusb_handle = NULL;
    nav.connected = 0;

    result = psnavigatorPoll(&nav);

    expectInt("poll disconnected result", PSNAV_RESULT_NOT_CONNECTED, result);
    expectInt("poll disconnected connected", 0, nav.connected);
    expectUInt32("poll disconnected buttons", 0, nav.buttons);
    expectInt("poll disconnected stick_x_raw", 128, nav.stick_x_raw);
    expectInt("poll disconnected battery_raw", -1, nav.battery_raw);
}

static void testDeviceEnumerationApi(void)
{
    char path[256];
    PSNavResult result;

    resetTestContext();
    g_test_context.device_paths[0] = "\\\\?\\usb#vid_054c&pid_042f#nav0";
    g_test_context.device_paths[1] = "\\\\?\\usb#vid_054c&pid_042f#nav1";
    g_test_context.device_count = 2;

    expectInt("device count", 2, psnavigatorGetDeviceCount());

    result = psnavigatorGetDevicePathById(0, path, (int)sizeof(path));
    expectInt("path by id 0 result", PSNAV_RESULT_OK, result);
    expectInt("path by id 0 cmp", 0, strcmp(path, g_test_context.device_paths[0]));

    result = psnavigatorGetDevicePathById(1, path, (int)sizeof(path));
    expectInt("path by id 1 result", PSNAV_RESULT_OK, result);
    expectInt("path by id 1 cmp", 0, strcmp(path, g_test_context.device_paths[1]));

    result = psnavigatorGetDevicePathById(2, path, (int)sizeof(path));
    expectInt("path by id 2 result", PSNAV_RESULT_NOT_FOUND, result);
    expectInt("path by id 2 empty", 0, path[0]);
}

static void testConnectByIdAndPath(void)
{
    PSNavigator *nav;

    resetTestContext();
    g_test_context.device_paths[0] = "\\\\?\\usb#vid_054c&pid_042f#nav0";
    g_test_context.device_paths[1] = "\\\\?\\usb#vid_054c&pid_042f#nav1";
    g_test_context.device_count = 2;

    nav = psnavigatorConnectById(1);
    expectInt("connect by id non-null", 1, nav != NULL);
    expectInt("connect by id path cmp", 0, strcmp(g_test_context.last_opened_path, g_test_context.device_paths[1]));
    if (nav) {
        expectInt("connect by id connected", 1, psnavigatorIsConnected(nav));
        psnavigatorDisconnect(nav);
    }

    nav = psnavigatorConnectByPath(g_test_context.device_paths[0]);
    expectInt("connect by path non-null", 1, nav != NULL);
    expectInt("connect by path path cmp", 0, strcmp(g_test_context.last_opened_path, g_test_context.device_paths[0]));
    if (nav) {
        expectInt("connect by path connected", 1, psnavigatorIsConnected(nav));
        psnavigatorDisconnect(nav);
    }
}

static void testConnectFailuresForInvalidEnumerationInputs(void)
{
    char path[32];
    PSNavigator *nav;
    PSNavResult result;

    resetTestContext();
    g_test_context.device_paths[0] = "\\\\?\\usb#vid_054c&pid_042f#nav0";
    g_test_context.device_count = 1;

    result = psnavigatorGetDevicePathById(-1, path, (int)sizeof(path));
    expectInt("path by invalid id", PSNAV_RESULT_INVALID_ARGUMENT, result);

    result = psnavigatorGetDevicePathById(0, NULL, 0);
    expectInt("path invalid buffer", PSNAV_RESULT_INVALID_ARGUMENT, result);

    nav = psnavigatorConnectById(5);
    expectInt("connect missing id", 1, nav == NULL);

    g_test_context.open_should_fail = 1;
    nav = psnavigatorConnectByPath(g_test_context.device_paths[0]);
    expectInt("connect failing path", 1, nav == NULL);
}

int psnavBackendGetDeviceCount(void)
{
    return g_test_context.device_count;
}

int psnavBackendGetDevicePathByIndex(int index, char *path, size_t path_size)
{
    if (index < 0 || !path || path_size == 0) {
        return 0;
    }

    if (index >= g_test_context.device_count || !g_test_context.device_paths[index]) {
        path[0] = '\0';
        return 0;
    }

    strncpy(path, g_test_context.device_paths[index], path_size - 1);
    path[path_size - 1] = '\0';
    return 1;
}

int psnavBackendFindDevicePath(char *path, size_t path_size)
{
    if (!path || path_size == 0) {
        return 0;
    }

    path[0] = '\0';
    return 0;
}

int psnavBackendOpenDevice(PSNavigator *nav, const char *path)
{
    if (!nav || !path || !path[0] || g_test_context.open_should_fail) {
        return 0;
    }

    nav->device_handle = (HANDLE)1;
    nav->winusb_handle = (WINUSB_INTERFACE_HANDLE)1;
    nav->connected = 1;
    strncpy(nav->device_path, path, sizeof(nav->device_path) - 1);
    nav->device_path[sizeof(nav->device_path) - 1] = '\0';
    g_test_context.last_opened_path = path;
    return 1;
}

int psnavBackendOpenDeviceByIndex(PSNavigator *nav, int index)
{
    (void)nav;
    (void)index;
    return 0;
}

void psnavBackendCloseDevice(PSNavigator *nav)
{
    if (!nav) {
        return;
    }

    nav->winusb_handle = NULL;
    nav->device_handle = INVALID_HANDLE_VALUE;
    nav->connected = 0;
}

int psnavBackendSendMagic(PSNavigator *nav)
{
    (void)nav;
    return 1;
}

int psnavBackendReadReport(PSNavigator *nav, uint8_t *buffer, uint32_t *transferred)
{
    (void)nav;

    if (!buffer || !transferred || !g_test_context.read_ok) {
        return 0;
    }

    memcpy(buffer, g_test_context.report, sizeof(g_test_context.report));
    *transferred = g_test_context.transferred;
    return 1;
}

int main(void)
{
    PSNavResult result;

    result = psnavigatorInit(PSNAVIGATOR_API_VERSION);
    expectInt("init", PSNAV_RESULT_OK, result);

    testResultToString();
    testBatteryMapping();
    testUsbDecode();
    testBluetoothDecode();
    testButtonTransitionsHelpers();
    testPollDecodesAndTracksTransitions();
    testPollReadFailureDisconnects();
    testPollShortReportDisconnects();
    testPollOnDisconnectedNavigator();
    testDeviceEnumerationApi();
    testConnectByIdAndPath();
    testConnectFailuresForInvalidEnumerationInputs();

    psnavigatorShutdown();

    if (g_failures != 0) {
        fprintf(stderr, "Tests failed: %d\n", g_failures);
        return 1;
    }

    printf("All tests passed.\n");
    return 0;
}
