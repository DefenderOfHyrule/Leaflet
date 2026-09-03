#include <algorithm>
#include <filesystem>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <switch.h>
#include "ui/MainApplication.hpp"
#include "ui/mainPage.hpp"
#include "ui/instPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/error.hpp"
#include "util/lang.hpp"
#include "data/buffered_placeholder_writer.hpp"
#include "gcInstall.hpp"
#include "ui/fileBrowserPage.hpp"
#include "nx/usbhdd.h"
#include "ui/bottomHint.hpp"
#include <pu/ui/extras/extras_Toast.hpp>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;
    bool appletFinished = false;

    constexpr int kMainCardCount = 6;
    constexpr int kCardWNormal = 184;
    constexpr int kCardHNormal = 170;
    constexpr int kCardWSelected = 232;
    constexpr int kCardHSelected = 210;
    constexpr int kCardGap = 14;
    constexpr int kCardCenterY = 136;
    constexpr int kCardRadius = 16;
    constexpr int kRingPadding = 5;
    constexpr int kCardLabelFontSize = 18;
    constexpr int kCardLabelLines = 2;
    constexpr int kPaneX = 90;
    constexpr int kPaneY = 272;
    constexpr int kPaneW = 1100;
    constexpr int kPaneH = 340;
    constexpr int kPaneRadius = 18;
    constexpr int kPanePadX = 34;
    constexpr int kPaneTitleFontSize = 30;
    constexpr int kPaneDescFontSize = 22;
    constexpr int kPaneDescLineCount = 6;
    constexpr int kPaneDescLineSpacing = 34;
    constexpr float kCardAnimSpeed = 0.35f;

    static pu::ui::Color ShadeColor(pu::ui::Color c, float factor) {
        auto scale = [factor](u8 v) -> u8 {
            float out = static_cast<float>(v) * factor;
            if (out < 0.0f) out = 0.0f;
            if (out > 255.0f) out = 255.0f;
            return static_cast<u8>(out);
        };
        return pu::ui::Color(scale(c.R), scale(c.G), scale(c.B), c.A);
    }

    static pu::ui::Color WithAlpha(pu::ui::Color c, u8 a) {
        return pu::ui::Color(c.R, c.G, c.B, a);
    }

    static pu::ui::Color WithMinAlpha(pu::ui::Color c, u8 a) {
        return (c.A < a) ? WithAlpha(c, a) : c;
    }

    static bool IsLightColor(pu::ui::Color c) {
        const float lum = (0.299f * c.R) + (0.587f * c.G) + (0.114f * c.B);
        return lum > 150.0f;
    }

    static pu::ui::Color TextColorFor(pu::ui::Color surface) {
        return IsLightColor(surface) ? pu::ui::Color(26, 26, 26, 255) : pu::ui::Color(255, 255, 255, 255);
    }

    std::string WrapGridLabelText(const std::string& text, int maxWidth, int fontSize, int maxLines)
    {
        if (text.empty() || maxWidth <= 0 || maxLines <= 0)
            return text;

        auto measure = pu::ui::elm::TextBlock::New(0, 0, "", fontSize);
        std::stringstream words(text);
        std::string word;
        std::vector<std::string> lines;
        std::string line;

        while (words >> word) {
            std::string candidate = line.empty() ? word : (line + " " + word);
            measure->SetText(candidate);
            if (measure->GetTextWidth() <= maxWidth) {
                line = candidate;
                continue;
            }

            if (!line.empty()) {
                lines.push_back(line);
                line.clear();
                if (static_cast<int>(lines.size()) >= maxLines)
                    break;
            }

            measure->SetText(word);
            if (measure->GetTextWidth() <= maxWidth) {
                line = word;
                continue;
            }

            std::string trimmed = word;
            while (!trimmed.empty()) {
                std::string candidateToken = trimmed + "...";
                measure->SetText(candidateToken);
                if (measure->GetTextWidth() <= maxWidth)
                    break;
                trimmed.pop_back();
            }
            line = trimmed.empty() ? "..." : (trimmed + "...");
            lines.push_back(line);
            line.clear();
            break;
        }

        if (!line.empty() && static_cast<int>(lines.size()) < maxLines)
            lines.push_back(line);

        if (lines.empty())
            lines.push_back(text);

        if (static_cast<int>(lines.size()) > maxLines)
            lines.resize(maxLines);

        if (static_cast<int>(lines.size()) == maxLines) {
            std::string& last = lines.back();
            measure->SetText(last);
            if (measure->GetTextWidth() > maxWidth) {
                std::string trimmed = last;
                while (!trimmed.empty()) {
                    std::string candidateToken = trimmed + "...";
                    measure->SetText(candidateToken);
                    if (measure->GetTextWidth() <= maxWidth)
                        break;
                    trimmed.pop_back();
                }
                last = trimmed.empty() ? "..." : (trimmed + "...");
            }
        }

        std::string wrapped;
        for (std::size_t i = 0; i < lines.size(); i++) {
            if (i > 0)
                wrapped += "\n";
            wrapped += lines[i];
        }
        return wrapped;
    }

    void mainMenuThread() {
        bool menuLoaded = mainApp->IsShown();
        if (!appletFinished && appletGetAppletType() == AppletType_LibraryApplet) {
            leaf::data::NUM_BUFFER_SEGMENTS = 2;
            if (menuLoaded) {
                inst::ui::appletFinished = true;
                mainApp->CreateShowDialog("main.applet.title"_lang, "main.applet.desc"_lang, {"common.ok"_lang}, true);
            }
        } else if (!appletFinished) {
            inst::ui::appletFinished = true;
            leaf::data::NUM_BUFFER_SEGMENTS = 4;
        }
    }

    MainPage::MainPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::config::colorBackground));
        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else this->SetBackgroundImage("romfs:/images/background.jpg");
        const auto botColor = COLOR(inst::config::colorBotBar);
        this->botRect = Rectangle::New(0, 660, 1280, 60, botColor);
        this->appVersionText = TextBlock::New(0, 681, "v" + inst::config::appVersion + (appletGetAppletType() == AppletType_LibraryApplet ? " | Applet Mode" : ""), 18);
        this->appVersionText->SetColor(WithAlpha(TextColorFor(botColor), 0xC8));
        this->appVersionText->SetX(1280 - 10 - this->appVersionText->GetTextWidth());
        const std::string mainButtonsText = "main.buttons"_lang;
        this->butText = TextBlock::New(10, 678, mainButtonsText, 20);
        this->butText->SetColor(COLOR("#FFFFFFFF"));
        this->bottomHintSegments = BuildBottomHintSegments(mainButtonsText, 10, 20);

        const std::vector<std::string> gridLabels = {
            "main.menu.sd"_lang,
            "main.menu.hdd"_lang,
            "main.menu.gc"_lang,
            "main.menu.usb"_lang,
            "main.menu.set"_lang,
            "main.menu.browser"_lang
        };
        const std::vector<std::string> gridIcons = {
            "romfs:/images/icons/micro-sd.png",
            "romfs:/images/icons/usb-install.png",
            "romfs:/images/icons/gamecard-inserted.png",
            "romfs:/images/icons/quark.png",
            "romfs:/images/icons/settings.png",
            "romfs:/images/icons/folder.png"
        };

        pu::ui::Color cardBase;
        pu::ui::Color ringColor;
        pu::ui::Color paneColor;
        {
            const std::string& baseHex = (inst::config::colorTileBase.size() == 9)
                ? inst::config::colorTileBase : inst::config::colorTopBar;
            cardBase = COLOR(baseHex);
            ringColor = WithAlpha(COLOR(inst::config::colorTileHighlight), 0xFF);
            paneColor = WithMinAlpha(ShadeColor(cardBase, 0.65f), 0xC8);
        }
        this->cardColorNormal = WithMinAlpha(ShadeColor(cardBase, 0.85f), 0xC0);
        this->cardColorSelected = WithMinAlpha(ShadeColor(cardBase, 1.25f), 0xD9);
        this->cardTextColor = TextColorFor(this->cardColorNormal);
        this->paneTextColor = TextColorFor(paneColor);
        this->paneTextDimColor = WithAlpha(this->paneTextColor, 0xB4);

        this->mainCardRing = Rectangle::New(0, 0, kCardWSelected + (kRingPadding * 2),
            kCardHSelected + (kRingPadding * 2), ringColor, kCardRadius + 4);

        this->mainCards.reserve(kMainCardCount);
        this->mainCardIcons.reserve(kMainCardCount);
        this->mainCardLabels.reserve(kMainCardCount * kCardLabelLines);
        for (int i = 0; i < kMainCardCount; i++) {
            auto card = Rectangle::New(0, 0, kCardWNormal, kCardHNormal, this->cardColorNormal, kCardRadius);
            auto icon = Image::New(0, 0, gridIcons[i]);
            icon->SetWidth(64);
            icon->SetHeight(64);
            for (int line = 0; line < kCardLabelLines; line++) {
                auto label = TextBlock::New(0, 0, "", kCardLabelFontSize);
                label->SetColor(this->cardTextColor);
                this->mainCardLabels.push_back(label);
            }
            this->cardLabelNormal[i] = WrapGridLabelText(gridLabels[i], kCardWNormal - 16, kCardLabelFontSize, kCardLabelLines);
            this->cardLabelSelected[i] = WrapGridLabelText(gridLabels[i], kCardWSelected - 20, kCardLabelFontSize, kCardLabelLines);
            this->mainCards.push_back(card);
            this->mainCardIcons.push_back(icon);
            this->cardAnim[i] = (i == this->selectedMainIndex) ? 1.0f : 0.0f;
        }

        this->detailPane = Rectangle::New(kPaneX, kPaneY, kPaneW, kPaneH, paneColor, kPaneRadius);
        this->detailTitle = TextBlock::New(kPaneX + kPanePadX, kPaneY + 30, "", kPaneTitleFontSize);
        this->detailTitle->SetColor(this->paneTextColor);
        this->detailAccent = Rectangle::New(kPaneX + kPanePadX, kPaneY + 88, 52, 5, ringColor, 2);
        this->detailLines.reserve(kPaneDescLineCount);
        for (int i = 0; i < kPaneDescLineCount; i++) {
            auto line = TextBlock::New(kPaneX + kPanePadX, kPaneY + 108 + (i * kPaneDescLineSpacing), "", kPaneDescFontSize);
            line->SetColor(this->paneTextDimColor);
            this->detailLines.push_back(line);
        }

        this->Add(this->botRect);
        this->Add(this->appVersionText);
        this->Add(this->butText);
        this->Add(this->detailPane);
        this->Add(this->detailTitle);
        this->Add(this->detailAccent);
        for (auto& line : this->detailLines)
            this->Add(line);
        this->Add(this->mainCardRing);
        for (auto& card : this->mainCards)
            this->Add(card);
        for (auto& icon : this->mainCardIcons)
            this->Add(icon);
        for (auto& label : this->mainCardLabels)
            this->Add(label);
        this->updateMainGridSelection();
        this->layoutCards();
        this->AddThread(mainMenuThread);
        this->AddThread([this]() {
            bool dirty = false;
            for (int i = 0; i < kMainCardCount; i++) {
                const float target = (i == this->selectedMainIndex) ? 1.0f : 0.0f;
                const float diff = target - this->cardAnim[i];
                if (diff > 0.02f || diff < -0.02f) {
                    this->cardAnim[i] += diff * kCardAnimSpeed;
                    dirty = true;
                } else if (this->cardAnim[i] != target) {
                    this->cardAnim[i] = target;
                    dirty = true;
                }
            }
            if (dirty)
                this->layoutCards();
        });
        this->AddThread([this]() {
            if (!this->m_showUpdateToast.load()) return;
            this->m_showUpdateToast.store(false);
            if (this->m_pendingUpdateInfo.empty()) return;
            const std::string version = this->m_pendingUpdateInfo[0];
            const std::string msg = version + " is available - press \uE0E0 to update Leaflet";
            this->m_updateToast = pu::ui::extras::Toast::New(msg, 18,
                pu::ui::Color(255, 255, 255, 255),
                pu::ui::Color(30, 30, 60, 230));
            mainApp->StartOverlayWithTimeout(this->m_updateToast, 6000);

            this->m_toastExpiry = std::chrono::steady_clock::now() + std::chrono::milliseconds(6000);
        });

        this->AddThread([this]() {
            if (!this->m_triggerUpdate.load()) return;
            this->m_triggerUpdate.store(false);
            if (this->m_isSimulated) {
                const std::string version = this->m_pendingUpdateInfo.empty()
                    ? std::string() : this->m_pendingUpdateInfo[0];
                const std::string notes = this->m_pendingUpdateInfo.size() > 2
                    ? this->m_pendingUpdateInfo[2] : "No changelog available.";
                while (true) {
                    int choice = mainApp->CreateShowDialog(
                        "options.update.title"_lang,
                        "options.update.desc0"_lang + version + "options.update.desc1"_lang,
                        {"options.update.opt0"_lang, "View Changelog", "common.cancel"_lang}, false);
                    if (choice == 1) {
                        mainApp->CreateShowDialog(
                            "Changelog " + version,
                            notes,
                            {"common.ok"_lang}, false);
                        continue;
                    }
                    if (choice == 0) {
                        auto notice = pu::ui::extras::Toast::New(
                            "This is a simulated environment - no update will be performed",
                            18,
                            pu::ui::Color(255, 255, 255, 255),
                            pu::ui::Color(60, 20, 20, 230));
                        mainApp->StartOverlayWithTimeout(notice, 4000);
                    }
                    break;
                }
                return;
            }
            std::vector<std::string> info = this->m_pendingUpdateInfo;
            this->m_pendingUpdateInfo.clear();
            inst::ui::optionsPage::askToUpdate(info);
        });

        const std::vector<std::string>& cached = inst::util::getCachedUpdateInfo();
        if (!cached.empty()) {
            m_isSimulated = false;
            showUpdateBanner(cached);
        }

        this->AddThread([this]() {
            if (this->m_pendingUpdateInfo.empty() && !this->m_showUpdateToast.load()) {
                const std::vector<std::string>& info = inst::util::getCachedUpdateInfo();
                if (!info.empty()) {
                    m_isSimulated = false;
                    showUpdateBanner(info);
                }
            }
        });
    }

    MainPage::~MainPage() {}

    void MainPage::showUpdateBanner(const std::vector<std::string>& updateInfo) {
        this->m_pendingUpdateInfo = updateInfo;
        this->m_showUpdateToast.store(true);
    }

    void MainPage::simulateUpdate(const std::string& fakeVersion) {
        this->m_isSimulated = true;
        this->m_pendingUpdateInfo = {fakeVersion, "", ""};
        this->m_showUpdateToast.store(true);
    }

    static bool warnIfPatchesMissing() {
        if (inst::config::sigPatchCheckDisabled) return true;

        if (inst::util::isFsPatchLogStale()) {
            mainApp->CreateShowDialog(
                "fs-patch removed from SD card",
                "A stale fs-patch log was found in sd:/config/fs-patch/log.ini,\nbut the sysmodule is no longer present in sd:/atmosphere/contents.\n\nThe patches from the previous boot are no longer active in memory.\nInstalling content will fail.\n\nReinstall fs-patch and reboot, then try again.",
                {"common.ok"_lang},
                true
            );
            return false;
        }
        if (!inst::util::checkSigPatches()) {
            mainApp->CreateShowDialog(
                "options.sigpatch_check.blocked.title"_lang,
                "options.sigpatch_check.blocked.desc"_lang,
                {"common.ok"_lang},
                true
            );
            return false;
        }
        return true;
    }

    static bool warnIfSysmmc() {
        if (inst::config::emummcSafetyDisabled) return true;

        const auto status = inst::util::getEmuMmcCheckResult();
        if (status == inst::util::EmuMmcCheckResult::OnEmuMmc) return true;

        if (status == inst::util::EmuMmcCheckResult::Undetermined) {
            mainApp->CreateShowDialog(
                "options.emummc_check.undetermined.title"_lang,
                "options.emummc_check.undetermined.desc"_lang,
                {"common.ok"_lang},
                true
            );
            return false;
        }

        mainApp->CreateShowDialog(
            "options.emummc_check.blocked.title"_lang,
            "options.emummc_check.blocked.desc"_lang,
            {"common.ok"_lang},
            true
        );
        return false;
    }

    void MainPage::installMenuItem_Click() {
        if (!warnIfSysmmc()) return;
        if (!warnIfPatchesMissing()) return;
        mainApp->sdinstPage->drawMenuItems(true, "sdmc:/");
        mainApp->sdinstPage->menu->SetSelectedIndex(0);
        mainApp->LoadLayout(mainApp->sdinstPage);
    }

    void MainPage::usbInstallMenuItem_Click() {
        if (!warnIfSysmmc()) return;
        if (!warnIfPatchesMissing()) return;
        int choice = mainApp->CreateShowDialog(
            "main.quark.choose.title"_lang,
            "main.quark.choose.desc"_lang,
            {"main.quark.choose.usb"_lang, "main.quark.choose.net"_lang, "common.cancel"_lang},
            true);
        if (choice == 0) {
            if (!inst::config::usbAck) {
                if (mainApp->CreateShowDialog("main.usb.warn.title"_lang, "main.usb.warn.desc"_lang, {"common.ok"_lang, "main.usb.warn.opt1"_lang}, false) == 1) {
                    inst::config::usbAck = true;
                    inst::config::setConfig();
                }
            }
            if (inst::util::usbIsConnected()) mainApp->usbinstPage->startUsb();
            else mainApp->CreateShowDialog("main.usb.error.title"_lang, "main.usb.error.desc"_lang, {"common.ok"_lang}, false);
        } else if (choice == 1) {
            mainApp->netinstPage->startNet();
        }
    }

    void MainPage::netInstallMenuItem_Click() {
        if (!warnIfSysmmc()) return;
        if (!warnIfPatchesMissing()) return;
        mainApp->netinstPage->startNet();
    }

    void MainPage::hddInstallMenuItem_Click() {
        if (!warnIfSysmmc()) return;
        if (!warnIfPatchesMissing()) return;
        if (nx::hdd::count() && nx::hdd::rootPath()) {
            mainApp->hddinstPage->drawMenuItems(true, nx::hdd::rootPath());
            mainApp->hddinstPage->menu->SetSelectedIndex(0);
            mainApp->LoadLayout(mainApp->hddinstPage);
        } else {
            mainApp->CreateShowDialog("main.hdd.title"_lang, "main.hdd.notfound"_lang, {"common.ok"_lang}, true);
        }
    }

    void MainPage::gcInstallMenuItem_Click() {
        if (!warnIfSysmmc()) return;
        if (!warnIfPatchesMissing()) return;

        if (!inst::gc::Init()) {
            mainApp->CreateShowDialog("Gamecard Error",
                "Failed to initialize the gamecard reader.\nThe device operator could not be opened.",
                {"common.ok"_lang}, true);
            return;
        }

        mainApp->LoadLayout(mainApp->gcinstPage);
    }

    void MainPage::exitMenuItem_Click() {
        mainApp->FadeOut();
        mainApp->Close();
    }

    void MainPage::settingsMenuItem_Click() {
        mainApp->LoadLayout(mainApp->optionspage);
    }

    void MainPage::updateMainGridSelection() {
        if (this->selectedMainIndex < 0)
            this->selectedMainIndex = 0;
        const int maxIndex = kMainCardCount - 1;
        if (this->selectedMainIndex > maxIndex)
            this->selectedMainIndex = maxIndex;
        for (int i = 0; i < kMainCardCount; i++) {
            const bool selected = (i == this->selectedMainIndex);
            this->mainCards[i]->SetColor(selected ? this->cardColorSelected : this->cardColorNormal);
            const std::string& wrapped = selected ? this->cardLabelSelected[i] : this->cardLabelNormal[i];
            const std::size_t split = wrapped.find('\n');
            const std::string line1 = (split == std::string::npos) ? wrapped : wrapped.substr(0, split);
            const std::string line2 = (split == std::string::npos) ? "" : wrapped.substr(split + 1);
            this->cardLabelHasTwoLines[i] = !line2.empty();
            const auto labelColor = selected ? this->cardTextColor : WithAlpha(this->cardTextColor, 0xC8);
            this->mainCardLabels[(i * kCardLabelLines) + 0]->SetText(line1);
            this->mainCardLabels[(i * kCardLabelLines) + 0]->SetColor(labelColor);
            this->mainCardLabels[(i * kCardLabelLines) + 1]->SetText(line2);
            this->mainCardLabels[(i * kCardLabelLines) + 1]->SetColor(labelColor);
        }
        this->refreshDetailPane();
        this->layoutCards();
    }

    void MainPage::layoutCards() {
        float total = static_cast<float>((kMainCardCount - 1) * kCardGap);
        for (int i = 0; i < kMainCardCount; i++)
            total += static_cast<float>(kCardWNormal) + (this->cardAnim[i] * static_cast<float>(kCardWSelected - kCardWNormal));
        float cursor = (1280.0f - total) / 2.0f;
        for (int i = 0; i < kMainCardCount; i++) {
            const float fw = static_cast<float>(kCardWNormal) + (this->cardAnim[i] * static_cast<float>(kCardWSelected - kCardWNormal));
            const int w = static_cast<int>(fw + 0.5f);
            const int h = kCardHNormal + static_cast<int>((this->cardAnim[i] * static_cast<float>(kCardHSelected - kCardHNormal)) + 0.5f);
            const int x = static_cast<int>(cursor + 0.5f);
            const int y = kCardCenterY - (h / 2);
            this->cardLayoutX[i] = x;
            this->cardLayoutY[i] = y;
            this->cardLayoutW[i] = w;
            this->cardLayoutH[i] = h;
            this->mainCards[i]->SetX(x);
            this->mainCards[i]->SetY(y);
            this->mainCards[i]->SetWidth(w);
            this->mainCards[i]->SetHeight(h);
            const int iconSize = (h * 40) / 100;
            this->mainCardIcons[i]->SetWidth(iconSize);
            this->mainCardIcons[i]->SetHeight(iconSize);
            this->mainCardIcons[i]->SetX(x + ((w - iconSize) / 2));
            this->mainCardIcons[i]->SetY(y + ((h * 10) / 100));
            auto& label1 = this->mainCardLabels[(i * kCardLabelLines) + 0];
            auto& label2 = this->mainCardLabels[(i * kCardLabelLines) + 1];
            label1->SetX(x + ((w - label1->GetTextWidth()) / 2));
            label2->SetX(x + ((w - label2->GetTextWidth()) / 2));
            if (this->cardLabelHasTwoLines[i]) {
                label1->SetY(y + h - 58);
                label2->SetY(y + h - 34);
            } else {
                label1->SetY(y + h - 40);
                label2->SetY(y + h - 34);
            }
            if (i == this->selectedMainIndex) {
                this->mainCardRing->SetX(x - kRingPadding);
                this->mainCardRing->SetY(y - kRingPadding);
                this->mainCardRing->SetWidth(w + (kRingPadding * 2));
                this->mainCardRing->SetHeight(h + (kRingPadding * 2));
            }
            cursor += fw + static_cast<float>(kCardGap);
        }
    }

    static void GetMainItemInfo(int index, std::string& title, std::string& desc) {
        switch (index) {
            case 0: title = "main.menu.sd"_lang;      desc = "main.info.sd"_lang;      break;
            case 1: title = "main.menu.hdd"_lang;     desc = "main.info.hdd"_lang;     break;
            case 2: title = "main.menu.gc"_lang;      desc = "main.info.gc"_lang;      break;
            case 3: title = "main.menu.usb"_lang;     desc = "main.info.usb"_lang;     break;
            case 4: title = "main.menu.set"_lang;     desc = "main.info.set"_lang;     break;
            case 5: title = "main.menu.browser"_lang; desc = "main.info.browser"_lang; break;
            default: title.clear(); desc.clear(); break;
        }
    }

    void MainPage::refreshDetailPane() {
        std::string title;
        std::string desc;
        GetMainItemInfo(this->selectedMainIndex, title, desc);
        this->detailTitle->SetText(title);
        const std::string wrapped = WrapGridLabelText(desc, kPaneW - (kPanePadX * 2), kPaneDescFontSize, kPaneDescLineCount);
        std::stringstream stream(wrapped);
        std::string lineText;
        std::size_t lineIndex = 0;
        while (lineIndex < this->detailLines.size() && std::getline(stream, lineText)) {
            this->detailLines[lineIndex]->SetText(lineText);
            lineIndex++;
        }
        for (; lineIndex < this->detailLines.size(); lineIndex++)
            this->detailLines[lineIndex]->SetText("");
    }

    int MainPage::getMainGridIndexFromTouch(int x, int y) const {
        for (int i = 0; i < kMainCardCount; i++) {
            const int tx = this->cardLayoutX[i];
            const int ty = this->cardLayoutY[i];
            const int tw = this->cardLayoutW[i];
            const int th = this->cardLayoutH[i];
            if (x >= (tx - (kCardGap / 2)) && x <= (tx + tw + (kCardGap / 2)) && y >= (ty - 10) && y <= (ty + th + 10))
                return i;
        }
        return -1;
    }

    void MainPage::fileBrowserMenuItem_Click() {
        mainApp->filebrowserPage->openDriveMenu();
        mainApp->filebrowserPage->menu->SetSelectedIndex(0);
        mainApp->LoadLayout(mainApp->filebrowserPage);
    }

    void MainPage::activateSelectedMainItem() {
        switch (this->selectedMainIndex) {
            case 0: this->installMenuItem_Click();       break;
            case 1: this->hddInstallMenuItem_Click();    break;
            case 2: this->gcInstallMenuItem_Click();    break;
            case 3: this->usbInstallMenuItem_Click();    break;
            case 4: this->settingsMenuItem_Click();      break;
            case 5: this->fileBrowserMenuItem_Click();   break;
            default: break;
        }
    }

    void MainPage::showSelectedMainInfo() {
        std::string title;
        std::string desc;
        GetMainItemInfo(this->selectedMainIndex, title, desc);
        if (title.empty()) return;
        mainApp->CreateShowDialog(title, desc, {"common.ok"_lang}, true);
    }

    void MainPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX)) {
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        }
        inst::util::playNavigationClickIfNeeded(Down);

        constexpr u64 kShoulderCombo =
            HidNpadButton_L | HidNpadButton_R | HidNpadButton_ZL | HidNpadButton_ZR;
        {
            static bool shoulderHeld = false;
            if ((Held & kShoulderCombo) == kShoulderCombo) {
                if (!shoulderHeld) {
                    shoulderHeld = true;
                    this->simulateUpdate("99.0.0");
                }
            } else {
                shoulderHeld = false;
            }
        }

        if (this->m_updateToast && this->m_toastExpiry.time_since_epoch().count() > 0
                && std::chrono::steady_clock::now() >= this->m_toastExpiry) {
            this->m_updateToast = nullptr;
            this->m_toastExpiry = {};
        }

        bool toastConsumedInput = false;
        if (this->m_updateToast && !this->m_pendingUpdateInfo.empty()) {
            bool pressed = (Down & HidNpadButton_A) != 0;
            if (!pressed && !Pos.IsEmpty()) {
                const s32 tx = this->m_updateToast->GetX();
                const s32 ty = this->m_updateToast->GetY();
                const s32 tw = this->m_updateToast->GetWidth();
                const s32 th = this->m_updateToast->GetHeight();
                if (Pos.X >= tx && Pos.X <= (tx + tw) && Pos.Y >= ty && Pos.Y <= (ty + th)) {
                    pressed = true;
                }
            }
            if (Down & HidNpadButton_B) {
                toastConsumedInput = true;
                mainApp->EndOverlay();
                this->m_updateToast = nullptr;
            } else if (pressed) {
                toastConsumedInput = true;
                mainApp->EndOverlay();
                this->m_updateToast = nullptr;
                this->m_triggerUpdate.store(true);
            }
        }

        if (!toastConsumedInput && ((Down & HidNpadButton_Plus) || (Down & HidNpadButton_Minus) || (Down & HidNpadButton_B)) && mainApp->IsShown()) {
            mainApp->FadeOut();
            mainApp->Close();
        }
        if (Down & HidNpadButton_Y) {
            this->showSelectedMainInfo();
        }
        if (Down & (HidNpadButton_Left | HidNpadButton_StickLLeft)) {
            if (this->selectedMainIndex > 0) {
                this->selectedMainIndex--;
                this->updateMainGridSelection();
            }
        }
        if (Down & (HidNpadButton_Right | HidNpadButton_StickLRight)) {
            if (this->selectedMainIndex < (kMainCardCount - 1)) {
                this->selectedMainIndex++;
                this->updateMainGridSelection();
            }
        }
        bool touchSelect = false;
        if (!Pos.IsEmpty()) {
            const int touchedIndex = this->getMainGridIndexFromTouch(Pos.X, Pos.Y);
            if (!this->touchActive && touchedIndex >= 0) {
                this->touchActive = true;
                this->touchMoved = false;
                this->touchStartX = Pos.X;
                this->touchStartY = Pos.Y;
                this->selectedMainIndex = touchedIndex;
                this->updateMainGridSelection();
            } else if (this->touchActive) {
                int dx = Pos.X - this->touchStartX;
                int dy = Pos.Y - this->touchStartY;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                if (dx > 12 || dy > 12) {
                    this->touchMoved = true;
                }
                if (touchedIndex >= 0 && touchedIndex != this->selectedMainIndex) {
                    this->selectedMainIndex = touchedIndex;
                    this->updateMainGridSelection();
                }
            }
        } else if (this->touchActive) {
            if (!this->touchMoved) {
                touchSelect = true;
            }
            this->touchActive = false;
            this->touchMoved = false;
        }

        if (!toastConsumedInput && ((Down & HidNpadButton_A) || touchSelect))
            this->activateSelectedMainItem();
    }
}
