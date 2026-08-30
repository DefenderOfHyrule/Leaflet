#include <curl/curl.h>
#include <string>
#include <sstream>
#include <iostream>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>
#include <vector>
#include "util/curl.hpp"
#include "util/config.hpp"
#include "util/error.hpp"
#include "util/lang.hpp"
#include "util/uid.hpp"
#include "ui/instPage.hpp"

static size_t writeDataFile(void *ptr, size_t size, size_t nmemb, void *stream) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

static bool isLikelyImageFile(const char *path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    unsigned char buf[12] = {};
    in.read(reinterpret_cast<char *>(buf), sizeof(buf));
    std::streamsize read = in.gcount();
    if (read >= 3 && buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) return true;
    if (read >= 8 && buf[0] == 0x89 && buf[1] == 0x50 && buf[2] == 0x4E && buf[3] == 0x47 &&
        buf[4] == 0x0D && buf[5] == 0x0A && buf[6] == 0x1A && buf[7] == 0x0A) return true;
    if (read >= 12 && buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F' &&
        buf[8] == 'W' && buf[9] == 'E' && buf[10] == 'B' && buf[11] == 'P') return true;
    return false;
}

namespace inst::curl {
    const std::string& getDefaultUserAgent() {
        static const std::string kDefaultUserAgent = "leaflet";
        return kDefaultUserAgent;
    }

    const std::string& getEmptyUserAgent() {
        static const std::string kEmptyUserAgent;
        return kEmptyUserAgent;
    }

    const std::string& getDownloadUserAgent() {
        static const std::string kChromeUserAgent =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36";
        static const std::string kSafariUserAgent =
            "Mozilla/5.0 (iPhone; CPU iPhone OS 17_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.3 Mobile/15E148 Safari/604.1";
        static const std::string kFirefoxUserAgent =
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:123.0) Gecko/20100101 Firefox/123.0";

        const std::string mode = inst::config::NormalizeHttpUserAgentMode(inst::config::httpUserAgentMode);
        if (mode == "chrome")  return kChromeUserAgent;
        if (mode == "safari")  return kSafariUserAgent;
        if (mode == "blank") return getEmptyUserAgent();
        if (mode == "firefox") return kFirefoxUserAgent;
        if (mode == "custom")  return inst::config::httpUserAgent;
        return getDefaultUserAgent();
    }

    const std::string& getUserAgent() {
        return getDownloadUserAgent();
    }
}

size_t writeDataBuffer(char *ptr, size_t size, size_t nmemb, void *userdata) {
    std::ostringstream *stream = (std::ostringstream*)userdata;
    size_t count = size * nmemb;
    stream->write(ptr, count);
    return count;
}

int progress_callback(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (ultotal) {
        int uploadProgress = (int)(((double)ulnow / (double)ultotal) * 100.0);
        inst::ui::instPage::setInstBarPerc(uploadProgress);
    } else if (dltotal) {
        int downloadProgress = (int)(((double)dlnow / (double)dltotal) * 100.0);
        inst::ui::instPage::setInstBarPerc(downloadProgress);
    }
    return 0;
}

struct DownloadProgressContext {
    const inst::curl::DownloadProgressCallback* cb = nullptr;
    curl_off_t lastNow = -1;
    curl_off_t lastTotal = -1;
};

struct WriteAtOffsetContext {
    FILE* file = nullptr;
};

int progress_callback_file(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) {
    auto* ctx = static_cast<DownloadProgressContext*>(clientp);
    if (!ctx || !ctx->cb || !(*ctx->cb)) return 0;
    if (ctx->lastNow == dlnow && ctx->lastTotal == dltotal) return 0;
    ctx->lastNow = dlnow; ctx->lastTotal = dltotal;
    const std::uint64_t now   = dlnow   > 0 ? static_cast<std::uint64_t>(dlnow)   : 0;
    const std::uint64_t total = dltotal > 0 ? static_cast<std::uint64_t>(dltotal) : 0;
    (*ctx->cb)(now, total);
    return 0;
}

static size_t writeDataFileAtOffset(void *ptr, size_t size, size_t nmemb, void *stream) {
    auto* ctx = static_cast<WriteAtOffsetContext*>(stream);
    if (!ctx || !ctx->file) return 0;
    return fwrite(ptr, size, nmemb, ctx->file);
}

static constexpr long kDefaultConnectTimeoutMs   = 15000;
static constexpr long kLowSpeedLimitBytesPerSec  = 1;
static constexpr long kLowSpeedTimeSeconds        = 45;
static constexpr long kFileDownloadCurlBufferSize = 512L * 1024L;
static constexpr std::size_t kFileDownloadIoBufferSize = 1024U * 1024U;

static bool ensureCurlGlobalInit() {
    static std::once_flag initFlag;
    static bool initOk = false;
    std::call_once(initFlag, []() { initOk = (curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK); });
    return initOk;
}

static void removeFileIfExistsNoThrow(const char* path) {
    if (!path || path[0] == '\0') return;
    std::error_code ec;
    std::filesystem::remove(path, ec);
}

static void applyCommonCurlOptions(CURL *curl_handle, const std::string& url, long timeout, bool writeProgress) {
    curl_easy_setopt(curl_handle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, inst::curl::getDownloadUserAgent().c_str());
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl_handle, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_BUFFERSIZE, kFileDownloadCurlBufferSize);
    curl_easy_setopt(curl_handle, CURLOPT_TCP_KEEPALIVE, 1L);
    if (writeProgress) {
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    }
    if (timeout > 0) {
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT_MS, timeout);
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, timeout);
    } else {
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT_MS, kDefaultConnectTimeoutMs);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, kLowSpeedLimitBytesPerSec);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, kLowSpeedTimeSeconds);
    }
}

static void applyBufferedFileIo(FILE* file) {
    if (file) setvbuf(file, nullptr, _IOFBF, kFileDownloadIoBufferSize);
}

namespace inst::curl {
    bool downloadFile(const std::string ourUrl, const char *pagefilename, long timeout, bool writeProgress) {
        if (!ensureCurlGlobalInit()) return false;
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return false;
        applyCommonCurlOptions(curl_handle, ourUrl, timeout, writeProgress);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataFile);
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        FILE *pagefile = fopen(pagefilename, "wb");
        if (!pagefile) { curl_easy_cleanup(curl_handle); return false; }
        applyBufferedFileIo(pagefile);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
        const CURLcode result = curl_easy_perform(curl_handle);
        long responseCode = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        fclose(pagefile);
        curl_easy_cleanup(curl_handle);
        if (result == CURLE_OK && responseCode >= 200 && responseCode < 300) return true;
        removeFileIfExistsNoThrow(pagefilename);
        LOG_DEBUG("downloadFile failed rc=%s http=%ld url=%s\n", curl_easy_strerror(result), responseCode, ourUrl.c_str());
        return false;
    }

    bool downloadFileWithProgress(const std::string ourUrl, const char *pagefilename, long timeout, const DownloadProgressCallback& progressCb) {
        if (!ensureCurlGlobalInit()) return false;
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return false;
        applyCommonCurlOptions(curl_handle, ourUrl, timeout, false);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataFile);
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        DownloadProgressContext progressCtx{};
        progressCtx.cb = &progressCb; progressCtx.lastNow = -1; progressCtx.lastTotal = -1;
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback_file);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &progressCtx);
        FILE *pagefile = fopen(pagefilename, "wb");
        if (!pagefile) { curl_easy_cleanup(curl_handle); return false; }
        applyBufferedFileIo(pagefile);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
        const CURLcode result = curl_easy_perform(curl_handle);
        long responseCode = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        fclose(pagefile);
        curl_easy_cleanup(curl_handle);
        if (result == CURLE_OK && responseCode >= 200 && responseCode < 300) {
            if (progressCb)
                progressCb(progressCtx.lastNow > 0 ? static_cast<std::uint64_t>(progressCtx.lastNow) : 0,
                           progressCtx.lastTotal > 0 ? static_cast<std::uint64_t>(progressCtx.lastTotal) : 0);
            return true;
        }
        removeFileIfExistsNoThrow(pagefilename);
        LOG_DEBUG("downloadFileWithProgress failed rc=%s http=%ld url=%s\n", curl_easy_strerror(result), responseCode, ourUrl.c_str());
        return false;
    }

    bool downloadFileRangeWithProgress(const std::string ourUrl, const char *pagefilename, std::uint64_t start, std::uint64_t endInclusive, long timeout, const DownloadProgressCallback& progressCb) {
        if (!ensureCurlGlobalInit()) return false;
        if (endInclusive < start) return false;
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return false;
        applyCommonCurlOptions(curl_handle, ourUrl, timeout, false);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataFile);
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        DownloadProgressContext progressCtx{};
        progressCtx.cb = &progressCb; progressCtx.lastNow = -1; progressCtx.lastTotal = -1;
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback_file);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &progressCtx);
        const std::string range = std::to_string(start) + "-" + std::to_string(endInclusive);
        curl_easy_setopt(curl_handle, CURLOPT_RANGE, range.c_str());
        FILE *pagefile = fopen(pagefilename, "wb");
        if (!pagefile) { curl_easy_cleanup(curl_handle); return false; }
        applyBufferedFileIo(pagefile);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
        const CURLcode result = curl_easy_perform(curl_handle);
        long responseCode = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        fclose(pagefile);
        curl_easy_cleanup(curl_handle);
        if (result == CURLE_OK && responseCode == 206) {
            if (progressCb)
                progressCb(progressCtx.lastNow > 0 ? static_cast<std::uint64_t>(progressCtx.lastNow) : 0,
                           progressCtx.lastTotal > 0 ? static_cast<std::uint64_t>(progressCtx.lastTotal) : 0);
            return true;
        }
        removeFileIfExistsNoThrow(pagefilename);
        LOG_DEBUG("downloadFileRangeWithProgress failed rc=%s http=%ld url=%s\n", curl_easy_strerror(result), responseCode, ourUrl.c_str());
        return false;
    }

    bool downloadFileRangeToOffsetWithProgress(const std::string ourUrl, const char *pagefilename, std::uint64_t fileOffset, std::uint64_t start, std::uint64_t endInclusive, long timeout, const DownloadProgressCallback& progressCb) {
        if (!ensureCurlGlobalInit()) return false;
        if (endInclusive < start) return false;
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return false;
        applyCommonCurlOptions(curl_handle, ourUrl, timeout, false);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataFileAtOffset);
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
        DownloadProgressContext progressCtx{};
        progressCtx.cb = &progressCb; progressCtx.lastNow = -1; progressCtx.lastTotal = -1;
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFOFUNCTION, progress_callback_file);
        curl_easy_setopt(curl_handle, CURLOPT_XFERINFODATA, &progressCtx);
        const std::string range = std::to_string(start) + "-" + std::to_string(endInclusive);
        curl_easy_setopt(curl_handle, CURLOPT_RANGE, range.c_str());
        FILE *pagefile = fopen(pagefilename, "r+b");
        if (!pagefile) { curl_easy_cleanup(curl_handle); return false; }
        applyBufferedFileIo(pagefile);
        if (fseeko(pagefile, static_cast<off_t>(fileOffset), SEEK_SET) != 0) {
            fclose(pagefile); curl_easy_cleanup(curl_handle); return false;
        }
        WriteAtOffsetContext writeCtx{}; writeCtx.file = pagefile;
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &writeCtx);
        const CURLcode result = curl_easy_perform(curl_handle);
        long responseCode = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        fflush(pagefile); fclose(pagefile);
        curl_easy_cleanup(curl_handle);
        if (result == CURLE_OK && responseCode == 206) {
            if (progressCb)
                progressCb(progressCtx.lastNow > 0 ? static_cast<std::uint64_t>(progressCtx.lastNow) : 0,
                           progressCtx.lastTotal > 0 ? static_cast<std::uint64_t>(progressCtx.lastTotal) : 0);
            return true;
        }
        LOG_DEBUG("downloadFileRangeToOffsetWithProgress failed rc=%s http=%ld url=%s\n", curl_easy_strerror(result), responseCode, ourUrl.c_str());
        return false;
    }

    std::string downloadToBuffer(const std::string ourUrl, int firstRange, int secondRange, long timeout) {
        if (!ensureCurlGlobalInit()) return "";
        CURL *curl_handle = curl_easy_init();
        if (!curl_handle) return "";
        std::ostringstream stream;
        applyCommonCurlOptions(curl_handle, ourUrl, timeout, false);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, writeDataBuffer);
        std::string ourRange;
        if (firstRange && secondRange) {
            ourRange = std::to_string(firstRange) + "-" + std::to_string(secondRange);
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, ourRange.c_str());
        }
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &stream);
        const CURLcode result = curl_easy_perform(curl_handle);
        long responseCode = 0;
        curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &responseCode);
        curl_easy_cleanup(curl_handle);
        if (result == CURLE_OK && responseCode >= 200 && responseCode < 300) return stream.str();
        LOG_DEBUG("downloadToBuffer failed rc=%s http=%ld url=%s\n", curl_easy_strerror(result), responseCode, ourUrl.c_str());
        return "";
    }
}
