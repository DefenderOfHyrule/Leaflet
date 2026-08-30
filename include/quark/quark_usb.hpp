#pragma once
#include <switch.h>
#include <string>
#include <usb/usb_Base.hpp>

namespace quark::usb {

    Result Initialize();
    void Finalize();
    bool IsConnected();
    size_t Read(void *buf, size_t size);
    size_t Write(const void *buf, size_t size);

}
