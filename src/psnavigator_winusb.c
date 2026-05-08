#include "psnavigator_internal.h"

#include <stdlib.h>
#include <string.h>

static const GUID g_usb_device_guid = {
    0xA5DCBF10, 0x6530, 0x11D2,
    { 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED }
};

#define PSNAV_VENDOR_ID_STR  "vid_054c"
#define PSNAV_PRODUCT_ID_STR "pid_042f"

static char asciiToLowerChar(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }

    return ch;
}

static int containsAsciiNoCase(const char *text, const char *needle)
{
    size_t text_len;
    size_t needle_len;
    size_t i;
    size_t j;

    if (!text || !needle) {
        return 0;
    }

    text_len = strlen(text);
    needle_len = strlen(needle);

    if (needle_len == 0 || needle_len > text_len) {
        return 0;
    }

    for (i = 0; i + needle_len <= text_len; ++i) {
        int matched = 1;

        for (j = 0; j < needle_len; ++j) {
            if (asciiToLowerChar(text[i + j]) != asciiToLowerChar(needle[j])) {
                matched = 0;
                break;
            }
        }

        if (matched) {
            return 1;
        }
    }

    return 0;
}

static void setSystemError(PSNavigator *nav, const char *prefix, const char *detail)
{
    DWORD error_code;
    char message_buffer[512];
    DWORD message_len;

    if (!nav || !prefix) {
        return;
    }

    error_code = GetLastError();
    message_buffer[0] = '\0';

    message_len = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message_buffer,
        (DWORD)sizeof(message_buffer),
        NULL
    );

    if (message_len > 0) {
        while (message_len > 0 &&
               (message_buffer[message_len - 1] == '\r' ||
                message_buffer[message_len - 1] == '\n')) {
            message_buffer[--message_len] = '\0';
        }
    }

    if (detail && detail[0]) {
        if (message_len > 0) {
            psnavSetError(
                nav,
                "%s: %s: %s (code=%lu)",
                prefix,
                detail,
                message_buffer,
                (unsigned long)error_code
            );
        } else {
            psnavSetError(
                nav,
                "%s: %s (code=%lu)",
                prefix,
                detail,
                (unsigned long)error_code
            );
        }
    } else {
        if (message_len > 0) {
            psnavSetError(
                nav,
                "%s: %s (code=%lu)",
                prefix,
                message_buffer,
                (unsigned long)error_code
            );
        } else {
            psnavSetError(
                nav,
                "%s: code=%lu",
                prefix,
                (unsigned long)error_code
            );
        }
    }
}

static int isMatchingNavigatorPath(const char *path)
{
    if (!path) {
        return 0;
    }

    return containsAsciiNoCase(path, PSNAV_VENDOR_ID_STR) &&
           containsAsciiNoCase(path, PSNAV_PRODUCT_ID_STR);
}

static int canOpenWinUsbPath(const char *path)
{
    HANDLE device_handle;
    WINUSB_INTERFACE_HANDLE winusb_handle;
    int ok;

    if (!path) {
        return 0;
    }

    device_handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (device_handle == INVALID_HANDLE_VALUE) {
        return 0;
    }

    winusb_handle = NULL;
    ok = WinUsb_Initialize(device_handle, &winusb_handle) ? 1 : 0;

    if (winusb_handle) {
        WinUsb_Free(winusb_handle);
    }

    CloseHandle(device_handle);
    return ok;
}

int psnavBackendGetDeviceCount(void)
{
    HDEVINFO device_info_set;
    SP_DEVICE_INTERFACE_DATA interface_data;
    DWORD index;
    int count;

    device_info_set = SetupDiGetClassDevsA(
        &g_usb_device_guid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (device_info_set == INVALID_HANDLE_VALUE) {
        return 0;
    }

    interface_data.cbSize = sizeof(interface_data);
    index = 0;
    count = 0;

    while (SetupDiEnumDeviceInterfaces(
        device_info_set,
        NULL,
        &g_usb_device_guid,
        index,
        &interface_data
    )) {
        DWORD required_length;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detail_data;

        ++index;
        required_length = 0;

        SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            NULL,
            0,
            &required_length,
            NULL
        );

        if (required_length < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A)) {
            continue;
        }

        detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required_length);
        if (!detail_data) {
            break;
        }

        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            detail_data,
            required_length,
            NULL,
            NULL
        ) &&
            isMatchingNavigatorPath(detail_data->DevicePath) &&
            canOpenWinUsbPath(detail_data->DevicePath)) {
            ++count;
        }

        free(detail_data);
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return count;
}

int psnavBackendGetDevicePathByIndex(int index, char *path, size_t path_size)
{
    HDEVINFO device_info_set;
    SP_DEVICE_INTERFACE_DATA interface_data;
    DWORD interface_index;
    int candidate_index;

    if (index < 0 || !path || path_size == 0) {
        return 0;
    }

    path[0] = '\0';
    device_info_set = SetupDiGetClassDevsA(
        &g_usb_device_guid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (device_info_set == INVALID_HANDLE_VALUE) {
        return 0;
    }

    interface_data.cbSize = sizeof(interface_data);
    interface_index = 0;
    candidate_index = 0;

    while (SetupDiEnumDeviceInterfaces(
        device_info_set,
        NULL,
        &g_usb_device_guid,
        interface_index,
        &interface_data
    )) {
        DWORD required_length;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detail_data;

        ++interface_index;
        required_length = 0;

        SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            NULL,
            0,
            &required_length,
            NULL
        );

        if (required_length < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A)) {
            continue;
        }

        detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required_length);
        if (!detail_data) {
            break;
        }

        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            detail_data,
            required_length,
            NULL,
            NULL
        ) &&
            isMatchingNavigatorPath(detail_data->DevicePath) &&
            canOpenWinUsbPath(detail_data->DevicePath)) {
            if (candidate_index == index) {
                strncpy(path, detail_data->DevicePath, path_size - 1);
                path[path_size - 1] = '\0';
                free(detail_data);
                SetupDiDestroyDeviceInfoList(device_info_set);
                return 1;
            }

            ++candidate_index;
        }

        free(detail_data);
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return 0;
}

int psnavBackendFindDevicePath(char *path, size_t path_size)
{
    HDEVINFO device_info_set;
    SP_DEVICE_INTERFACE_DATA interface_data;
    DWORD index;

    if (!path || path_size == 0) {
        return 0;
    }

    path[0] = '\0';

    device_info_set = SetupDiGetClassDevsA(
        &g_usb_device_guid,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (device_info_set == INVALID_HANDLE_VALUE) {
        return 0;
    }

    interface_data.cbSize = sizeof(interface_data);
    index = 0;

    while (SetupDiEnumDeviceInterfaces(
        device_info_set,
        NULL,
        &g_usb_device_guid,
        index,
        &interface_data
    )) {
        DWORD required_length;
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A detail_data;

        ++index;
        required_length = 0;

        SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            NULL,
            0,
            &required_length,
            NULL
        );

        if (required_length < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A)) {
            continue;
        }

        detail_data = (PSP_DEVICE_INTERFACE_DETAIL_DATA_A)malloc(required_length);
        if (!detail_data) {
            break;
        }

        detail_data->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        if (SetupDiGetDeviceInterfaceDetailA(
            device_info_set,
            &interface_data,
            detail_data,
            required_length,
            NULL,
            NULL
        )) {
            if (isMatchingNavigatorPath(detail_data->DevicePath)) {
                strncpy(path, detail_data->DevicePath, path_size - 1);
                path[path_size - 1] = '\0';

                free(detail_data);
                SetupDiDestroyDeviceInfoList(device_info_set);
                return 1;
            }
        }

        free(detail_data);
    }

    SetupDiDestroyDeviceInfoList(device_info_set);
    return 0;
}

int psnavBackendOpenDevice(PSNavigator *nav, const char *path)
{
    if (!nav || !path) {
        return 0;
    }

    nav->device_handle = CreateFileA(
        path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (nav->device_handle == INVALID_HANDLE_VALUE) {
        setSystemError(nav, "CreateFileA failed", path);
        return 0;
    }

    if (!WinUsb_Initialize(nav->device_handle, &nav->winusb_handle)) {
        setSystemError(nav, "WinUsb_Initialize failed", path);
        CloseHandle(nav->device_handle);
        nav->device_handle = INVALID_HANDLE_VALUE;
        return 0;
    }

    strncpy(nav->device_path, path, sizeof(nav->device_path) - 1);
    nav->device_path[sizeof(nav->device_path) - 1] = '\0';

    nav->connected = 1;
    nav->connection_type = PSNAV_CONNECTION_USB;

    psnavLog(nav, 0, "Navigator opened: %s", nav->device_path);
    return 1;
}

int psnavBackendOpenDeviceByIndex(PSNavigator *nav, int index)
{
    char path[MAX_PATH * 4];

    if (!nav || index < 0) {
        return 0;
    }

    if (!psnavBackendGetDevicePathByIndex(index, path, sizeof(path))) {
        psnavSetError(nav, "PS Navigator with id %d not found", index);
        return 0;
    }

    return psnavBackendOpenDevice(nav, path);
}

void psnavBackendCloseDevice(PSNavigator *nav)
{
    if (!nav) {
        return;
    }

    if (nav->winusb_handle) {
        WinUsb_Free(nav->winusb_handle);
        nav->winusb_handle = NULL;
    }

    if (nav->device_handle != NULL && nav->device_handle != INVALID_HANDLE_VALUE) {
        CloseHandle(nav->device_handle);
        nav->device_handle = INVALID_HANDLE_VALUE;
    }

    nav->connected = 0;
    nav->has_last_report = 0;
    nav->device_path[0] = '\0';

    memset(nav->last_report, 0, sizeof(nav->last_report));
}

int psnavBackendSendMagic(PSNavigator *nav)
{
    UCHAR magic[4];
    WINUSB_SETUP_PACKET setup_packet;
    ULONG transferred;

    if (!nav || !nav->winusb_handle) {
        return 0;
    }

    magic[0] = 0x42;
    magic[1] = 0x0C;
    magic[2] = 0x00;
    magic[3] = 0x00;

    setup_packet.RequestType = 0x21;
    setup_packet.Request = 0x09;
    setup_packet.Value = 0x03F4;
    setup_packet.Index = 0x0000;
    setup_packet.Length = (USHORT)sizeof(magic);

    transferred = 0;

    if (!WinUsb_ControlTransfer(
        nav->winusb_handle,
        setup_packet,
        magic,
        (ULONG)sizeof(magic),
        &transferred,
        NULL
    )) {
        setSystemError(nav, "WinUsb_ControlTransfer failed", "navigator magic packet");
        return 0;
    }

    if (transferred != sizeof(magic)) {
        psnavSetError(
            nav,
            "WinUsb_ControlTransfer short write: got %lu expected %u",
            (unsigned long)transferred,
            (unsigned int)sizeof(magic)
        );
        return 0;
    }

    return 1;
}

int psnavBackendReadReport(PSNavigator *nav, uint8_t *buffer, uint32_t *transferred)
{
    ULONG bytes_read;

    if (!nav || !buffer || !transferred || !nav->winusb_handle) {
        return 0;
    }

    bytes_read = 0;
    *transferred = 0;

    if (!WinUsb_ReadPipe(
        nav->winusb_handle,
        PSNAV_INPUT_PIPE_ID,
        buffer,
        (ULONG)PSNAV_INPUT_REPORT_SIZE,
        &bytes_read,
        NULL
    )) {
        setSystemError(nav, "WinUsb_ReadPipe failed", "navigator input pipe");
        return 0;
    }

    *transferred = (uint32_t)bytes_read;
    return 1;
}
