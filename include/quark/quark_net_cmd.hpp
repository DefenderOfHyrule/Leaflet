#pragma once
#include <switch.h>
#include <string>
#include <vector>

namespace quark::net::cmd {

    constexpr uint32_t kInputMagic  = 0x49434C47; // GLCI
    constexpr uint32_t kOutputMagic = 0x4F434C47; // GLCO
    constexpr size_t   kBlockSize   = 0x1000;

    struct PathType {
        enum Value : uint32_t { Invalid = 0, File = 1, Directory = 2 };
    };

    // low-level block I/O
    bool SendBlock(const void* buf);
    bool RecvBlock(void* buf);
    bool SendRaw(const void* buf, size_t size);
    bool RecvRaw(void* buf, size_t size);

    // high-level command helpers used by netInstCmd
    bool GetDriveCount(uint32_t& out);
    bool GetDriveInfo(uint32_t idx, std::string& out_label, std::string& out_path, uint64_t& out_total, uint64_t& out_free);
    bool GetSpecialPathCount(uint32_t& out);
    bool GetSpecialPath(uint32_t idx, std::string& out_name, std::string& out_path);
    bool AnnounceConsoleId(const std::string& id);
    bool GetFileCount(const std::string& dir, uint32_t& out);
    bool GetFile(const std::string& dir, uint32_t idx, std::string& out);
    bool GetDirectoryCount(const std::string& dir, uint32_t& out);
    bool GetDirectory(const std::string& dir, uint32_t idx, std::string& out);
    bool StatPath(const std::string& path, uint32_t& out_type, uint64_t& out_size);
    bool StartFile(const std::string& path);
    bool ReadFile(const std::string& path, uint64_t offset, uint64_t size, uint64_t& out_read, void* buf);
    bool EndFile();

}
