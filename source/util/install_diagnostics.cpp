#include "util/install_diagnostics.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>

#include "util/config.hpp"
#include "util/error.hpp"

namespace inst::diag {
    namespace {
        std::mutex g_logMutex;
        std::string g_logPath;

        std::string ToLower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return value;
        }

        std::string TimestampNow()
        {
            const std::time_t t = std::time(nullptr);
            std::tm tmInfo{};
#if defined(_WIN32)
            localtime_s(&tmInfo, &t);
#else
            localtime_r(&t, &tmInfo);
#endif
            char buffer[32] = {};
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tmInfo);
            return std::string(buffer);
        }

        bool ContainsAny(const std::string& haystackLower, const std::initializer_list<const char*>& needles)
        {
            for (const char* needle : needles) {
                if (haystackLower.find(needle) != std::string::npos)
                    return true;
            }
            return false;
        }

        void AppendLine(const std::string& level, const std::string& message)
        {
            std::lock_guard<std::mutex> lock(g_logMutex);

            const std::filesystem::path logsDir = std::filesystem::path(inst::config::appDir) / "logs";
            std::error_code ec;
            std::filesystem::create_directories(logsDir, ec);

            if (g_logPath.empty())
                g_logPath = (logsDir / "install.log").string();

            std::ofstream out(g_logPath, std::ios::out | std::ios::app);
            if (!out)
                return;

            out << "[" << TimestampNow() << "] [" << level << "] " << message << "\n";
        }
    }

    bool IsVerboseEnabled()
    {
        return inst::config::verboseInstallLogging;
    }

    const std::string& GetInstallLogPath()
    {
        if (g_logPath.empty()) {
            const std::filesystem::path logsDir = std::filesystem::path(inst::config::appDir) / "logs";
            g_logPath = (logsDir / "install.log").string();
        }
        return g_logPath;
    }

    void StartSession(const std::string& source, std::size_t totalItems)
    {
        std::ostringstream line;
        line << "Session start source=" << source << " items=" << totalItems << " verbose=" << (IsVerboseEnabled() ? "on" : "off");
        AppendLine("INFO", line.str());
    }

    void NoteTransferReceived(const std::string& item)
    {
        AppendLine("INFO", "Transfer received: " + item);
    }

    void NoteInstallStarted(const std::string& item)
    {
        AppendLine("INFO", "Install started: " + item);
    }

    void NoteStep(const std::string& step, bool verboseOnly)
    {
        if (verboseOnly && !IsVerboseEnabled())
            return;
        AppendLine("DEBUG", step);
    }

    void RecordSuccess(const std::string& item)
    {
        AppendLine("INFO", "Install succeeded: " + item);
    }

    InstallFailure ClassifyFailure(const std::string& errorText)
    {
        InstallFailure failure{};
        failure.rawMessage = errorText;
        const std::string lower = ToLower(errorText);

        std::smatch m;
        const std::regex rcRegex("(0x[0-9a-fA-F]{8})");
        if (std::regex_search(errorText, m, rcRegex) && m.size() > 1)
            failure.code = m[1].str();

        if (ContainsAny(lower, {"installation canceled", "cancelled", "canceled"})) {
            failure.canceled = true;
            failure.category = "Canceled";
            failure.summary = "Installation canceled by user.";
            return failure;
        }

        if (ContainsAny(lower, {"already installed at the same version", "same version"})) {
            failure.skipItem = true;
            failure.category = "Already installed";
            failure.summary = "This version is already installed on this console.";
            failure.recommendation = "No action needed.";
            return failure;
        }

        if (ContainsAny(lower, {"newer version", "downgrade blocked"})) {
            failure.category = "Downgrade blocked";
            failure.summary = "A newer version of this title is already installed.";
            failure.recommendation = "Uninstall the existing version first if you want to downgrade.";
            return failure;
        }

        if (ContainsAny(lower, {"signature", "master key", "key mismatch", "ticket / cert", "tik", "cert", "rights id"})) {
            failure.category = "Bad dump / Personalized ticket (es)";
            failure.summary = "[ERROR] Content has a device-specific (personalized) ticket that cannot be imported on this console.";
            failure.recommendation = "Please redump your game using nxdumptool and enable \"nca/tik: remove titlekey crypto\" before dumping.";
            return failure;
        }

        if (ContainsAny(lower, {"invalid nca magic", "bad dump", "unreadable", "truncated", "corrupt", "decompress", "hash", "cnmt"})) {
            failure.category = "Corrupt or incomplete file";
            failure.summary = "[ERROR] Package appears corrupt or incomplete.";
            failure.recommendation = "Redump the NSP/XCI and verify its integrity.";
            return failure;
        }

        if (ContainsAny(lower, {"failed to import ticket", "personalized", "device-specific", "titlekey crypto", "improperly dumped"})) {
            failure.category = "fs-patch patching failure (fs)";
            failure.summary = "[ERROR] You normally shouldn't be able to see this error message. Leaflet has detected the stale log.ini in sd:/config/fs-patch.";
            failure.recommendation = "If you do, please (re)install fs-patch.";
            return failure;
        }

        if (ContainsAny(lower, {"not enough free space", "not enough space"})) {
            failure.category = "Not enough storage space";
            failure.summary = "[ERROR] There is not enough free space on the target storage to install this content.";
            failure.recommendation = "Delete unused content or installed games to free up space, then try again.";
            return failure;
        }

        if (ContainsAny(lower, {"lost contact with the gamecard"})) {
            failure.category = "Gamecard removed";
            failure.summary = "[ERROR] Contact with the gamecard was lost partway through installing.";
            failure.recommendation = "Reinsert the gamecard, make sure it's seated properly, and try again.";
            return failure;
        }

        if (ContainsAny(lower, {"failed to register", "failed to set content records", "commit content records", "failed to read file", "failed to write", "storage", "sd", "i/o", "no space", "filesystem"})) {
            failure.category = "Storage write failure";
            failure.summary = "[ERROR] Failed to write content to target storage. Leaflet may have detected the stale log.ini in sd:/config/fs-patch.";
            failure.recommendation = "(Re)install fs-patch, check filesystem health, and check write permissions.";
            return failure;
        }

        if (ContainsAny(lower, {"firmware", "required system firmware", "too low to decrypt", "unsupported", "key generation"})) {
            failure.category = "Firmware version too low";
            failure.summary = "[ERROR] This content requires a higher firmware version.";
            failure.recommendation = "Update your firmware.";
            return failure;
        }

        failure.category = "Unclassified";
        failure.summary = "[ERROR] Installation failed due to an unknown error.";
        failure.recommendation = "Review install log and raw error details.";
        return failure;
    }

    std::string BuildUserMessage(const InstallFailure& failure)
    {
        if (failure.canceled)
            return failure.summary;

        std::string text = failure.summary + "\nCategory: " + failure.category;
        if (!failure.code.empty())
            text += "\nCode: " + failure.code;
        if (!failure.recommendation.empty())
            text += "\nHint: " + failure.recommendation;
        if (IsVerboseEnabled() && !failure.rawMessage.empty())
            text += "\n\nRaw: " + failure.rawMessage;
        text += "\n\nLog: " + GetInstallLogPath();
        return text;
    }

    void RecordFailure(const std::string& item, const InstallFailure& failure)
    {
        std::ostringstream line;
        line << "Install failed: " << item << " category=" << failure.category;
        if (!failure.code.empty())
            line << " code=" << failure.code;
        if (!failure.rawMessage.empty())
            line << " raw=\"" << failure.rawMessage << "\"";
        AppendLine("ERROR", line.str());
    }
}