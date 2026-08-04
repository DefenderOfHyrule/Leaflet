#include "quark/quark_cmd.hpp"
#include <algorithm>
#include <cctype>
#include <usb/cf/cf_CommandFramework.hpp>
#include <fs/fs_Explorer.hpp>

namespace quark::cmd {

    std::vector<std::string> GetFiles(const std::string &dir_path) {
        std::vector<std::string> files;
        u32 count = 0;
        if (R_FAILED(::usb::cf::GetFileCount(dir_path, count))) return files;
        files.reserve(count);
        for (u32 i = 0; i < count; i++) {
            std::string name;
            if (R_SUCCEEDED(::usb::cf::GetFile(dir_path, i, name)))
                files.push_back(std::move(name));
        }
        return files;
    }

    std::vector<std::string> GetDirectories(const std::string &dir_path) {
        std::vector<std::string> dirs;
        u32 count = 0;
        if (R_FAILED(::usb::cf::GetDirectoryCount(dir_path, count))) return dirs;
        dirs.reserve(count);
        for (u32 i = 0; i < count; i++) {
            std::string name;
            if (R_SUCCEEDED(::usb::cf::GetDirectory(dir_path, i, name)))
                dirs.push_back(std::move(name));
        }
        return dirs;
    }

    Result StatPath(const std::string &path, PathType &out_type, size_t &out_file_size) {
        return ::usb::cf::StatPath(path, out_type, out_file_size);
    }

    u64 GetFileSize(const std::string &path) {
        PathType type;
        size_t sz = 0;
        if (R_SUCCEEDED(::usb::cf::StatPath(path, type, sz)) && type == PathType::File)
            return static_cast<u64>(sz);
        return 0;
    }

    Result OpenFile(const std::string &path) {
        return ::usb::cf::StartFile(path, ::fs::FileMode::Read);
    }

    Result ReadFile(const std::string &path, u64 offset, u64 size, u64 &out_read, void *buf) {
        return ::usb::cf::ReadFile(path, offset, size, out_read, buf);
    }

    void CloseFile() {
        ::usb::cf::EndFile(::fs::FileMode::Read);
    }

    u32 GetDriveCount() {
        u32 count = 0;
        ::usb::cf::GetDriveCount(count);
        return count;
    }

    Result GetDriveInfo(u32 idx, DriveInfo &out) {
        return ::usb::cf::GetDriveInfo(idx, out.label, out.path, out.total_size, out.free_size);
    }

    u32 GetSpecialPathCount() {
        u32 count = 0;
        ::usb::cf::GetSpecialPathCount(count);
        return count;
    }

    Result GetSpecialPath(u32 idx, SpecialPath &out) {
        return ::usb::cf::GetSpecialPath(idx, out.name, out.path);
    }

    bool IsInstallableFile(const std::string &path) {
        if (path.size() < 4) return false;
        std::string ext = path.substr(path.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
        return (ext == ".nsp") || (ext == ".xci");
    }

}
