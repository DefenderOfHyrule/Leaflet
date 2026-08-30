#pragma once

#include <switch.h>
#include <cstdint>
#include <string>
#include <vector>

namespace inst::util {

    struct InstalledContentPiece {
        NcmContentMetaKey key;
        NcmStorageId storageId;
        std::uint64_t sizeBytes;
        std::string label;
        bool isBase;
    };

    std::vector<InstalledContentPiece> GetInstalledContentForTitle(std::uint64_t baseAppId);

    bool DeleteInstalledContentPiece(std::uint64_t baseAppId, const InstalledContentPiece& piece);

    bool DeleteTitleCompletely(std::uint64_t baseAppId);

    bool IsCurrentTakeoverTarget(std::uint64_t baseAppId);

}
