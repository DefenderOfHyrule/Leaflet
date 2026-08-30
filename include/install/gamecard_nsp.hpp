#pragma once

#include "install/nsp.hpp"
#include <string>
#include <vector>

namespace leaf::install::nsp
{
    // wraps a mounted gamecard directory as a virtual NSP.
    // builds a synthetic PFS0 header so the existing install pipeline works.
    class GamecardNSP : public NSP
    {
    public:
        GamecardNSP(const std::string& mountPath);
        ~GamecardNSP();

        void StreamToPlaceholder(std::shared_ptr<nx::ncm::ContentStorage>& contentStorage, NcmContentId ncaId) override;
        void BufferData(void* buf, off_t offset, size_t size) override;

        void RetrieveHeader() override;

    private:
        struct FileInfo {
            std::string name;
            std::string fullPath;
            u64 fileSize;
            u64 virtualOffset; // offset within the virtual data region
        };

        std::string m_mountPath;
        std::vector<FileInfo> m_files;
        u64 m_totalDataSize = 0;

        void readFromFiles(void* buf, u64 virtualOffset, u64 size);
    };
}
