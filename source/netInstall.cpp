#include <string>
#include <thread>
#include <malloc.h>
#include <algorithm>
#include "netInstall.hpp"
#include "install/net_nsp.hpp"
#include "install/net_xci.hpp"
#include "install/install_nsp.hpp"
#include "install/install_xci.hpp"
#include "quark/quark_net.hpp"
#include "util/error.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/install_diagnostics.hpp"
#include "util/lang.hpp"
#include "util/title_util.hpp"
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace netInstStuff {

    void installTitleNet(std::vector<std::string> ourTitleList, int ourStorage)
    {
        inst::util::initInstallServices();
        inst::ui::instPage::loadInstallScreen();

        bool nspInstalled        = true;
        NcmStorageId destStorage = ourStorage ? NcmStorageId_BuiltInUser : NcmStorageId_SdCard;
        std::string currentName;

        inst::diag::StartSession("quark_net", ourTitleList.size());

        std::vector<std::string> fileNames;
        fileNames.reserve(ourTitleList.size());
        for (const auto &path : ourTitleList)
            fileNames.push_back(inst::util::shortenString(inst::util::formatUrlString(path), 40, true));

        unsigned int fileItr = 0;
        try {
            for (fileItr = 0; fileItr < ourTitleList.size(); fileItr++) {
                currentName = fileNames[fileItr];
                inst::diag::NoteTransferReceived(currentName);
                inst::ui::instPage::setTopInstInfoText(
                    "inst.info_page.top_info0"_lang + currentName + "inst.net.source_string"_lang);

                std::unique_ptr<leaf::install::Install> installTask;
                const std::string &remotePath = ourTitleList[fileItr];

                if (remotePath.size() >= 3 && remotePath.compare(remotePath.size() - 3, 2, "xc") == 0) {
                    auto xci = std::make_shared<quark::net::install::NetXCI>(remotePath);
                    installTask = std::make_unique<leaf::install::xci::XCIInstallTask>(
                        destStorage, inst::config::ignoreReqVers, xci);
                } else {
                    auto nsp = std::make_shared<quark::net::install::NetNSP>(remotePath);
                    installTask = std::make_unique<leaf::install::nsp::NSPInstall>(
                        destStorage, inst::config::ignoreReqVers, nsp);
                }

                inst::ui::instPage::setInstInfoText("Transfer received. Install started...");
                inst::ui::instPage::setInstBarPerc(0);
                inst::diag::NoteInstallStarted(currentName);
                inst::ui::instPage::clearInstallIcon();
                installTask->Prepare();
                if (installTask->downgradeDetected) {
                    const int choice = inst::ui::mainApp->CreateShowDialog(
                        "inst.downgrade.title"_lang,
                        "inst.downgrade.desc"_lang,
                        {"inst.downgrade.proceed"_lang, "common.cancel"_lang}, false);
                    if (choice != 0) {
                        nspInstalled = false;
                        break;
                    }
                    installTask->downgradeDetected = false;
                    installTask->m_skipDowngradeCheck = true;
                    installTask->Prepare();
                }
                inst::ui::instPage::setInstallIconFromTitleId(
                    leaf::util::GetBaseTitleId(installTask->GetTitleId(0), installTask->GetContentMetaType(0)));
                installTask->Begin();
                inst::diag::RecordSuccess(currentName);
                inst::ui::instPage::setInstInfoText("Install succeeded: " + currentName);
            }
        }
        catch (std::exception &e) {
            LOG_DEBUG("Failed to install\n");
            LOG_DEBUG("%s", e.what());
            fprintf(stdout, "%s", e.what());

            const std::string failedName = currentName.empty()
                ? (fileNames.empty() ? std::string("unknown item") : fileNames.front())
                : currentName;
            const auto failure = inst::diag::ClassifyFailure(e.what());
            inst::diag::RecordFailure(failedName, failure);
            inst::ui::instPage::setInstInfoText(
                (failure.canceled || failure.skipItem) ? "Installation canceled."
                                 : ("inst.info_page.failed"_lang + failedName));
            inst::ui::instPage::setInstBarPerc(0);

            if (!failure.canceled && !failure.skipItem) {
                std::string audioPath = "romfs:/audio/bark.wav";
                if (!inst::config::soundEnabled) audioPath = "";
                if (std::filesystem::exists(inst::config::appDir + "/bark.wav"))
                    audioPath = inst::config::appDir + "/bark.wav";
                std::thread audio(inst::util::playAudio, audioPath);
                inst::ui::mainApp->CreateShowDialog(
                    "inst.info_page.failed"_lang + failedName + "!",
                    inst::diag::BuildUserMessage(failure), {"common.ok"_lang}, true);
                audio.join();
            }
            nspInstalled = false;
            if (failure.canceled) {
                svcSleepThread(1500000000ULL);
            } else if (!failure.skipItem && inst::config::cancelQueueOnError) {
                svcSleepThread(500000000ULL);
            }
        }
        
        if (nspInstalled) {
            inst::ui::instPage::setInstInfoText("inst.info_page.complete"_lang);
            inst::ui::instPage::setInstBarPerc(100);

            std::string audioPath = "romfs:/audio/success.wav";
            if (!inst::config::soundEnabled) audioPath = "";
            if (std::filesystem::exists(inst::config::appDir + "/success.wav"))
                audioPath = inst::config::appDir + "/success.wav";
            std::thread audio(inst::util::playAudio, audioPath);

            if (ourTitleList.size() > 1)
                inst::ui::mainApp->CreateShowDialog(
                    std::to_string(ourTitleList.size()) + "inst.info_page.desc0"_lang,
                    Language::GetRandomMsg(), {"common.ok"_lang}, true);
            else
                inst::ui::mainApp->CreateShowDialog(
                    fileNames[0] + "inst.info_page.desc1"_lang,
                    Language::GetRandomMsg(), {"common.ok"_lang}, true);
            audio.join();
        }

        LOG_DEBUG("Done\n");
        inst::ui::instPage::loadMainMenu();
        inst::util::deinitInstallServices();
    }

}
