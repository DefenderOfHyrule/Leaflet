/**
 * @file usb_comms.h
 * @brief USB comms.
 * @author yellows8
 * @author plutoo
 * @copyright libnx Authors
 */
#ifdef __cplusplus
extern "C" {
#endif

#pragma once
#include "switch/types.h"

typedef struct {
    u8 bInterfaceClass;
    u8 bInterfaceSubClass;
    u8 bInterfaceProtocol;
} quark_UsbCommsInterfaceInfo;

/// initializes usbComms with the default number of interfaces (1)
Result quark_usbCommsInitialize(void);

/// initializes usbComms with a specific number of interfaces.
Result quark_usbCommsInitializeEx(u32 num_interfaces, const quark_UsbCommsInterfaceInfo *infos);

/// exits usbComms.
void quark_usbCommsExit(void);

/// sets whether to throw a fatal error in usbComms{Read/Write}* on failure, or just return the transferred size. By default (false) the latter is used.
void quark_usbCommsSetErrorHandling(bool flag);

/// read data with the default interface.
size_t quark_usbCommsRead(void* buffer, size_t size, u64 timeout);

/// write data with the default interface.
size_t quark_usbCommsWrite(const void* buffer, size_t size, u64 timeout);

/// same as usbCommsRead except with the specified interface.
size_t quark_usbCommsReadEx(void* buffer, size_t size, u32 interface, u64 timeout);

/// same as usbCommsWrite except with the specified interface.
size_t quark_usbCommsWriteEx(const void* buffer, size_t size, u32 interface, u64 timeout);

#ifdef __cplusplus
}
#endif