#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inst::config {
    static const std::string appDir = "sdmc:/switch/Leaflet";
    static const std::string configPath = appDir + "/config.json";
    static const std::string appVersion = std::string(APP_VERSION);

#ifdef APP_GIT_META
    static const std::string appGitMeta = std::string(APP_GIT_META);
#else
    static const std::string appGitMeta = std::string();
#endif
#ifdef APP_VERSION_FULL
    static const std::string appVersionFull = std::string(APP_VERSION_FULL);
#else
    static const std::string appVersionFull = std::string(APP_VERSION);
#endif

    // default colors.
    static constexpr const char* kDefaultColorBackground     = "#670000FF";
    static constexpr const char* kDefaultColorTopBar         = "#8D00FFBF";
    static constexpr const char* kDefaultColorBotBar         = "#17090980";
    static constexpr const char* kDefaultColorTileHighlight  = "#FF3B0043";
    static constexpr const char* kDefaultColorMenuHighlight  = "#FF3B0043";
    static constexpr const char* kDefaultColorDialogBackground = "#E1E1E1FF";
    static constexpr const char* kDefaultColorDialogBorder     = "#00000040";
    static constexpr const char* kDefaultGradientColorA      = "#FF00FFFF";
    static constexpr const char* kDefaultGradientColorB      = "#00FFFFFF";
    static constexpr const char* kDefaultGradientType        = "linear";
    static constexpr int         kDefaultGradientAngle       = 45;

    static constexpr std::uint8_t kMenuHighlightMaxAlpha = 0x80;
    static constexpr std::uint8_t kDialogBackgroundMinAlpha = 0x99;

    // runtime color values read from config, defaulting to the constants above.
    // all pages use these; changes take effect on next layout rebuild.
    extern std::string colorBackground;
    extern std::string colorTileBase;
    extern std::string colorTopBar;
    extern std::string colorBotBar;
    extern std::string colorTileHighlight;
    extern std::string colorMenuHighlight;
    extern std::string colorDialogBackground;
    extern std::string colorDialogBorder;
    extern std::string gradientColorA;
    extern std::string gradientColorB;
    extern std::string gradientType;
    extern int          gradientAngle;

    std::string menuHighlightColorCapped();
    std::string dialogBackgroundColorFloored();

    static const std::string themesDir = appDir + "/themes";

    struct SavedTheme {
        std::string name;
        std::string background;
        std::string tileBase;
        std::string topBar;
        std::string botBar;
        std::string tileHighlight;
        std::string menuHighlight;
        std::string dialogBackground;
        std::string dialogBorder;
        std::string gradA;
        std::string gradB;
        std::string gradType;
        int          gradAngle;
    };

    // save the current colors to a .leaflet.theme file. returns the path, or "" on failure
    std::string exportTheme(const std::string& name);

    // load all .leaflet.theme files from the themes directory
    std::vector<SavedTheme> loadThemes();

    // apply a saved theme to the live config variables
    void applyTheme(const SavedTheme& t);

    extern std::string httpUserAgentMode;
    extern std::string httpUserAgent;
    extern int languageSetting;
    extern bool ignoreReqVers;
    extern bool validateNCAs;
    extern bool overClock;
    extern bool deletePrompt;
    extern bool soundEnabled;
    extern bool autoSkipReinstall;
    extern bool cancelQueueOnError;
    extern bool usbAck;
    extern bool verboseInstallLogging;
    extern bool installDimDisable;
    extern int installDimDelay;
    extern bool gcVerifyRepair;
    extern bool emummcSafetyDisabled;
    extern bool sigPatchCheckDisabled;

    extern std::string consoleId;
    extern bool consoleIdIsCustom;
    constexpr size_t kConsoleIdMaxLen = 32;
    bool isValidConsoleId(const std::string& id);

    std::string NormalizeHttpUserAgentMode(const std::string& mode);

    struct QuarkHost {
        std::string name;
        std::string ip;
    };
    extern std::vector<QuarkHost> savedHosts;
    void setConfig();
    void saveHosts();
    void loadHosts();
    void parseConfig();
}
