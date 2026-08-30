#pragma once

#include <switch.h>
#include <cstdint>
#include <vector>

namespace inst::util {

    std::uint64_t GetPatchIdForApp(std::uint64_t baseAppId);

    std::uint32_t GetInstalledContentVersion(std::uint64_t titleId, NcmContentMetaType type);

    void SetAvmLaunchFloor(std::uint64_t baseAppId, std::uint32_t version);

    void FixAvmFloorForTitle(std::uint64_t baseAppId);

    std::vector<std::uint64_t> ListDigitallyInstalledAppIds();

    int FixAllAvmFloors();

}
