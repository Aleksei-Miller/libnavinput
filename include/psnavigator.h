#ifndef PSNAVIGATOR_H
#define PSNAVIGATOR_H

#include <stdint.h>

/*
    PSNavigator API v1

    Lightweight library for working with Sony PS Navigator controller.
    Designed as a low-level hardware layer similar to psmoveapi.

    Features:
    - Device detection and connection
    - Polling input state
    - Buttons / axes access
    - Battery status
    - Disconnect detection

    Notes:
    - No mapping / deadzone / smoothing inside library
    - No auto-reconnect (handled by higher-level code)
    - Current backend supports USB; Bluetooth is reserved for future support
    - Thread-compatible, but not thread-safe:
      each PSNavigator instance must be owned by one thread at a time
    - psnavigatorInit() / psnavigatorShutdown() must be externally serialized
    - psnavigatorDisconnect() must not race with psnavigatorPoll() or psnavigatorGet*()
*/

#ifdef _WIN32
    #ifdef PSNAVIGATOR_BUILD
        #define PSNAV_API __declspec(dllexport)
    #else
        #define PSNAV_API __declspec(dllimport)
    #endif
    #define PSNAV_CALL __cdecl
#else
    #define PSNAV_API
    #define PSNAV_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _PSNavigator PSNavigator;

#define PSNAVIGATOR_API_VERSION 1

/* Result codes returned by API functions */
typedef enum PSNavResult {
    PSNAV_RESULT_OK = 0,
    PSNAV_RESULT_ERROR = -1,
    PSNAV_RESULT_INVALID_ARGUMENT = -2,
    PSNAV_RESULT_NOT_FOUND = -3,
    PSNAV_RESULT_OPEN_FAILED = -4,
    PSNAV_RESULT_INIT_FAILED = -5,
    PSNAV_RESULT_READ_FAILED = -6,
    PSNAV_RESULT_NOT_CONNECTED = -7,
    PSNAV_RESULT_UNSUPPORTED = -8
} PSNavResult;

/* Analog inputs */
typedef enum PSNavAxis {
    PSNAV_AXIS_STICK_X = 0,   /* Left stick X (-128..127 normalized) */
    PSNAV_AXIS_STICK_Y = 1,   /* Left stick Y (-128..127 normalized) */
    PSNAV_AXIS_TRIGGER = 2    /* L2 trigger (0..255) */
} PSNavAxis;

/* Buttons */
typedef enum PSNavButton {
    PSNAV_BUTTON_PS = 0,
    PSNAV_BUTTON_CROSS,
    PSNAV_BUTTON_CIRCLE,
    PSNAV_BUTTON_L1,
    PSNAV_BUTTON_L2,
    PSNAV_BUTTON_L3,
    PSNAV_BUTTON_UP,
    PSNAV_BUTTON_RIGHT,
    PSNAV_BUTTON_DOWN,
    PSNAV_BUTTON_LEFT,
    PSNAV_BUTTON_COUNT
} PSNavButton;

/* Connection type.
   Bluetooth enum value is reserved for future backend support. */
typedef enum PSNavConnectionType {
	PSNAV_CONNECTION_BLUETOOTH = 0,
	PSNAV_CONNECTION_USB = 1,
    PSNAV_CONNECTION_UNKNOWN = 2
} PSNavConnectionType;

/* Optional logging callback */
typedef void (PSNAV_CALL *PSNavLogCallback)(int level, const char *message, void *user_data);

/* ===================== Library ===================== */

/* Initialize library (must be called before use) */
PSNAV_API PSNavResult  PSNAV_CALL psnavigatorInit(int api_version);

/* Shutdown library */
PSNAV_API void         PSNAV_CALL psnavigatorShutdown(void);

/* Get API version */
PSNAV_API int          PSNAV_CALL psnavigatorGetApiVersion(void);

/* Get stable string name for a result code */
PSNAV_API const char  *PSNAV_CALL psnavigatorResultToString(PSNavResult result);

/* Get number of currently openable devices in the current enumeration snapshot */
PSNAV_API int          PSNAV_CALL psnavigatorGetDeviceCount(void);

/* Get device path for index in the current enumeration snapshot.
   Device indices are not stable persistent IDs. */
PSNAV_API PSNavResult  PSNAV_CALL psnavigatorGetDevicePathById(
    int id,
    char *buffer,
    int buffer_size
);

/* ===================== Device ===================== */

/* Connect to device by index in the current enumeration snapshot.
   Device indices are not stable persistent IDs. */
PSNAV_API PSNavigator *PSNAV_CALL psnavigatorConnectById(int id);

/* Connect to device by a previously enumerated device path */
PSNAV_API PSNavigator *PSNAV_CALL psnavigatorConnectByPath(const char *path);

/* Disconnect and free device */
PSNAV_API void         PSNAV_CALL psnavigatorDisconnect(PSNavigator *nav);

/* Check if device is still connected */
PSNAV_API int          PSNAV_CALL psnavigatorIsConnected(PSNavigator *nav);

/* Poll device (must be called regularly) */
PSNAV_API PSNavResult  PSNAV_CALL psnavigatorPoll(PSNavigator *nav);

/* ===================== Device Info ===================== */

/* Get device path (WinUSB path) */
PSNAV_API const char  *PSNAV_CALL psnavigatorGetDevicePath(PSNavigator *nav);

/* Get connection type.
   Current implementation reports USB; Bluetooth is reserved for future support. */
PSNAV_API int          PSNAV_CALL psnavigatorGetConnectionType(PSNavigator *nav);

/* ===================== Input ===================== */

/* Get normalized axis value */
PSNAV_API int          PSNAV_CALL psnavigatorGetAxis(PSNavigator *nav, PSNavAxis axis);

/* Get raw axis value */
PSNAV_API int          PSNAV_CALL psnavigatorGetAxisRaw(PSNavigator *nav, PSNavAxis axis);

/* Get button state (0/1) */
PSNAV_API int          PSNAV_CALL psnavigatorGetButton(PSNavigator *nav, PSNavButton button);

/* Get full button bitmask */
PSNAV_API uint32_t     PSNAV_CALL psnavigatorGetButtons(PSNavigator *nav);

/* Buttons pressed this frame */
PSNAV_API uint32_t     PSNAV_CALL psnavigatorGetButtonsPressed(PSNavigator *nav);

/* Buttons released this frame */
PSNAV_API uint32_t     PSNAV_CALL psnavigatorGetButtonsReleased(PSNavigator *nav);

/* Helper: button pressed this frame */
PSNAV_API int          PSNAV_CALL psnavigatorWasPressed(PSNavigator *nav, PSNavButton button);

/* Helper: button released this frame */
PSNAV_API int          PSNAV_CALL psnavigatorWasReleased(PSNavigator *nav, PSNavButton button);

/* ===================== Power ===================== */

/* Battery percent (or -1 if USB) */
PSNAV_API int          PSNAV_CALL psnavigatorGetBattery(PSNavigator *nav);

/* Raw battery value */
PSNAV_API int          PSNAV_CALL psnavigatorGetBatteryRaw(PSNavigator *nav);

/* ===================== Diagnostics ===================== */

/* Get last error string.
   Passing NULL returns a thread-local library-level error buffer for APIs that
   fail before a PSNavigator instance exists. */
PSNAV_API const char  *PSNAV_CALL psnavigatorGetLastError(PSNavigator *nav);

/* Set log callback */
PSNAV_API void         PSNAV_CALL psnavigatorSetLogCallback(
    PSNavigator *nav,
    PSNavLogCallback callback,
    void *user_data
);

#ifdef __cplusplus
}
#endif

#endif
