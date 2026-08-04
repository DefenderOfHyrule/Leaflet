/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <cstring>
#include <sstream>
#include <filesystem>
#include <ctime>
#include <thread>
#include <memory>
#include "sdInstall.hpp"
#include "install/install_nsp.hpp"
#include "install/install_xci.hpp"
#include "install/sdmc_xci.hpp"
#include "install/sdmc_nsp.hpp"
#include "nx/fs.hpp"
#include "util/file_util.hpp"
#include "util/title_util.hpp"
#include "util/error.hpp"
#include "util/config.hpp"
#include "util/install_diagnostics.hpp"
#include "util/util.hpp"
#include "util/lang.hpp"
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace nspInstStuff {

    void installNspFromFile(std::vector<std::filesystem::path> ourTitleList, int whereToInstall)
    {
        inst::util::initInstallServices();
        inst::ui::instPage::loadInstallScreen();
        bool nspInstalled = true;
        NcmStorageId m_destStorageId = NcmStorageId_SdCard;
        std::string currentName;

        if (whereToInstall) m_destStorageId = NcmStorageId_BuiltInUser;
        unsigned int titleItr;
        inst::diag::StartSession("sd", ourTitleList.size());

        int successCount = 0;
        int failCount = 0;

        for (titleItr = 0; titleItr < ourTitleList.size(); titleItr++) {
            try {
                currentName = inst::util::shortenString(ourTitleList[titleItr].filename().string(), 40, true);
                inst::diag::NoteTransferReceived(currentName);
                inst::ui::instPage::setTopInstInfoText("inst.info_page.top_info0"_lang + currentName + "inst.sd.source_string"_lang);
                std::unique_ptr<leaf::install::Install> installTask;

                if (ourTitleList[titleItr].extension() == ".xci" || ourTitleList[titleItr].extension() == ".xcz") {
                    auto sdmcXCI = std::make_shared<leaf::install::xci::SDMCXCI>(ourTitleList[titleItr]);
                    installTask = std::make_unique<leaf::install::xci::XCIInstallTask>(m_destStorageId, inst::config::ignoreReqVers, sdmcXCI);
                } else {
                    auto sdmcNSP = std::make_shared<leaf::install::nsp::SDMCNSP>(ourTitleList[titleItr]);
                    installTask = std::make_unique<leaf::install::nsp::NSPInstall>(m_destStorageId, inst::config::ignoreReqVers, sdmcNSP);
                }

                LOG_DEBUG("%s\n", "Preparing installation");
                inst::ui::instPage::setInstInfoText("inst.info_page.preparing"_lang);
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
                        failCount++;
                        continue;
                    }
                    installTask->downgradeDetected = false;
                    installTask->m_skipDowngradeCheck = true;
                    installTask->Prepare();
                }

                const u64 titleId = installTask->GetTitleId(0);
                const NcmContentMetaType metaType = installTask->GetContentMetaType(0);
                const u64 baseTitleId = leaf::util::GetBaseTitleId(titleId, metaType);
                inst::ui::instPage::setInstallIconFromTitleId(baseTitleId);

                installTask->Begin();
                inst::diag::RecordSuccess(currentName);
                successCount++;
            }
            catch (std::exception& e)
            {
                LOG_DEBUG("Failed to install");
                LOG_DEBUG("%s", e.what());
                fprintf(stdout, "%s", e.what());
                const std::string failedName = currentName.empty()
                    ? (ourTitleList.empty() ? std::string("unknown item") : inst::util::shortenString(ourTitleList.front().filename().string(), 42, true))
                    : currentName;
                const auto failure = inst::diag::ClassifyFailure(e.what());
                inst::diag::RecordFailure(failedName, failure);
                inst::ui::instPage::setInstInfoText(failure.canceled || failure.skipItem ? "Installation canceled." : ("inst.info_page.failed"_lang + failedName));
                inst::ui::instPage::setInstBarPerc(0);
                if (!failure.canceled && !failure.skipItem) {
                    std::string audioPath = "romfs:/audio/bark.wav";
                    if (!inst::config::soundEnabled) audioPath = "";
                    if (std::filesystem::exists(inst::config::appDir + "/bark.wav")) audioPath = inst::config::appDir + "/bark.wav";
                    std::thread audioThread(inst::util::playAudio,audioPath);
                    inst::ui::mainApp->CreateShowDialog("inst.info_page.failed"_lang + failedName + "!", inst::diag::BuildUserMessage(failure), {"common.ok"_lang}, true);
                    audioThread.join();
                }
                nspInstalled = false;
                failCount++;

                if (failure.canceled) {
                    svcSleepThread(1500000000ULL);
                    break;
                }
                if (!failure.skipItem && inst::config::cancelQueueOnError) {
                    svcSleepThread(500000000ULL);
                    break;
                }
                svcSleepThread(failure.skipItem ? 0 : 500000000ULL);
            }
        }

        if(successCount > 0 && failCount == 0) {
            inst::ui::instPage::setInstInfoText("inst.info_page.complete"_lang);
            inst::ui::instPage::setInstBarPerc(100);
            std::string audioPath = "romfs:/audio/success.wav";
            if (!inst::config::soundEnabled) audioPath = "";
            if (std::filesystem::exists(inst::config::appDir + "/success.wav")) audioPath = inst::config::appDir + "/success.wav";
            std::thread audioThread(inst::util::playAudio,audioPath);
            if (ourTitleList.size() > 1) {
                if (inst::config::deletePrompt) {
                    if(inst::ui::mainApp->CreateShowDialog(std::to_string(ourTitleList.size()) + "inst.sd.delete_info_multi"_lang, "inst.sd.delete_desc"_lang, {"common.no"_lang,"common.yes"_lang}, false) == 1) {
                        for (long unsigned int i = 0; i < ourTitleList.size(); i++) {
                            if (std::filesystem::exists(ourTitleList[i])) {
                                try {
                                    std::filesystem::remove(ourTitleList[i]);
                                } catch (...){ };
                            }
                        }
                    }
                } else inst::ui::mainApp->CreateShowDialog(std::to_string(ourTitleList.size()) + "inst.info_page.desc0"_lang, Language::GetRandomMsg(), {"common.ok"_lang}, true);
            } else {
                if (inst::config::deletePrompt) {
                    if(inst::ui::mainApp->CreateShowDialog(inst::util::shortenString(ourTitleList[0].filename().string(), 32, true) + "inst.sd.delete_info"_lang, "inst.sd.delete_desc"_lang, {"common.no"_lang,"common.yes"_lang}, false) == 1) {
                        if (std::filesystem::exists(ourTitleList[0])) {
                            try {
                                std::filesystem::remove(ourTitleList[0]);
                            } catch (...){ };
                        }
                    }
                } else inst::ui::mainApp->CreateShowDialog(inst::util::shortenString(ourTitleList[0].filename().string(), 42, true) + "inst.info_page.desc1"_lang, Language::GetRandomMsg(), {"common.ok"_lang}, true);
            }
            audioThread.join();
        }

        LOG_DEBUG("Done");
        inst::ui::instPage::loadMainMenu();
        inst::util::deinitInstallServices();
        return;
    }
}
