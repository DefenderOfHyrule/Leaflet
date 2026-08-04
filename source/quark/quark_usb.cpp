#include "quark/quark_usb.hpp"
#include <usb/usb_Base.hpp>

namespace quark::usb {

    Result Initialize() {
        return ::usb::Initialize();
    }

    void Finalize() {
        ::usb::Finalize();
    }

    bool IsConnected() {
        return ::usb::IsStateOk();
    }

    size_t Read(void *buf, size_t size) {
        if (R_FAILED(::usb::Read(buf, size))) return 0;
        return size;
    }

    size_t Write(const void *buf, size_t size) {
        if (R_FAILED(::usb::Write(buf, size))) return 0;
        return size;
    }

}
