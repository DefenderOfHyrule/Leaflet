#include "quark/quark_net_cmd.hpp"
#include "quark/quark_net.hpp"
#include <cstring>
#include <new>

namespace quark::net::cmd {

    namespace {

        struct Block {
            alignas(0x1000) uint8_t buf[kBlockSize];
            size_t pos = 0;

            void reset() { pos = 0; memset(buf, 0, kBlockSize); }

            bool writeU32(uint32_t v) {
                if (pos + 4 > kBlockSize) return false;
                memcpy(buf + pos, &v, 4); pos += 4; return true;
            }
            bool writeU64(uint64_t v) {
                if (pos + 8 > kBlockSize) return false;
                memcpy(buf + pos, &v, 8); pos += 8; return true;
            }
            bool writeStr(const std::string& s) {
                uint32_t len = static_cast<uint32_t>(s.size());
                if (!writeU32(len)) return false;
                if (pos + len > kBlockSize) return false;
                memcpy(buf + pos, s.data(), len); pos += len; return true;
            }
            bool readU32(uint32_t& v) {
                if (pos + 4 > kBlockSize) return false;
                memcpy(&v, buf + pos, 4); pos += 4; return true;
            }
            bool readU64(uint64_t& v) {
                if (pos + 8 > kBlockSize) return false;
                memcpy(&v, buf + pos, 8); pos += 8; return true;
            }
            bool readStr(std::string& s) {
                uint32_t len = 0;
                if (!readU32(len)) return false;
                if (pos + len > kBlockSize) return false;
                s.assign(reinterpret_cast<const char*>(buf + pos), len);
                pos += len; return true;
            }
        };

        bool sendBlock(Block& b) { return quark::net::Write(b.buf, kBlockSize); }
        bool recvBlock(Block& b) { b.reset(); return quark::net::Read(b.buf, kBlockSize); }

        bool checkResponse(Block& resp) {
            uint32_t magic = 0, rc = 0;
            return resp.readU32(magic) && magic == kOutputMagic && resp.readU32(rc) && rc == 0;
        }

        bool sendCommand(uint32_t cmdId, Block& req) {
            req.reset();
            req.writeU32(kInputMagic);
            req.writeU32(cmdId);
            return true;
        }

    }

    bool SendBlock(const void* buf) { return quark::net::Write(buf, kBlockSize); }
    bool RecvBlock(void* buf) { return quark::net::Read(buf, kBlockSize); }
    bool SendRaw(const void* buf, size_t size) { return quark::net::Write(buf, size); }
    bool RecvRaw(void* buf, size_t size) { return quark::net::Read(buf, size); }

    bool GetDriveCount(uint32_t& out) {
        Block req; sendCommand(1, req);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readU32(out);
    }

    bool GetDriveInfo(uint32_t idx, std::string& out_label, std::string& out_path, uint64_t& out_total, uint64_t& out_free) {
        Block req; sendCommand(2, req);
        req.writeU32(idx);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readStr(out_label) && resp.readStr(out_path) && resp.readU64(out_total) && resp.readU64(out_free);
    }

    bool StatPath(const std::string& path, uint32_t& out_type, uint64_t& out_size) {
        Block req; sendCommand(3, req);
        req.writeStr(path);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readU32(out_type) && resp.readU64(out_size);
    }

    bool GetFileCount(const std::string& dir, uint32_t& out) {
        Block req; sendCommand(4, req);
        req.writeStr(dir);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readU32(out);
    }

    bool GetFile(const std::string& dir, uint32_t idx, std::string& out) {
        Block req; sendCommand(5, req);
        req.writeStr(dir); req.writeU32(idx);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readStr(out);
    }

    bool GetDirectoryCount(const std::string& dir, uint32_t& out) {
        Block req; sendCommand(6, req);
        req.writeStr(dir);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readU32(out);
    }

    bool GetDirectory(const std::string& dir, uint32_t idx, std::string& out) {
        Block req; sendCommand(7, req);
        req.writeStr(dir); req.writeU32(idx);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readStr(out);
    }

    bool StartFile(const std::string& path) {
        Block req; sendCommand(8, req);
        req.writeStr(path);
        req.writeU32(1); // FileModeRead
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return true;
    }

    bool ReadFile(const std::string& path, uint64_t offset, uint64_t size, uint64_t& out_read, void* buf) {
        Block req; sendCommand(9, req);
        req.writeStr(path); req.writeU64(offset); req.writeU64(size);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        if (!resp.readU64(out_read)) return false;
        return quark::net::Read(buf, static_cast<size_t>(out_read));
    }

    bool EndFile() {
        Block req; sendCommand(11, req);
        req.writeU32(1); // FileModeRead
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return true;
    }

    bool GetSpecialPathCount(uint32_t& out) {
        Block req; sendCommand(15, req);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readU32(out);
    }

    bool GetSpecialPath(uint32_t idx, std::string& out_name, std::string& out_path) {
        Block req; sendCommand(16, req);
        req.writeU32(idx);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return resp.readStr(out_name) && resp.readStr(out_path);
    }

    bool AnnounceConsoleId(const std::string& id) {
        Block req; sendCommand(18, req);
        req.writeStr(id);
        if (!sendBlock(req)) return false;
        Block resp; if (!recvBlock(resp) || !checkResponse(resp)) return false;
        return true;
    }

}
