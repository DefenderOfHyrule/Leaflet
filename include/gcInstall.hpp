#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <switch.h>

namespace inst::gc {

    struct GameCardTitle {
        std::uint64_t titleId;
        std::uint32_t baseVersion;
        std::uint32_t updateVersion;
        std::string   displayVersion;   // human-readable version string from NACP
        bool          hasBase;
        bool          hasUpdate;
        std::uint32_t dlcCount;
        std::uint8_t  keyGeneration;
        std::uint64_t totalSize;
        std::uint64_t baseSize;
        std::uint64_t updateSize;
        std::uint64_t dlcSize;
        std::string   name;
        std::string   author;
        std::vector<std::uint8_t> icon;
    };

    bool Init();
    void Exit();

    bool IsInserted();

    bool Mount();
    void Unmount();
    bool IsMounted();

    // returns the mount path for the gamecard's secure partition.
    std::string GetMountPath();

    // return one GameCardTitle per unique title found on the gamecard.
    std::vector<GameCardTitle> EnumerateTitles();

    void InstallFromGamecard(int storageChoice);

    // install specific content types. contentMask: bit 0=base, 1=update, 2=DLC.
    void InstallSelectedFromGamecard(int storageChoice, uint32_t contentMask);
}
