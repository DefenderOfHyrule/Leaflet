#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <switch.h>

namespace inst::gc::direct {

    // one entry per CNMT found on the gamecard one each for base/update/DLC.
    struct GameCardContentEntry {
        NcmContentMetaKey key;
        uint64_t          appId;        // base app ID, shared by all entries
        uint64_t          totalNcaSize;
        uint8_t           keyGen;
    };

    // parse all CNMTs on the gamecard. returns one entry per title, or empty on failure.
    std::vector<GameCardContentEntry> EnumerateContent(const std::string& mountPath);

    // install only the selected content entries. storageId is SD or NAND.
    bool InstallSelectedFromGamecard(const std::string& mountPath, NcmStorageId storageId,
                                     const std::vector<size_t>& selection);

    std::string GetLastError();

    // install all content from the mounted gamecard.
    bool InstallAllFromGamecard(const std::string& mountPath, NcmStorageId storageId);

    // get the base title ID from the gamecard's CNMT data. returns 0 on failure.
    std::uint64_t GetGamecardAppId(const std::string& mountPath);
}
