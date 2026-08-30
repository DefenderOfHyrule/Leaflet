#include <filesystem>
#include <switch.h>
#include <dirent.h>
#include <vector>
#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <curl/curl.h>
#include <regex>
#include <mutex>
#include <cmath>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "switch.h"
#include "util/util.hpp"
#include "nx/ipc/leaf_ipc.h"
#include "util/config.hpp"
#include "util/curl.hpp"
#include "ui/MainApplication.hpp"
#include "quark/quark_usb.hpp"
#include "util/json.hpp"
#include "nx/usbhdd.h"

namespace inst::util {
    namespace {
        std::mutex gAudioPlaybackMutex;
        Mix_Chunk* gNavigationClickChunk = nullptr;
        std::string gNavigationClickLoadedPath;
        bool gNavigationClickAudioOpen = false;

        bool ensureNavigationClickAudioReadyLocked(const std::string& audioPath) {
            int audio_rate = 22050;
            Uint16 audio_format = AUDIO_S16SYS;
            int audio_channels = 2;
            int audio_buffers = 1024;

            if (!gNavigationClickAudioOpen) {
                if (Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0)
                    return false;
                gNavigationClickAudioOpen = true;
            }

            if (gNavigationClickChunk == nullptr || gNavigationClickLoadedPath != audioPath) {
                if (gNavigationClickChunk != nullptr) {
                    Mix_FreeChunk(gNavigationClickChunk);
                    gNavigationClickChunk = nullptr;
                }
                gNavigationClickChunk = Mix_LoadWAV(audioPath.c_str());
                if (gNavigationClickChunk == nullptr)
                    return false;
                gNavigationClickLoadedPath = audioPath;
            }
            return true;
        }

        std::string resolveNavigationClickPath() {
            std::string audioPath = "romfs:/audio/click.wav";
            const std::string customClickPath = inst::config::appDir + "/click.wav";
            if (std::filesystem::exists(customClickPath))
                audioPath = customClickPath;
            return audioPath;
        }

        std::string NormalizeReleaseNotes(std::string text) {
            if (text.empty())
                return "No changelog available for this release.";

            std::string out;
            out.reserve(text.size());
            bool lastWasNewline = false;
            int consecutiveNewlines = 0;
            for (char c : text) {
                if (c == '\r')
                    continue;
                if (c == '\n') {
                    if (!lastWasNewline) {
                        out.push_back('\n');
                        consecutiveNewlines = 1;
                    } else if (consecutiveNewlines < 2) {
                        out.push_back('\n');
                        consecutiveNewlines++;
                    }
                    lastWasNewline = true;
                    continue;
                }
                out.push_back(c);
                lastWasNewline = false;
                consecutiveNewlines = 0;
            }

            while (!out.empty() && (out.back() == '\n' || out.back() == ' ' || out.back() == '\t'))
                out.pop_back();
            if (out.empty())
                return "No changelog available for this release.";

            static constexpr std::size_t kMaxReleaseNotesLen = 3500;
            if (out.size() > kMaxReleaseNotesLen) {
                out = out.substr(0, kMaxReleaseNotesLen);
                out += "\n\n[Changelog truncated]";
            }
            return out;
        }

        float GradientFactor(float nx, float ny, const std::string& type, int angleDeg) {
            if (type == "radial") {
                const float dx = nx - 0.5f, dy = ny - 0.5f;
                const float dist = std::sqrt(dx * dx + dy * dy) / std::sqrt(0.5f);
                return std::min(dist, 1.0f);
            }
            const float rad = static_cast<float>(angleDeg) * static_cast<float>(M_PI) / 180.0f;
            const float cosT = std::cos(rad), sinT = std::sin(rad);
            const float minRaw = std::min(0.0f, cosT) + std::min(0.0f, sinT);
            const float maxRaw = std::max(0.0f, cosT) + std::max(0.0f, sinT);
            const float range  = maxRaw - minRaw;
            if (range < 0.0001f) return 0.5f;
            const float raw = nx * cosT + ny * sinT;
            return (raw - minRaw) / range;
        }
    }

    void regenerateBackground(bool applyLive) {
        const std::string bgPath = inst::config::appDir + "/background.png";
        auto parseHex = [](const char* hex, Uint8& r, Uint8& g, Uint8& b) {
            unsigned int rv = 0, gv = 0, bv = 0;
            sscanf(hex + 1, "%02x%02x%02x", &rv, &gv, &bv);
            r = static_cast<Uint8>(rv);
            g = static_cast<Uint8>(gv);
            b = static_cast<Uint8>(bv);
        };
        Uint8 rA, gA, bA, rB, gB, bB;
        parseHex(inst::config::gradientColorA.c_str(), rA, gA, bA);
        parseHex(inst::config::gradientColorB.c_str(), rB, gB, bB);
        const std::string gradType  = inst::config::gradientType;
        const int         gradAngle = inst::config::gradientAngle;

        constexpr int W = 1280, H = 720;

        if (applyLive) {
            // generate raw RGB pixels and apply directly to all layouts via
            // setBackgroundRgbImage
            std::vector<Uint8> pixels(W * H * 3);
            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    const float nx = static_cast<float>(x) / (W - 1);
                    const float ny = static_cast<float>(y) / (H - 1);
                    const float t = GradientFactor(nx, ny, gradType, gradAngle);
                    const int base = (y * W + x) * 3;
                    pixels[base + 0] = static_cast<Uint8>(rA + (rB - rA) * t);
                    pixels[base + 1] = static_cast<Uint8>(gA + (gB - gA) * t);
                    pixels[base + 2] = static_cast<Uint8>(bA + (bB - bA) * t);
                }
            }
            auto apply = [&](pu::ui::Layout::Ref lyt) {
                if (lyt) lyt->SetBackgroundRgbImage(pixels.data(), W, H, 3);
            };
            if (inst::ui::mainApp) {
                apply(inst::ui::mainApp->mainPage);
                apply(inst::ui::mainApp->sdinstPage);
                apply(inst::ui::mainApp->usbinstPage);
                apply(inst::ui::mainApp->netinstPage);
                apply(inst::ui::mainApp->hddinstPage);
                apply(inst::ui::mainApp->instpage);
                apply(inst::ui::mainApp->optionspage);
                apply(inst::ui::mainApp->filebrowserPage);
                apply(inst::ui::mainApp->gcinstPage);
            }
            // save to sd so the next launch picks up the new gradient.
            SDL_Surface* surf = SDL_CreateRGBSurface(0, W, H, 24,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0);
            if (surf) {
                SDL_LockSurface(surf);
                auto* px = static_cast<Uint8*>(surf->pixels);
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        const float nx = static_cast<float>(x) / (W - 1);
                        const float ny = static_cast<float>(y) / (H - 1);
                        const float t = GradientFactor(nx, ny, gradType, gradAngle);
                        const int si = (y * surf->pitch) + x * 3;
                        px[si + 0] = static_cast<Uint8>(rA + (rB - rA) * t);
                        px[si + 1] = static_cast<Uint8>(gA + (gB - gA) * t);
                        px[si + 2] = static_cast<Uint8>(bA + (bB - bA) * t);
                    }
                }
                SDL_UnlockSurface(surf);
                IMG_SavePNG(surf, bgPath.c_str());
                SDL_FreeSurface(surf);
            }
        } else {
            SDL_Surface* surf = SDL_CreateRGBSurface(0, W, H, 32,
                0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);
            if (surf) {
                SDL_LockSurface(surf);
                auto* px = static_cast<Uint32*>(surf->pixels);
                for (int y = 0; y < H; y++) {
                    for (int x = 0; x < W; x++) {
                        const float nx = static_cast<float>(x) / (W - 1);
                        const float ny = static_cast<float>(y) / (H - 1);
                        const float t = GradientFactor(nx, ny, gradType, gradAngle);
                        const Uint8 r = static_cast<Uint8>(rA + (rB - rA) * t);
                        const Uint8 g = static_cast<Uint8>(gA + (gB - gA) * t);
                        const Uint8 b = static_cast<Uint8>(bA + (bB - bA) * t);
                        px[y * W + x] = SDL_MapRGBA(surf->format, r, g, b, 0xFF);
                    }
                }
                SDL_UnlockSurface(surf);
                IMG_SavePNG(surf, bgPath.c_str());
                SDL_FreeSurface(surf);
            }
        }
    }

    void initApp () {
        if (!std::filesystem::exists("sdmc:/switch")) std::filesystem::create_directory("sdmc:/switch");
        if (!std::filesystem::exists(inst::config::appDir)) std::filesystem::create_directory(inst::config::appDir);
        inst::config::parseConfig();
        primeNavigationClickAudio();

        const std::string bgPath = inst::config::appDir + "/background.png";
        if (!std::filesystem::exists(bgPath))
            regenerateBackground();

        socketInitializeDefault();
        #ifdef __DEBUG__
            nxlinkStdio();
        #endif
        quark::usb::Initialize();
        nx::hdd::init();
    }

    void deinitApp () {
        nx::hdd::exit();
        socketExit();
        quark::usb::Finalize();
    }

    void initInstallServices() {
        ncmInitialize();
        nsextInitialize();
        avmInitialize();
        esInitialize();
        splCryptoInitialize();
        splInitialize();
    }

    void deinitInstallServices() {
        ncmExit();
        nsextExit();
        avmExit();
        esExit();
        splCryptoExit();
        splExit();
    }

    bool ignoreCaseCompare(const std::string &a, const std::string &b) {
        const auto case_insensitive_less = [](char x, char y) {
            return toupper(static_cast<unsigned char>(x)) < toupper(static_cast<unsigned char>(y));
        };

        return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(), case_insensitive_less);
    }

    bool isMacMetadataName(const std::string &name) {
        static const std::vector<std::string> exactNames = {
            ".DS_Store", ".localized", "__MACOSX", ".Spotlight-V100",
            ".Trashes", ".fseventsd", ".TemporaryItems", ".VolumeIcon.icns", ".apdisk"
        };
        for (const auto &n : exactNames) {
            if (name == n) return true;
        }
        return name.rfind("._", 0) == 0;
    }

    std::vector<std::filesystem::path> getDirectoryFiles(const std::string & dir, const std::vector<std::string> & extensions) {
        std::vector<std::filesystem::path> files;
        DIR* dp = opendir(dir.c_str());
        if (!dp) return files;
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            if (dt->d_type & DT_DIR) continue;
            if (!extensions.empty()) {
                const auto dotPos = name.rfind('.');
                std::string ext = (dotPos != std::string::npos) ? name.substr(dotPos) : "";
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (std::find(extensions.begin(), extensions.end(), ext) == extensions.end()) continue;
            }
            const std::string childStr = dir + (dir.back() == '/' ? "" : "/") + name;
            files.push_back(std::filesystem::path(childStr));
        }
        closedir(dp);
        std::sort(files.begin(), files.end(), ignoreCaseCompare);
        return files;
    }

    std::vector<std::filesystem::path> getDirsAtPath(const std::string & dir) {
        std::vector<std::filesystem::path> files;
        DIR* dp = opendir(dir.c_str());
        if (!dp) return files;
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            if (dt->d_type & DT_DIR) {
                const std::string childStr = dir + (dir.back() == '/' ? "" : "/") + name;
                files.push_back(std::filesystem::path(childStr));
            }
        }
        closedir(dp);
        std::sort(files.begin(), files.end(), ignoreCaseCompare);
        return files;
    }

    bool setArchiveBit(const std::string& path) {
        if (path.rfind("sdmc:", 0) != 0) {
            return false;
        }
        const Result rc = fsdevSetConcatenationFileAttribute(path.c_str());
        if (R_SUCCEEDED(rc)) {
            fsdevCommitDevice("sdmc");
        }
        return R_SUCCEEDED(rc);
    }

    bool removeDirectory(std::string dir) {
        DIR* dp = opendir(dir.c_str());
        if (!dp) return false;
        while (true) {
            struct dirent* dt = readdir(dp);
            if (!dt) break;
            const std::string name = dt->d_name;
            if (name == "." || name == "..") continue;
            const std::string child = dir + "/" + name;
            if (dt->d_type & DT_DIR) {
                removeDirectory(child);
            } else {
                std::remove(child.c_str());
            }
        }
        closedir(dp);
        rmdir(dir.c_str());
        return true;
    }

    bool copyFile(std::string inFile, std::string outFile) {
       char ch;
       std::ifstream f1(inFile);
       std::ofstream f2(outFile);

       if(!f1 || !f2) return false;
       
       while(f1 && f1.get(ch)) f2.put(ch);
       return true;
    }

    std::string formatUrlString(std::string ourString) {
        std::stringstream ourStream(ourString);
        std::string segment;
        std::vector<std::string> seglist;

        while(std::getline(ourStream, segment, '/')) {
            seglist.push_back(segment);
        }

        CURL *curl = curl_easy_init();
        int outlength;
        std::string finalString = curl_easy_unescape(curl, seglist[seglist.size() - 1].c_str(), seglist[seglist.size() - 1].length(), &outlength);
        curl_easy_cleanup(curl);

        return finalString;
    }

    std::string formatUrlLink(std::string ourString){
        std::string::size_type pos = ourString.find('/');
        if (pos != std::string::npos)
            return ourString.substr(0, pos);
        else
            return ourString;
    }

    std::string formatString(const std::string& fmt, const std::string& arg) {
        std::string result = fmt;
        const auto pos = result.find("{}");
        if (pos != std::string::npos) result.replace(pos, 2, arg);
        return result;
    }

    std::string shortenString(std::string ourString, int ourLength, bool isFile) {
        std::filesystem::path ourStringAsAPath = ourString;
        std::string ourExtension = ourStringAsAPath.extension().string();
        if (ourString.size() - ourExtension.size() > (unsigned long)ourLength) {
            if(isFile) return (std::string)ourString.substr(0,ourLength) + "(...)" + ourExtension;
            else return (std::string)ourString.substr(0,ourLength) + "...";
        } else return ourString;
    }

    std::string readTextFromFile(std::string ourFile) {
        if (std::filesystem::exists(ourFile)) {
            FILE * file = fopen(ourFile.c_str(), "r");
            char line[1024];
            fgets(line, 1024, file);
            std::string url = line;
            fflush(file);
            fclose(file);
            return url;
        }
        return "";
    }

    std::string softwareKeyboard(std::string guideText, std::string initialText, int LenMax) {
        Result rc=0;
        SwkbdConfig kbd;
        char tmpoutstr[LenMax + 1] = {0};
        rc = swkbdCreate(&kbd, 0);
        if (R_SUCCEEDED(rc)) {
            swkbdConfigMakePresetDefault(&kbd);
            swkbdConfigSetGuideText(&kbd, guideText.c_str());
            swkbdConfigSetInitialText(&kbd, initialText.c_str());
            swkbdConfigSetStringLenMax(&kbd, LenMax);
            rc = swkbdShow(&kbd, tmpoutstr, sizeof(tmpoutstr));
            swkbdClose(&kbd);
            if (R_SUCCEEDED(rc) && tmpoutstr[0] != 0) return(((std::string)(tmpoutstr)));
        }
        return "";
    }

    std::string getIPAddress() {
        struct in_addr addr = {(in_addr_t) gethostid()};
        return inet_ntoa(addr);
    }
    
    bool usbIsConnected() {
        return quark::usb::IsConnected();
    }

    void primeNavigationClickAudio() {
        std::lock_guard<std::mutex> lock(gAudioPlaybackMutex);
        const std::string audioPath = resolveNavigationClickPath();
        ensureNavigationClickAudioReadyLocked(audioPath);
    }

    void playAudio(std::string audioPath) {
        if (audioPath.empty())
            return;
        std::lock_guard<std::mutex> lock(gAudioPlaybackMutex);
        int audio_rate = 22050;
        Uint16 audio_format = AUDIO_S16SYS;
        int audio_channels = 2;
        int audio_buffers = 4096;

        if(Mix_OpenAudio(audio_rate, audio_format, audio_channels, audio_buffers) != 0) return;

        Mix_Chunk *sound = NULL;
        sound = Mix_LoadWAV(audioPath.c_str());
        if(sound == NULL) {
            Mix_FreeChunk(sound);
            Mix_CloseAudio();
            return;
        }

        int channel = Mix_PlayChannel(-1, sound, 0);
        if(channel == -1) {
            Mix_FreeChunk(sound);
            Mix_CloseAudio();
            return;
        }

        while(Mix_Playing(channel) != 0);

        Mix_FreeChunk(sound);
        Mix_CloseAudio();

        return;
    }

    void playNavigationClick() {
        if (!inst::config::soundEnabled)
            return;

        static u64 lastPlayTick = 0;
        const u64 now = armGetSystemTick();
        const u64 tickFreq = armGetSystemTickFreq();
        if (lastPlayTick != 0 && (now - lastPlayTick) < (tickFreq / 20))
            return;
        lastPlayTick = now;

        const std::string audioPath = resolveNavigationClickPath();

        std::lock_guard<std::mutex> lock(gAudioPlaybackMutex);
        if (!ensureNavigationClickAudioReadyLocked(audioPath))
            return;

        Mix_PlayChannel(-1, gNavigationClickChunk, 0);
    }

    void playNavigationClickIfNeeded(std::uint64_t buttonsDown) {
        constexpr std::uint64_t kNavButtons =
            HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left | HidNpadButton_Right |
            HidNpadButton_StickLUp | HidNpadButton_StickLDown | HidNpadButton_StickLLeft | HidNpadButton_StickLRight |
            HidNpadButton_StickRUp | HidNpadButton_StickRDown | HidNpadButton_StickRLeft | HidNpadButton_StickRRight;

        if ((buttonsDown & kNavButtons) != 0)
            playNavigationClick();
    }
    
    static std::vector<std::string> g_cachedUpdateInfo;

   // [arse "MAJOR.MINOR.PATCH" or "vMAJOR.MINOR.PATCH" into a comparable tuple
    static std::tuple<int,int,int> parseSemver(const std::string& v) {
        int ma = 0, mi = 0, pa = 0;
        const char* s = v.c_str();
        if (*s == 'v' || *s == 'V') ++s;   // strip optional 'v' prefix
        std::sscanf(s, "%d.%d.%d", &ma, &mi, &pa);
        return {ma, mi, pa};
    }

   std::vector<std::string> checkForAppUpdate () {
        try {
            std::string jsonData = inst::curl::downloadToBuffer("https://api.github.com/repos/DefenderOfHyrule/Leaflet/releases/latest", 0, 0, 1000L);
            if (jsonData.size() == 0) return {};
            nlohmann::json ourJson = nlohmann::json::parse(jsonData);
            const std::string remoteTag = ourJson["tag_name"].get<std::string>();
            if (parseSemver(remoteTag) > parseSemver(inst::config::appVersion)) {
                std::string downloadUrl;
                if (ourJson.contains("assets") && ourJson["assets"].is_array()) {
                    for (const auto& asset : ourJson["assets"]) {
                        if (!asset.contains("browser_download_url") || !asset["browser_download_url"].is_string()) continue;
                        const std::string url = asset["browser_download_url"].get<std::string>();
                        if (url.size() >= 4 && url.substr(url.size() - 4) == ".nro") {
                            downloadUrl = url;
                            break;
                        }
                    }
                    if (downloadUrl.empty() && !ourJson["assets"].empty()
                        && ourJson["assets"][0].contains("browser_download_url")
                        && ourJson["assets"][0]["browser_download_url"].is_string()) {
                        downloadUrl = ourJson["assets"][0]["browser_download_url"].get<std::string>();
                    }
                }
                if (downloadUrl.empty() && ourJson.contains("zipball_url") && ourJson["zipball_url"].is_string()) {
                    downloadUrl = ourJson["zipball_url"].get<std::string>();
                }

                std::string releaseNotes = "No changelog available for this release.";
                if (ourJson.contains("body") && ourJson["body"].is_string())
                    releaseNotes = NormalizeReleaseNotes(ourJson["body"].get<std::string>());

                std::vector<std::string> ourUpdateInfo = {
                    ourJson["tag_name"].get<std::string>(),
                    downloadUrl,
                    releaseNotes
                };
                
                g_cachedUpdateInfo = ourUpdateInfo;
                return ourUpdateInfo;
            }
        } catch (...) {}
        g_cachedUpdateInfo = {};
        return {};
    }

    static bool fsPatchSysmodulePresent() {
        return std::filesystem::exists("sdmc:/atmosphere/contents/6900000000000420");
    }

    bool isFsPatchLogStale() {
        bool logExists = std::filesystem::exists("sdmc:/config/fs-patch/log.ini");
        return logExists && !fsPatchSysmodulePresent();
    }

    bool checkSigPatches() {
        std::ifstream f("sdmc:/config/fs-patch/log.ini");
        if (!f) return false;

        bool fsPatched = false;
        bool ldrPatched = false;
        bool ldrSectionExists = false;
        std::string currentSection;
        std::string line;

        while (std::getline(f, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            if (line.empty()) continue;

            if (line.front() == '[') {
                const auto end = line.find(']');
                if (end != std::string::npos)
                    currentSection = line.substr(1, end - 1);
                if (currentSection == "ldr") ldrSectionExists = true;
                continue;
            }

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            std::string value = line.substr(eq + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                value.erase(0, 1);

            if (value.substr(0, 7) == "Patched") {
                if (currentSection == "fs")  fsPatched  = true;
                if (currentSection == "ldr") ldrPatched = true;
            }
        }

        return ldrSectionExists ? (fsPatched && ldrPatched) : fsPatched;
    }

    bool isEmuMmc() {
        std::ifstream f("sdmc:/config/fs-patch/log.ini");
        if (!f) return false;

        std::string currentSection;
        std::string line;

        while (std::getline(f, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            if (line.empty()) continue;

            if (line.front() == '[') {
                const auto end = line.find(']');
                if (end != std::string::npos)
                    currentSection = line.substr(1, end - 1);
                continue;
            }

            if (currentSection != "stats") continue;

            const auto eq = line.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = line.substr(0, eq);
            std::string value = line.substr(eq + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
                value.erase(0, 1);

            if (key == "is_emummc")
                return !value.empty() && value.front() == '1';
        }

        return false;
    }
    const std::vector<std::string>& getCachedUpdateInfo() {
        return g_cachedUpdateInfo;
    }

}
