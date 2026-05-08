# libnavinput

`libnavinput` is a small C library for working with the Sony PS Navigation controller on Windows.

The library is designed as a low-level hardware layer, similar in spirit to `psmoveapi`.

## Features

- Device detection
- Connect / disconnect
- Polling input state
- Button state access
- Button pressed / released helpers
- Analog stick and trigger access
- Battery reporting
- Disconnect detection
- WinUSB backend

## Scope

`libnavinput` only talks to the controller.

It does **not** implement:

- Input mapping
- Deadzones
- Smoothing
- Emulation
- Auto-reconnect
- Profile logic

Those should stay in higher-level application code.

## Current status

- Windows only
- USB backend is implemented
- Bluetooth-facing API surface is reserved for future backend support
- Battery over USB is reported as `-1` / unknown

Bluetooth-related enums and internal paths are intentionally kept as forward-compatibility hooks for future support (for example via external driver stacks), but the current library implementation should be treated as USB-only.

## Public API

Main functions:

- `psnavigatorInit`
- `psnavigatorShutdown`
- `psnavigatorGetApiVersion`
- `psnavigatorResultToString`
- `psnavigatorGetDeviceCount`
- `psnavigatorGetDevicePathById`

Device functions:

- `psnavigatorConnectById`
- `psnavigatorConnectByPath`
- `psnavigatorDisconnect`
- `psnavigatorIsConnected`
- `psnavigatorPoll`

Device info:

- `psnavigatorGetDevicePath`
- `psnavigatorGetConnectionType`

Input:

- `psnavigatorGetAxis`
- `psnavigatorGetAxisRaw`
- `psnavigatorGetButton`
- `psnavigatorGetButtons`
- `psnavigatorGetButtonsPressed`
- `psnavigatorGetButtonsReleased`
- `psnavigatorWasPressed`
- `psnavigatorWasReleased`

Power / diagnostics:

- `psnavigatorGetBattery`
- `psnavigatorGetBatteryRaw`
- `psnavigatorGetLastError`
- `psnavigatorSetLogCallback`

## Using the library

Typical flow:

1. Call `psnavigatorInit`
2. Call `psnavigatorGetDeviceCount`
3. Choose a device index in the range `0 .. count - 1`
4. Optionally call `psnavigatorGetDevicePathById(index, ...)` if you want to keep a path-based identifier
5. Call `psnavigatorConnectById(index)` or `psnavigatorConnectByPath(path)`
6. In a loop:
   - Call `psnavigatorPoll`
   - Read buttons / axes / battery
7. Call `psnavigatorDisconnect`
8. Call `psnavigatorShutdown`

## Simple example

```c
#include <stdio.h>
#include "psnavigator.h"

int main(void)
{
    PSNavigator *nav;
    PSNavResult result;

    result = psnavigatorInit(PSNAVIGATOR_API_VERSION);
    if (result != PSNAV_RESULT_OK) {
        printf("init failed: %s\n", psnavigatorResultToString(result));
        return 1;
    }

    nav = psnavigatorConnectById(0);
    if (!nav) {
        printf("connect failed: %s\n", psnavigatorGetLastError(NULL));
        psnavigatorShutdown();
        return 1;
    }

    while (psnavigatorIsConnected(nav)) {
        result = psnavigatorPoll(nav);
        if (result != PSNAV_RESULT_OK) {
            printf("poll failed: %s | %s\n",
                psnavigatorResultToString(result),
                psnavigatorGetLastError(nav));
            break;
        }

        printf("x=%d y=%d tr=%d\n",
            psnavigatorGetAxis(nav, PSNAV_AXIS_STICK_X),
            psnavigatorGetAxis(nav, PSNAV_AXIS_STICK_Y),
            psnavigatorGetAxis(nav, PSNAV_AXIS_TRIGGER));
    }

    psnavigatorDisconnect(nav);
    psnavigatorShutdown();
    return 0;
}
```

`psnavigatorResultToString(...)` returns a stable short string for a `PSNavResult` value and is intended for logging alongside `psnavigatorGetLastError(...)`.

## Notes

- The library detects disconnect, but does not auto-reconnect
- Reconnect should be implemented in higher-level code
- WinUSB driver setup is required for USB access
- Current implementation should be treated as USB-only
- Bluetooth-related API values are reserved for future support
- `psnavigatorGetDeviceCount`, `psnavigatorGetDevicePathById`, and `psnavigatorConnectById` operate on the current enumeration snapshot
- Device indices are not stable persistent IDs and may change after reconnect, driver rebind, or re-enumeration
- Device paths are the preferred identifier when you need to reconnect to the same physical device later
- For TCC builds, `winusb_tcc.h` is used as a compatibility header

## Threading

`libnavinput` is thread-compatible, but not thread-safe.

- Call `psnavigatorInit()` and `psnavigatorShutdown()` under external synchronization.
- Treat each `PSNavigator*` as single-owner: one device instance should be used by one thread at a time.
- Do not call `psnavigatorDisconnect()` concurrently with `psnavigatorPoll()` or any `psnavigatorGet*()` call on the same instance.
- Different `PSNavigator*` instances may be used from different threads if each instance has a single owner.
- `psnavigatorGetLastError(NULL)` uses thread-local storage for library-level errors that occur before a device instance exists.

## Build

### CMake

Build with plain CMake commands.

#### GCC / MinGW

```bat
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
```

Output:

- `build\bin\libnavinput.dll`

#### TinyCC

```bat
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=tcc
cmake --build build
```

Output:

- `build\bin\libnavinput.dll`

#### MSVC

```bat
cmake -S . -B build -G "Visual Studio 16 2019" -A Win32
cmake --build build --config Release
```

Output:

- `build\bin\Release\libnavinput.dll`

Notes:

- `GCC` build requires `cmake`, `gcc`, and `dlltool`-compatible MinGW environment in `PATH`
- `TCC` build requires `cmake` and `tcc` in `PATH`
- `MSVC` build requires Visual Studio Build Tools with the C++ toolchain and a Developer Command Prompt or equivalent environment
- Before switching to another compiler or generator, remove `build\` or reconfigure it from scratch

## Trademark notice

This software is an independent, open-source project and is **not** affiliated with, authorized, maintained, sponsored, or endorsed by Sony Interactive Entertainment Inc., Microsoft Corporation, or any of their affiliates.

"PlayStation", "PS Move", "PS Navigation", "DUALSHOCK" and related marks are registered trademarks of Sony Interactive Entertainment Inc. All Sony controller names, images, and references in this repository are used strictly for **nominative purposes** — only to identify hardware compatibility and provide instructions to the user.


All other trademarks, logos, and brands are the property of their respective owners. The use of these names, logos, and brands does not imply endorsement.

## License

libnavinput is open-source software licensed under the **GNU General Public License v3.0 (GPLv3)**. 

You are free to use, modify, and distribute this software under the terms of this license. For more information, please see the `LICENSE` file in the repository.

## Credits

- Developed via ChatGPT (Prompt-based development).
