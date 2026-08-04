#include "quark/quark_id.hpp"
#include <switch.h>
#include <cstring>

namespace quark {

    namespace {

        const char* kAdjectives[] = {
            "Red", "Blue", "Gold", "Swift", "Brave", "Calm", "Dark", "Jade",
            "Iron", "Keen", "Lime", "Neon", "Opal", "Pink", "Rosy", "Sage",
            "Teal", "Volt", "Warm", "Zest", "Bold", "Cool", "Dusk", "Epic",
            "Fast", "Glow", "Hazy", "Icy", "Just", "Kiwi"
        };
        constexpr size_t kAdjectivesCount = sizeof(kAdjectives) / sizeof(kAdjectives[0]);

        const char* kNouns[] = {
            "Switch", "Nova", "Pixel", "Spark", "Drift", "Flame", "Ghost",
            "Haven", "Ivory", "Jewel", "Karma", "Lunar", "Mango", "Nexus",
            "Orbit", "Prism", "Quest", "Radar", "Solar", "Titan", "Unity",
            "Vapor", "Wave", "Xenon", "Yield", "Zenith", "Apex", "Blaze",
            "Comet", "Delta"
        };
        constexpr size_t kNounsCount = sizeof(kNouns) / sizeof(kNouns[0]);

        std::string g_consoleId;

    }

    const std::string& GetConsoleId() {
        return g_consoleId;
    }

    void InitConsoleId() {
        u64 seed = 0;
        if (R_FAILED(smInitialize())) seed = armGetSystemTick();
        else {
            smExit();
            seed = armGetSystemTick() ^ (u64)svcGetSystemTick();
        }

        const size_t adjIdx  = seed % kAdjectivesCount;
        const size_t nounIdx = (seed / kAdjectivesCount) % kNounsCount;

        g_consoleId = std::string(kAdjectives[adjIdx]) + "-" + std::string(kNouns[nounIdx]);
    }

    static std::string g_netConsoleId;

    const std::string& GetNetConsoleId() {
        return g_netConsoleId;
    }

    void InitNetConsoleId() {
        u64 seed = armGetSystemTick() ^ (u64)svcGetSystemTick() ^ 0xDEADBEEFCAFEBABEULL;
        const size_t adjIdx  = seed % kAdjectivesCount;
        const size_t nounIdx = (seed / kAdjectivesCount) % kNounsCount;
        g_netConsoleId = std::string(kAdjectives[adjIdx]) + "-" + std::string(kNouns[nounIdx]);
    }

    void ClearNetConsoleId() {
        g_netConsoleId.clear();
    }

}
