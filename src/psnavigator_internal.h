#ifndef PSNAVIGATOR_INTERNAL_H
#define PSNAVIGATOR_INTERNAL_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <stdint.h>
#include <stddef.h>

#include "psnavigator.h"

#if defined(__TINYC__)
    #include "winusb_tcc.h"
#else
    #include <winusb.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define PSNAV_INPUT_REPORT_SIZE 49
#define PSNAV_INPUT_PIPE_ID 0x81

#define PSNAV_VENDOR_ID  0x054C
#define PSNAV_PRODUCT_ID 0x042F

struct _PSNavigator {
    HANDLE device_handle;
    WINUSB_INTERFACE_HANDLE winusb_handle;

    int connected;
    int connection_type;

    uint8_t last_report[PSNAV_INPUT_REPORT_SIZE];
    int has_last_report;

    uint32_t buttons;
    uint32_t prev_buttons;

    int stick_x_raw;
    int stick_y_raw;
    int trigger_raw;

    int stick_x;
    int stick_y;
    int trigger;

    int battery_raw;
    int battery_percent;

    char device_path[MAX_PATH * 4];
    char last_error[512];

    PSNavLogCallback log_callback;
    void *log_user_data;
};

void psnavSetError(PSNavigator *nav, const char *fmt, ...);
void psnavLog(PSNavigator *nav, int level, const char *fmt, ...);

int psnavBackendGetDeviceCount(void);
int psnavBackendGetDevicePathByIndex(int index, char *path, size_t path_size);
int psnavBackendFindDevicePath(char *path, size_t path_size);
int psnavBackendOpenDevice(PSNavigator *nav, const char *path);
int psnavBackendOpenDeviceByIndex(PSNavigator *nav, int index);
void psnavBackendCloseDevice(PSNavigator *nav);

int psnavBackendSendMagic(PSNavigator *nav);
int psnavBackendReadReport(PSNavigator *nav, uint8_t *buffer, uint32_t *transferred);

void psnavDecodeReport(PSNavigator *nav, const uint8_t *buffer, uint32_t size);
int psnavComputeBatteryPercent(int battery_raw);

#ifdef __cplusplus
}
#endif

#endif
