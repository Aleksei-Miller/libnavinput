#ifndef WINUSB_TCC_H
#define WINUSB_TCC_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef PVOID WINUSB_INTERFACE_HANDLE;
typedef WINUSB_INTERFACE_HANDLE *PWINUSB_INTERFACE_HANDLE;

#pragma pack(push, 1)
typedef struct _WINUSB_SETUP_PACKET {
    UCHAR RequestType;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Length;
} WINUSB_SETUP_PACKET, *PWINUSB_SETUP_PACKET;
#pragma pack(pop)

BOOL __stdcall WinUsb_Initialize(
    HANDLE DeviceHandle,
    PWINUSB_INTERFACE_HANDLE InterfaceHandle
);

BOOL __stdcall WinUsb_Free(
    WINUSB_INTERFACE_HANDLE InterfaceHandle
);

BOOL __stdcall WinUsb_ReadPipe(
    WINUSB_INTERFACE_HANDLE InterfaceHandle,
    UCHAR PipeID,
    PUCHAR Buffer,
    ULONG BufferLength,
    PULONG LengthTransferred,
    LPOVERLAPPED Overlapped
);

BOOL __stdcall WinUsb_ControlTransfer(
    WINUSB_INTERFACE_HANDLE InterfaceHandle,
    WINUSB_SETUP_PACKET SetupPacket,
    PUCHAR Buffer,
    ULONG BufferLength,
    PULONG LengthTransferred,
    LPOVERLAPPED Overlapped
);

#ifdef __cplusplus
}
#endif

#endif
