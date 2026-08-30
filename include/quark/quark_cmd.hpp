#pragma once
#include <switch.h>
#include <string>
#include <vector>
#include <usb/cf/cf_CommandFramework.hpp>

namespace quark::cmd {

    using PathType = ::usb::cf::PathType;

    std::vector<std::string> GetFiles(const std::string &dir_path);
    std::vector<std::string> GetDirectories(const std::string &dir_path);

    Result StatPath(const std::string &path, PathType &out_type, size_t &out_file_size);
    u64 GetFileSize(const std::string &path);

    Result OpenFile(const std::string &path);
    Result ReadFile(const std::string &path, u64 offset, u64 size, u64 &out_read, void *buf);
    void CloseFile();

    u32 GetDriveCount();

    struct DriveInfo {
        std::string label;
        std::string path;
        size_t      total_size;
        size_t      free_size;
    };

    Result GetDriveInfo(u32 idx, DriveInfo &out);

    u32 GetSpecialPathCount();

    struct SpecialPath {
        std::string name;
        std::string path;
    };

    Result GetSpecialPath(u32 idx, SpecialPath &out);

    bool IsInstallableFile(const std::string &path);

}
