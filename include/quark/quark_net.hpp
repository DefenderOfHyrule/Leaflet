#pragma once
#include <switch.h>
#include <string>

namespace quark::net {

    constexpr int kDefaultPort = 2313;

    bool Connect(const std::string& host, int port = kDefaultPort);
    void CancelConnect();
    void Disconnect();
    bool IsConnected();

    bool Read(void* buf, size_t size);
    bool Write(const void* buf, size_t size);

}
