#include "install/net_xci.hpp"
#include "install/hfs0.hpp"
#include "quark/quark_net_cmd.hpp"
#include "data/buffered_placeholder_writer.hpp"
#include "util/error.hpp"
#include "util/lang.hpp"
#include "ui/MainApplication.hpp"

namespace inst::ui { extern MainApplication *mainApp; }
#include "ui/instPage.hpp"

#include <switch.h>
#include <malloc.h>
#include <threads.h>
#include <algorithm>
#include <string>

namespace quark::net::install {

    namespace {

        constexpr size_t kReadBufSize = 0x800000;

        struct ThreadArgs {
            std::string                            remotePath;
            leaf::data::BufferedPlaceholderWriter *writer;
            u64                                    hfs0Offset;
            u64                                    ncaSize;
        };

        bool        g_stopThreadsXci = false;
        std::string g_errorMessageXci;

        int ReadThreadFunc(void *raw) {
            auto *args = static_cast<ThreadArgs*>(raw);

            if (!quark::net::cmd::StartFile(args->remotePath)) {
                g_stopThreadsXci  = true;
                g_errorMessageXci = "net: failed to open remote file";
                return 0;
            }

            u8 *buf       = static_cast<u8*>(memalign(0x1000, kReadBufSize));
            u64 remaining = args->ncaSize;
            u64 offset    = args->hfs0Offset;

            try {
                while (remaining && !g_stopThreadsXci) {
                    const u64 chunkSize = std::min(remaining, (u64)kReadBufSize);
                    u64 read = 0;
                    if (!quark::net::cmd::ReadFile(args->remotePath, offset, chunkSize, read, buf) || read == 0) {
                        if (inst::ui::instPage::isInstallCancelRequested())
                            THROW_FORMAT("Installation canceled.");
                        THROW_FORMAT(("inst.usb.error"_lang).c_str());
                    }
                    remaining -= read;
                    offset    += read;
                    while (!args->writer->CanAppendData(read)) { svcSleepThread(0); }
                    args->writer->AppendData(buf, read);
                }
            } catch (std::exception &e) {
                g_stopThreadsXci  = true;
                g_errorMessageXci = e.what();
            }

            free(buf);
            quark::net::cmd::EndFile();
            return 0;
        }

        int WriteThreadFunc(void *raw) {
            auto *args = static_cast<ThreadArgs*>(raw);
            while (!args->writer->IsPlaceholderComplete() && !g_stopThreadsXci) {
                if (args->writer->CanWriteSegmentToPlaceholder())
                    args->writer->WriteSegmentToPlaceholder();
                else
                    svcSleepThread(0);
            }
            return 0;
        }

    }

    NetXCI::NetXCI(std::string remotePath)
        : m_remotePath(std::move(remotePath)) {}

    void NetXCI::StreamToPlaceholder(
        std::shared_ptr<nx::ncm::ContentStorage> &contentStorage,
        NcmContentId placeholderId)
    {
        const leaf::install::HFS0FileEntry *entry   = this->GetFileEntryByNcaId(placeholderId);
        const std::string                         ncaName = this->GetFileEntryName(entry);
        const size_t                              ncaSize = entry->fileSize;

        leaf::data::BufferedPlaceholderWriter writer(contentStorage, placeholderId, ncaSize);

        ThreadArgs args{
            .remotePath  = m_remotePath,
            .writer      = &writer,
            .hfs0Offset  = this->GetDataOffset() + entry->dataOffset,
            .ncaSize     = ncaSize,
        };

        g_stopThreadsXci = false;
        g_errorMessageXci.clear();

        thrd_t readThrd, writeThrd;
        thrd_create(&readThrd,  ReadThreadFunc,  &args);
        thrd_create(&writeThrd, WriteThreadFunc, &args);

        const u64 freq      = armGetSystemTickFreq();
        u64 startTime       = armGetSystemTick();
        size_t startBufSize = 0;

        inst::ui::instPage::setInstBarPerc(0);
        while (!writer.IsBufferDataComplete() && !g_stopThreadsXci) {
            svcSleepThread(0);
            if (inst::ui::instPage::isInstallCancelRequested()) {
                g_stopThreadsXci  = true;
                g_errorMessageXci = "Installation canceled.";
                break;
            }
            if (!inst::ui::instPage::isInstallCancelRequested()) {
                inst::ui::mainApp->UpdateButtons();
                if (inst::ui::mainApp->GetButtonsHeld() & HidNpadButton_B) {
                    const int choice = inst::ui::mainApp->CreateShowDialog(
                        "Cancel install?",
                        "Stop the current install and clean up partial data?",
                        {"Cancel Install", "Go Back"}, false);
                    if (choice == 0) {
                        inst::ui::instPage::requestInstallCancel();
                        g_stopThreadsXci  = true;
                        g_errorMessageXci = "Installation canceled.";
                        break;
                    }
                }
            }
            const u64 now = armGetSystemTick();
            if (now - startTime >= freq) {
                const size_t newBuf = writer.GetSizeBuffered();
                const double speed  = ((newBuf - startBufSize) / 1000000.0) /
                                      ((double)(now - startTime) / (double)freq);
                const int pct = (int)((double)newBuf / (double)writer.GetTotalDataSize() * 100.0);
                startTime    = now;
                startBufSize = newBuf;
                inst::ui::instPage::setInstInfoText(
                    "inst.info_page.downloading"_lang + ncaName +
                    "inst.info_page.at"_lang +
                    std::to_string(speed).substr(0, std::to_string(speed).size() - 4) + "MB/s");
                inst::ui::instPage::setInstBarPerc((double)pct);
            }
        }
        inst::ui::instPage::setInstBarPerc(100);

        inst::ui::instPage::setInstInfoText("inst.info_page.top_info0"_lang + ncaName + "...");
        inst::ui::instPage::setInstBarPerc(0);
        while (!writer.IsPlaceholderComplete() && !g_stopThreadsXci) {
            svcSleepThread(0);
            if (inst::ui::instPage::isInstallCancelRequested()) {
                g_stopThreadsXci  = true;
                g_errorMessageXci = "Installation canceled.";
                break;
            }
            const int pct = (int)((double)writer.GetSizeWrittenToPlaceholder() /
                                  (double)writer.GetTotalDataSize() * 100.0);
            inst::ui::instPage::setInstBarPerc((double)pct);
        }
        inst::ui::instPage::setInstBarPerc(100);

        thrd_join(readThrd,  nullptr);
        thrd_join(writeThrd, nullptr);

        if (g_stopThreadsXci)
            throw std::runtime_error(g_errorMessageXci.c_str());
    }

    void NetXCI::BufferData(void *buf, off_t offset, size_t size) {
        if (inst::ui::instPage::isInstallCancelRequested())
            THROW_FORMAT("Installation canceled.");

        if (!quark::net::cmd::StartFile(m_remotePath))
            THROW_FORMAT(("inst.usb.error"_lang).c_str());

        u64 read = 0;
        const bool ok = quark::net::cmd::ReadFile(m_remotePath,
                                                  static_cast<u64>(offset),
                                                  static_cast<u64>(size),
                                                  read, buf);
        quark::net::cmd::EndFile();

        if (!ok || read != static_cast<u64>(size)) {
            if (inst::ui::instPage::isInstallCancelRequested())
                THROW_FORMAT("Installation canceled.");
            THROW_FORMAT(("inst.usb.error"_lang).c_str());
        }
    }

}
