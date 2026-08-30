#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include "util/config.hpp"
#include "util/json.hpp"

namespace inst::config {
    std::string httpUserAgentMode;
    std::vector<QuarkHost> savedHosts;
    std::string httpUserAgent;
    int languageSetting;
    bool ignoreReqVers;
    bool deletePrompt;
    bool soundEnabled;
    bool autoSkipReinstall;
    bool cancelQueueOnError;
    bool usbAck;
    bool verboseInstallLogging;
    bool installDimDisable;
    int installDimDelay;
    bool gcVerifyRepair;
    bool emummcSafetyDisabled;
    bool sigPatchCheckDisabled;
    std::string consoleId;
    bool consoleIdIsCustom;

    // color variables initialized to defaults in parseConfig()
    std::string colorBackground;
    std::string colorTileBase;
    std::string colorTopBar;
    std::string colorBotBar;
    std::string colorTileHighlight;
    std::string colorMenuHighlight;
    std::string colorDialogBackground;
    std::string colorDialogBorder;
    std::string gradientColorA;
    std::string gradientColorB;
    std::string gradientType;
    int          gradientAngle;

    namespace {
        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        std::string Trim(const std::string& value)
        {
            if (value.empty())
                return "";
            std::size_t start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
                return "";
            std::size_t end = value.find_last_not_of(" \t\r\n");
            return value.substr(start, (end - start) + 1);
        }

        int HexVal(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        }
    }

    bool isValidConsoleId(const std::string& id)
    {
        if (id.empty() || id.size() > kConsoleIdMaxLen)
            return false;
        for (unsigned char c : id) {
            const bool ok = std::isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.';
            if (!ok) return false;
        }
        return true;
    }

    std::string NormalizeHttpUserAgentMode(const std::string& mode)
    {
        const std::string normalized = ToLower(Trim(mode));
        if (normalized == "chrome")  return "chrome";
        if (normalized == "safari")  return "safari";
        if (normalized == "blank") return "blank";
        if (normalized == "firefox") return "firefox";
        if (normalized == "custom")  return "custom";
        return "default";
    }

    std::string menuHighlightColorCapped() {
        std::string c = colorMenuHighlight;
        if (c.size() == 9 && c[0] == '#') {
            const int hi = HexVal(c[7]);
            const int lo = HexVal(c[8]);
            if (hi >= 0 && lo >= 0) {
                const int a = (hi << 4) | lo;
                if (a > kMenuHighlightMaxAlpha) {
                    static const char* kHexDigits = "0123456789ABCDEF";
                    c[7] = kHexDigits[(kMenuHighlightMaxAlpha >> 4) & 0xF];
                    c[8] = kHexDigits[kMenuHighlightMaxAlpha & 0xF];
                }
            }
        }
        return c;
    }

    std::string dialogBackgroundColorFloored() {
        std::string c = colorDialogBackground;
        if (c.size() == 9 && c[0] == '#') {
            const int hi = HexVal(c[7]);
            const int lo = HexVal(c[8]);
            if (hi >= 0 && lo >= 0) {
                const int a = (hi << 4) | lo;
                if (a < kDialogBackgroundMinAlpha) {
                    static const char* kHexDigits = "0123456789ABCDEF";
                    c[7] = kHexDigits[(kDialogBackgroundMinAlpha >> 4) & 0xF];
                    c[8] = kHexDigits[kDialogBackgroundMinAlpha & 0xF];
                }
            }
        }
        return c;
    }

    void setConfig() {
        nlohmann::json j = {
            {"deletePrompt",        deletePrompt},
            {"soundEnabled",        soundEnabled},
            {"autoSkipReinstall",   autoSkipReinstall},
            {"cancelQueueOnError",  cancelQueueOnError},
            {"ignoreReqVers",       ignoreReqVers},
            {"languageSetting",     languageSetting},
            {"usbAck",              usbAck},
            {"httpUserAgentMode",   httpUserAgentMode},
            {"httpUserAgent",       httpUserAgent},
            {"verboseInstallLogging", verboseInstallLogging},
            {"installDimDisable",    installDimDisable},
            {"installDimDelay",      installDimDelay},
            {"gcVerifyRepair",       gcVerifyRepair},
            {"emummcSafetyDisabled", emummcSafetyDisabled},
            {"sigPatchCheckDisabled", sigPatchCheckDisabled},
            {"consoleId",            consoleId},
            {"consoleIdIsCustom",    consoleIdIsCustom},
            {"colorBackground",     colorBackground},
            {"colorTileBase",       colorTileBase},
            {"colorTopBar",         colorTopBar},
            {"colorBotBar",         colorBotBar},
            {"colorTileHighlight",  colorTileHighlight},
            {"colorMenuHighlight",  colorMenuHighlight},
            {"colorDialogBackground", colorDialogBackground},
            {"colorDialogBorder",     colorDialogBorder},
            {"gradientColorA",      gradientColorA},
            {"gradientColorB",      gradientColorB},
            {"gradientType",        gradientType},
            {"gradientAngle",       gradientAngle},
        };
        std::ofstream file(inst::config::configPath);
        file << std::setw(4) << j << std::endl;
    }

    static const std::string hostsPath = appDir + "/quark_hosts.json";

    std::string exportTheme(const std::string& name) {
        std::filesystem::create_directories(themesDir);
        std::string safe = name;
        for (auto& c : safe) {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' ||
                c == '"' || c == '<' || c == '>' || c == '|') c = '_';
        }
        const std::string path = themesDir + "/" + safe + ".leaflet.theme";
        nlohmann::json j = {
            {"name",           name},
            {"colorBackground",    colorBackground},
            {"colorTileBase",      colorTileBase},
            {"colorTopBar",        colorTopBar},
            {"colorBotBar",        colorBotBar},
            {"colorTileHighlight", colorTileHighlight},
            {"colorMenuHighlight", colorMenuHighlight},
            {"colorDialogBackground", colorDialogBackground},
            {"colorDialogBorder",     colorDialogBorder},
            {"gradientColorA",     gradientColorA},
            {"gradientColorB",     gradientColorB},
            {"gradientType",       gradientType},
            {"gradientAngle",      gradientAngle},
        };
        try {
            std::ofstream f(path);
            f << std::setw(4) << j << std::endl;
            return path;
        } catch (...) { return ""; }
    }

    std::vector<SavedTheme> loadThemes() {
        std::vector<SavedTheme> result;
        if (!std::filesystem::exists(themesDir)) return result;
        auto isValidColor = [](const std::string& s) {
            if (s.size() != 9 || s[0] != '#') return false;
            for (int i = 1; i < 9; i++) {
                char c = s[i];
                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                    return false;
            }
            return true;
        };
        auto isValidGradientType = [](const std::string& s) {
            return s == "linear" || s == "radial";
        };
        for (const auto& entry : std::filesystem::directory_iterator(themesDir)) {
            if (entry.path().extension() != ".theme") continue;
            if (entry.path().stem().extension() != ".leaflet") continue;
            try {
                std::ifstream f(entry.path());
                nlohmann::json j;
                f >> j;
                SavedTheme t;
                t.name          = j.value("name", entry.path().stem().stem().string());
                t.background    = j.value("colorBackground",    kDefaultColorBackground);
                t.tileBase      = j.value("colorTileBase",      "");
                t.topBar        = j.value("colorTopBar",        kDefaultColorTopBar);
                t.botBar        = j.value("colorBotBar",        kDefaultColorBotBar);
                t.tileHighlight = j.value("colorTileHighlight", kDefaultColorTileHighlight);
                t.menuHighlight = j.value("colorMenuHighlight", kDefaultColorMenuHighlight);
                t.dialogBackground = j.value("colorDialogBackground", kDefaultColorDialogBackground);
                t.dialogBorder     = j.value("colorDialogBorder",     kDefaultColorDialogBorder);
                t.gradA         = j.value("gradientColorA",     kDefaultGradientColorA);
                t.gradB         = j.value("gradientColorB",     kDefaultGradientColorB);
                t.gradType      = j.value("gradientType",       kDefaultGradientType);
                t.gradAngle     = j.value("gradientAngle",      kDefaultGradientAngle);
                // validate all colors fall back to defaults for invalid values
                if (!isValidColor(t.background))    t.background    = kDefaultColorBackground;
                if (!isValidColor(t.tileBase))      t.tileBase      = "";
                if (!isValidColor(t.topBar))        t.topBar        = kDefaultColorTopBar;
                if (!isValidColor(t.botBar))        t.botBar        = kDefaultColorBotBar;
                if (!isValidColor(t.tileHighlight)) t.tileHighlight = kDefaultColorTileHighlight;
                if (!isValidColor(t.menuHighlight))  t.menuHighlight = kDefaultColorMenuHighlight;
                if (!isValidColor(t.dialogBackground)) t.dialogBackground = kDefaultColorDialogBackground;
                if (!isValidColor(t.dialogBorder))     t.dialogBorder     = kDefaultColorDialogBorder;
                if (!isValidColor(t.gradA))         t.gradA         = kDefaultGradientColorA;
                if (!isValidColor(t.gradB))         t.gradB         = kDefaultGradientColorB;
                if (!isValidGradientType(t.gradType)) t.gradType    = kDefaultGradientType;
                if (t.gradAngle < 0 || t.gradAngle > 359) t.gradAngle = kDefaultGradientAngle;
                result.push_back(t);
            } catch (...) {}
        }
        std::sort(result.begin(), result.end(),
            [](const SavedTheme& a, const SavedTheme& b) { return a.name < b.name; });
        return result;
    }

    void applyTheme(const SavedTheme& t) {
        colorBackground    = t.background;
        colorTileBase      = t.tileBase;
        colorTopBar        = t.topBar;
        colorBotBar        = t.botBar;
        colorTileHighlight = t.tileHighlight;
        colorMenuHighlight = t.menuHighlight;
        colorDialogBackground = t.dialogBackground;
        colorDialogBorder     = t.dialogBorder;
        gradientColorA     = t.gradA;
        gradientColorB     = t.gradB;
        gradientType        = t.gradType;
        gradientAngle       = t.gradAngle;
    }

    void saveHosts() {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& h : savedHosts) {
            j.push_back({{"name", h.name}, {"ip", h.ip}});
        }
        std::filesystem::create_directories(appDir);
        std::ofstream f(hostsPath);
        f << std::setw(4) << j << std::endl;
    }

    void loadHosts() {
        savedHosts.clear();
        if (!std::filesystem::exists(hostsPath)) return;
        try {
            std::ifstream f(hostsPath);
            nlohmann::json j;
            f >> j;
            for (const auto& item : j) {
                QuarkHost h;
                h.name = item.value("name", "");
                h.ip   = item.value("ip", "");
                if (!h.name.empty() && !h.ip.empty())
                    savedHosts.push_back(h);
            }
        } catch (...) {}
    }

    void parseConfig() {
        languageSetting     = 99;
        deletePrompt        = true;
        soundEnabled        = true;
        autoSkipReinstall    = false;
        cancelQueueOnError   = true;
        ignoreReqVers       = true;
        usbAck              = false;
        httpUserAgentMode   = "default";
        httpUserAgent.clear();
        verboseInstallLogging = false;
        installDimDisable     = false;
        installDimDelay       = 60;
        gcVerifyRepair        = false;
        emummcSafetyDisabled  = false;
        sigPatchCheckDisabled = false;
        consoleId.clear();
        consoleIdIsCustom     = false;

        // color defaults
        colorBackground    = kDefaultColorBackground;
        colorTileBase      = "";
        colorTopBar        = kDefaultColorTopBar;
        colorBotBar        = kDefaultColorBotBar;
        colorTileHighlight = kDefaultColorTileHighlight;
        colorMenuHighlight = kDefaultColorMenuHighlight;
        colorDialogBackground = kDefaultColorDialogBackground;
        colorDialogBorder     = kDefaultColorDialogBorder;
        gradientColorA     = kDefaultGradientColorA;
        gradientColorB     = kDefaultGradientColorB;
        gradientType       = kDefaultGradientType;
        gradientAngle      = kDefaultGradientAngle;

        bool needsConfigRewrite = false;

        try {
            std::ifstream file(inst::config::configPath);
            nlohmann::json j;
            file >> j;
            if (j.contains("deletePrompt"))         deletePrompt        = j["deletePrompt"].get<bool>();
            if (j.contains("soundEnabled"))         soundEnabled        = j["soundEnabled"].get<bool>();
            if (j.contains("autoSkipReinstall"))    autoSkipReinstall    = j["autoSkipReinstall"].get<bool>();
            if (j.contains("cancelQueueOnError"))   cancelQueueOnError   = j["cancelQueueOnError"].get<bool>();
            if (j.contains("ignoreReqVers"))        ignoreReqVers       = j["ignoreReqVers"].get<bool>();
            if (j.contains("languageSetting"))      languageSetting     = j["languageSetting"].get<int>();
            if (j.contains("usbAck"))               usbAck              = j["usbAck"].get<bool>();
            if (j.contains("httpUserAgentMode"))    httpUserAgentMode   = j["httpUserAgentMode"].get<std::string>();
            if (j.contains("httpUserAgent"))        httpUserAgent       = j["httpUserAgent"].get<std::string>();
            if (j.contains("verboseInstallLogging"))verboseInstallLogging = j["verboseInstallLogging"].get<bool>();
            if (j.contains("installDimDisable"))    installDimDisable     = j["installDimDisable"].get<bool>();
            if (j.contains("installDimDelay")) {
                int v = j["installDimDelay"].get<int>();
                installDimDelay = (v >= 5 && v <= 3600) ? v : 60;
            }
            if (j.contains("gcVerifyRepair"))        gcVerifyRepair        = j["gcVerifyRepair"].get<bool>();
            if (j.contains("emummcSafetyDisabled"))  emummcSafetyDisabled  = j["emummcSafetyDisabled"].get<bool>();
            if (j.contains("sigPatchCheckDisabled")) sigPatchCheckDisabled = j["sigPatchCheckDisabled"].get<bool>();
            if (j.contains("consoleId")) {
                const std::string v = j["consoleId"].get<std::string>();
                if (isValidConsoleId(v)) consoleId = v;
            }
            if (j.contains("consoleIdIsCustom")) consoleIdIsCustom = j["consoleIdIsCustom"].get<bool>();

            // colors fall back to defaults if absent or malformed
            auto loadColor = [&](const char* key, std::string& target, const char* def) {
                if (j.contains(key)) {
                    const auto& v = j[key];
                    if (v.is_string()) {
                        const std::string s = v.get<std::string>();
                        bool valid = (s.size() == 9 && s[0] == '#');
                        if (valid) {
                            for (int i = 1; i < 9; i++) {
                                char c = s[i];
                                if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
                                { valid = false; break; }
                            }
                        }
                        target = valid ? s : def;
                    } else {
                        target = def;
                    }
                }
            };
            loadColor("colorBackground",    colorBackground,    kDefaultColorBackground);
            loadColor("colorTileBase",      colorTileBase,      "");
            loadColor("colorTopBar",        colorTopBar,        kDefaultColorTopBar);
            loadColor("colorBotBar",        colorBotBar,        kDefaultColorBotBar);
            loadColor("colorTileHighlight", colorTileHighlight, kDefaultColorTileHighlight);
            loadColor("colorMenuHighlight", colorMenuHighlight, kDefaultColorMenuHighlight);
            loadColor("colorDialogBackground", colorDialogBackground, kDefaultColorDialogBackground);
            loadColor("colorDialogBorder",     colorDialogBorder,     kDefaultColorDialogBorder);
            loadColor("gradientColorA",     gradientColorA,     kDefaultGradientColorA);
            loadColor("gradientColorB",     gradientColorB,     kDefaultGradientColorB);

            if (j.contains("gradientType")) {
                const auto& v = j["gradientType"];
                if (v.is_string() && (v.get<std::string>() == "linear" || v.get<std::string>() == "radial"))
                    gradientType = v.get<std::string>();
                else
                    gradientType = kDefaultGradientType;
            }
            if (j.contains("gradientAngle")) {
                if (j["gradientAngle"].is_number_integer()) {
                    int v = j["gradientAngle"].get<int>();
                    gradientAngle = (v >= 0 && v <= 359) ? v : kDefaultGradientAngle;
                } else {
                    gradientAngle = kDefaultGradientAngle;
                }
            }

            static const char* currentKeys[] = {
                "deletePrompt", "soundEnabled",
                "autoSkipReinstall", "cancelQueueOnError",
                "ignoreReqVers", "languageSetting",
                "usbAck", "httpUserAgentMode", "httpUserAgent",
                "verboseInstallLogging", "installDimDisable", "installDimDelay", "gcVerifyRepair", "emummcSafetyDisabled", "sigPatchCheckDisabled",
                "gradientType", "gradientAngle"
            };
            for (const char* key : currentKeys) {
                if (!j.contains(key)) {
                    needsConfigRewrite = true;
                    break;
                }
            }
        }
        catch (...) {
            setConfig();
        }

        httpUserAgentMode = NormalizeHttpUserAgentMode(httpUserAgentMode);

        std::error_code ec;
        std::filesystem::create_directories(inst::config::appDir, ec);

        if (needsConfigRewrite)
            setConfig();
    }
}