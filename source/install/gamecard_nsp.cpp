#include "install/gamecard_nsp.hpp"
#include "error.hpp"
#include "debug.h"
#include "nx/nca_writer.h"
#include "ui/instPage.hpp"
#include "util/lang.hpp"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>

namespace leaf::install::nsp
{
    // magic for PFS0
    static constexpr u32 PFS0_MAGIC = 0x30534650; // "PFS0"

    GamecardNSP::GamecardNSP(const std::string& mountPath)
        : m_mountPath(mountPath)
    {
        // scan the mounted gamecard directory for all files
        DIR* dir = opendir(mountPath.c_str());
        if (!dir)
            THROW_FORMAT("Failed to open gamecard mount path: %s", mountPath.c_str());

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            if (entry->d_name[0] == '.') continue; // skip . and ..

            std::string fullPath = mountPath + entry->d_name;
            struct stat st = {};
            if (stat(fullPath.c_str(), &st) != 0) continue;
            if (!S_ISREG(st.st_mode)) continue;

            FileInfo fi;
            fi.name = entry->d_name;
            fi.fullPath = fullPath;
            fi.fileSize = static_cast<u64>(st.st_size);
            fi.virtualOffset = m_totalDataSize;
            m_totalDataSize += fi.fileSize;

            m_files.push_back(fi);
        }
        closedir(dir);

        if (m_files.empty())
            THROW_FORMAT("No files found on gamecard at %s", mountPath.c_str());

        LOG_DEBUG("GamecardNSP: found %zu files, total data size: %lu\n",
                  m_files.size(), m_totalDataSize);
    }

    GamecardNSP::~GamecardNSP()
    {
    }

    void GamecardNSP::RetrieveHeader()
    {

        const u32 numFiles = static_cast<u32>(m_files.size());

        std::vector<u8> stringTable;
        std::vector<u32> stringOffsets;
        for (const auto& fi : m_files) {
            stringOffsets.push_back(static_cast<u32>(stringTable.size()));
            stringTable.insert(stringTable.end(), fi.name.begin(), fi.name.end());
            stringTable.push_back(0); // null terminator
        }
        while (stringTable.size() % 0x20 != 0)
            stringTable.push_back(0);

        const u32 stringTableSize = static_cast<u32>(stringTable.size());

        const size_t headerSize = sizeof(PFS0BaseHeader)
            + numFiles * sizeof(PFS0FileEntry)
            + stringTableSize;

        m_headerBytes.resize(headerSize, 0);

        PFS0BaseHeader* baseHeader = reinterpret_cast<PFS0BaseHeader*>(m_headerBytes.data());
        baseHeader->magic = PFS0_MAGIC;
        baseHeader->numFiles = numFiles;
        baseHeader->stringTableSize = stringTableSize;
        baseHeader->reserved = 0;

        for (u32 i = 0; i < numFiles; i++) {
            PFS0FileEntry* fe = reinterpret_cast<PFS0FileEntry*>(
                m_headerBytes.data() + sizeof(PFS0BaseHeader) + i * sizeof(PFS0FileEntry));
            fe->dataOffset = m_files[i].virtualOffset;
            fe->fileSize = m_files[i].fileSize;
            fe->stringTableOffset = stringOffsets[i];
            fe->padding = 0;
        }

        std::memcpy(m_headerBytes.data() + sizeof(PFS0BaseHeader) + numFiles * sizeof(PFS0FileEntry),
                    stringTable.data(), stringTableSize);

        LOG_DEBUG("GamecardNSP: built synthetic PFS0 header, %zu bytes, %u files\n",
                  headerSize, numFiles);
    }

    void GamecardNSP::readFromFiles(void* buf, u64 virtualOffset, u64 size)
    {

        u8* dst = static_cast<u8*>(buf);
        u64 remaining = size;
        u64 pos = virtualOffset;

        for (const auto& fi : m_files) {
            if (remaining == 0) break;

            u64 fileEnd = fi.virtualOffset + fi.fileSize;
            if (pos >= fileEnd) continue;

            u64 offsetInFile = (pos > fi.virtualOffset) ? (pos - fi.virtualOffset) : 0;
            u64 availableInFile = fi.fileSize - offsetInFile;
            u64 toRead = std::min(remaining, availableInFile);

            FILE* fp = fopen(fi.fullPath.c_str(), "rb");
            if (!fp) {
                LOG_DEBUG("GamecardNSP: failed to open %s\n", fi.fullPath.c_str());
                std::memset(dst, 0, toRead);
            } else {
                fseeko(fp, static_cast<off_t>(offsetInFile), SEEK_SET);
                size_t got = fread(dst, 1, static_cast<size_t>(toRead), fp);
                if (got < toRead) {
                    std::memset(dst + got, 0, toRead - got);
                }
                fclose(fp);
            }

            dst += toRead;
            pos += toRead;
            remaining -= toRead;
        }

        if (remaining > 0) {
            std::memset(dst, 0, remaining);
        }
    }

    void GamecardNSP::BufferData(void* buf, off_t offset, size_t size)
    {
        const u64 headerSize = static_cast<u64>(m_headerBytes.size());
        const u64 absOffset = static_cast<u64>(offset);

        if (absOffset < headerSize) {
            u64 headerReadSize = std::min(static_cast<u64>(size), headerSize - absOffset);
            std::memcpy(buf, m_headerBytes.data() + absOffset, headerReadSize);

            if (headerReadSize < size) {
                // read crosses into data region
                readFromFiles(static_cast<u8*>(buf) + headerReadSize,
                              0, size - headerReadSize);
            }
        } else {
            // reading from the data region
            u64 dataOffset = absOffset - headerSize;
            readFromFiles(buf, dataOffset, size);
        }
    }

    void GamecardNSP::StreamToPlaceholder(std::shared_ptr<nx::ncm::ContentStorage>& contentStorage, NcmContentId ncaId)
    {
        const PFS0FileEntry* fileEntry = this->GetFileEntryByNcaId(ncaId);
        if (!fileEntry)
            THROW_FORMAT("GamecardNSP: NCA not found on gamecard");

        std::string ncaFileName = this->GetFileEntryName(fileEntry);
        LOG_DEBUG("GamecardNSP: Streaming %s\n", ncaFileName.c_str());

        size_t ncaSize = fileEntry->fileSize;
        NcaWriter writer(ncaId, contentStorage);

        std::string targetPath;
        for (const auto& fi : m_files) {
            if (fi.name == ncaFileName) {
                targetPath = fi.fullPath;
                break;
            }
        }

        if (targetPath.empty())
            THROW_FORMAT("GamecardNSP: file %s not found in mount", ncaFileName.c_str());

        FILE* fp = fopen(targetPath.c_str(), "rb");
        if (!fp)
            THROW_FORMAT("GamecardNSP: failed to open %s", targetPath.c_str());

        u64 fileOff = 0;
        u32 buffsize = ncaSize > 0x2000000 ? 0x2000000 /* 32mb buffer */ : 0x100000 /* 1mb buffer */;
        auto readBuffer = std::make_unique<u8[]>(buffsize);

        try
        {
            inst::ui::instPage::setInstInfoText("inst.info_page.top_info0"_lang + ncaFileName + " (Gamecard)...");
            inst::ui::instPage::setInstBarPerc(0);
            inst::ui::instPage::setProgressDetailText("0% • Calculating... • -- MB/s");

            auto lastTime = std::chrono::steady_clock::now();
            std::uint64_t lastBytes = 0;
            double emaRate = 0.0;

            while (fileOff < ncaSize)
            {
                float progress = (float)fileOff / (float)ncaSize;

                if (fileOff % (0x400000 * 3) == 0) {
                    inst::ui::instPage::setInstBarPerc((double)(progress * 100.0));

                    const auto now = std::chrono::steady_clock::now();
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTime).count();
                    if (elapsed >= 1000) {
                        const auto delta = static_cast<std::uint64_t>(fileOff - lastBytes);
                        const double rate = (elapsed > 0) ? (double)delta / ((double)elapsed / 1000.0) : 0.0;
                        if (rate > 0.0) {
                            emaRate = (emaRate <= 0.0) ? rate : (emaRate * 0.7 + rate * 0.3);
                        }
                        lastBytes = fileOff;
                        lastTime = now;
                    }

                    std::string etaText = "Calculating...";
                    if (emaRate > 0.0 && fileOff < ncaSize) {
                        const auto remaining = static_cast<std::uint64_t>(ncaSize - fileOff);
                        const auto seconds = static_cast<std::uint64_t>(remaining / emaRate);
                        const auto h = seconds / 3600;
                        const auto m = (seconds % 3600) / 60;
                        const auto s = seconds % 60;
                        if (h > 0) {
                            etaText = std::to_string(h) + ":" + (m < 10 ? "0" : "") + std::to_string(m)
                                + ":" + (s < 10 ? "0" : "") + std::to_string(s);
                        } else {
                            etaText = std::to_string(m) + ":" + (s < 10 ? "0" : "") + std::to_string(s);
                        }
                        etaText += " remaining";
                    }

                    std::string speedText;
                    if (emaRate > 0.0) {
                        const double mbps = emaRate / (1024.0 * 1024.0);
                        const double rounded = std::round(mbps * 10.0) / 10.0;
                        speedText = std::to_string(rounded);
                        if (speedText.find('.') != std::string::npos) {
                            while (!speedText.empty() && speedText.back() == '0') speedText.pop_back();
                            if (!speedText.empty() && speedText.back() == '.') speedText.pop_back();
                        }
                        speedText += " MB/s";
                    } else {
                        speedText = "-- MB/s";
                    }

                    const int pct = static_cast<int>(progress * 100.0 + 0.5);
                    inst::ui::instPage::setProgressDetailText(
                        std::to_string(pct) + "% • " + etaText + " • " + speedText);
                }

                size_t readSize = (fileOff + buffsize >= ncaSize) ? ncaSize - fileOff : buffsize;

                if (inst::ui::instPage::isInstallCancelRequested())
                    THROW_FORMAT("Installation canceled.");

                fseeko(fp, static_cast<off_t>(fileOff), SEEK_SET);
                size_t got = fread(readBuffer.get(), 1, readSize, fp);
                if (got < readSize) {
                    std::memset(readBuffer.get() + got, 0, readSize - got);
                }

                writer.write(readBuffer.get(), readSize);
                fileOff += readSize;
            }

            inst::ui::instPage::setInstBarPerc(100);
            inst::ui::instPage::setProgressDetailText("100% • done");
        }
        catch (std::exception& e)
        {
            LOG_DEBUG("GamecardNSP: error: %s\n", e.what());
            fclose(fp);
            writer.close();
            throw;
        }

        fclose(fp);
    }
}
