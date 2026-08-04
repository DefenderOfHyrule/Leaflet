#include "quark/quark_net.hpp"
#include <atomic>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <switch.h>

namespace quark::net {

    namespace {
        int g_sockfd = -1;
        std::atomic<bool> g_cancelConnect{false};
    }

    bool Connect(const std::string& host, int port) {
        g_cancelConnect.store(false, std::memory_order_relaxed);
        if (g_sockfd >= 0) {
            ::close(g_sockfd);
            g_sockfd = -1;
        }

        g_sockfd = ::socket(AF_INET, SOCK_STREAM, 0);
        if (g_sockfd < 0) return false;

        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port   = htons(static_cast<uint16_t>(port));
        if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
            ::close(g_sockfd);
            g_sockfd = -1;
            return false;
        }

        ::fcntl(g_sockfd, F_SETFL, O_NONBLOCK);
        ::connect(g_sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

        constexpr int kTimeoutSec = 5;
        for (int i = 0; i < kTimeoutSec * 10; i++) {
            if (g_cancelConnect.load(std::memory_order_relaxed)) {
                ::close(g_sockfd);
                g_sockfd = -1;
                return false;
            }
            fd_set wfds;
            FD_ZERO(&wfds);
            FD_SET(g_sockfd, &wfds);
            struct timeval tv{ .tv_sec = 0, .tv_usec = 100'000 }; // 100ms
            const int sel = ::select(g_sockfd + 1, nullptr, &wfds, nullptr, &tv);
            if (sel > 0) {
                int err = 0;
                socklen_t len = sizeof(err);
                ::getsockopt(g_sockfd, SOL_SOCKET, SO_ERROR, &err, &len);
                if (err != 0) {
                    ::close(g_sockfd);
                    g_sockfd = -1;
                    return false;
                }
                ::fcntl(g_sockfd, F_SETFL, 0);
                int one = 1;
                ::setsockopt(g_sockfd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
                struct timeval rwtv{ .tv_sec = 10, .tv_usec = 0 };
                ::setsockopt(g_sockfd, SOL_SOCKET, SO_RCVTIMEO, &rwtv, sizeof(rwtv));
                ::setsockopt(g_sockfd, SOL_SOCKET, SO_SNDTIMEO, &rwtv, sizeof(rwtv));
                return true;
            }
        }
        ::close(g_sockfd);
        g_sockfd = -1;
        return false;
    }

    void CancelConnect() {
        g_cancelConnect.store(true, std::memory_order_relaxed);
    }

    void Disconnect() {
        g_cancelConnect.store(false, std::memory_order_relaxed);
        if (g_sockfd >= 0) {
            ::close(g_sockfd);
            g_sockfd = -1;
        }
    }

    bool IsConnected() {
        return g_sockfd >= 0;
    }

    bool Read(void* buf, size_t size) {
        if (g_sockfd < 0) return false;
        size_t received = 0;
        auto* ptr = static_cast<uint8_t*>(buf);
        while (received < size) {
            ssize_t r = ::recv(g_sockfd, ptr + received, size - received, MSG_WAITALL);
            if (r <= 0) {
                Disconnect();
                return false;
            }
            received += static_cast<size_t>(r);
        }
        return true;
    }

    bool Write(const void* buf, size_t size) {
        if (g_sockfd < 0) return false;
        size_t sent = 0;
        const auto* ptr = static_cast<const uint8_t*>(buf);
        while (sent < size) {
            ssize_t w = ::send(g_sockfd, ptr + sent, size - sent, 0);
            if (w <= 0) {
                Disconnect();
                return false;
            }
            sent += static_cast<size_t>(w);
        }
        return true;
    }

}
