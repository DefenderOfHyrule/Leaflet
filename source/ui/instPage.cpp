#include <filesystem>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"
#include "quark/quark_id.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "ui/bottomHint.hpp"
#include <switch.h>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;
    static pu::ui::Layout::Ref lastLayoutBeforeInstall;
    static std::atomic<bool> g_installCancelRequested{false};
    static std::atomic<bool> g_installDimSessionActive{false};
    static std::atomic<bool> g_installDimStopRequested{false};
    static std::thread g_installDimThread;
    static std::mutex g_installDimMutex;
    static bool g_installDimLblReady = false;
    static bool g_installDimSavedBrightnessValid = false;
    static bool g_installDimIsDimmed = false;
    static float g_installDimSavedBrightness = 0.0f;
    static std::chrono::steady_clock::time_point g_installDimLastTouchAt;

    constexpr int kInstallIconSize = 256;
    constexpr int kInstallIconX = (1280 - kInstallIconSize) / 2;
    constexpr int kInstallIconY = 220;
    constexpr auto kInstallDimPollInterval = std::chrono::milliseconds(250);
    constexpr float kInstallDimBrightness = 0.2f;

    static void RestoreInstallBrightnessLocked()
    {
        if (!g_installDimLblReady || !g_installDimSavedBrightnessValid || !g_installDimIsDimmed)
            return;

        lblSetCurrentBrightnessSetting(g_installDimSavedBrightness);
        lblApplyCurrentBrightnessSettingToBacklight();
        g_installDimIsDimmed = false;
    }

    static void EnsureInstallDimmedLocked()
    {
        if (!g_installDimLblReady || !g_installDimSavedBrightnessValid || g_installDimIsDimmed)
            return;

        const float targetBrightness = std::min(g_installDimSavedBrightness, kInstallDimBrightness);
        if (targetBrightness >= g_installDimSavedBrightness)
            return;

        if (R_SUCCEEDED(lblSetCurrentBrightnessSetting(targetBrightness)) &&
            R_SUCCEEDED(lblApplyCurrentBrightnessSettingToBacklight())) {
            g_installDimIsDimmed = true;
        }
    }

    static void StartInstallDimSession()
    {
        if (inst::config::installDimDisable)
            return;

        const auto dimDelay = std::chrono::seconds(inst::config::installDimDelay);

        std::lock_guard<std::mutex> lock(g_installDimMutex);
        g_installDimLastTouchAt = std::chrono::steady_clock::now();

        if (g_installDimSessionActive)
            return;

        g_installDimStopRequested = false;
        g_installDimLblReady = R_SUCCEEDED(lblInitialize());
        g_installDimSavedBrightnessValid = false;
        g_installDimIsDimmed = false;
        g_installDimSavedBrightness = 0.0f;

        if (g_installDimLblReady) {
            float currentBrightness = 0.0f;
            if (R_SUCCEEDED(lblGetCurrentBrightnessSetting(&currentBrightness))) {
                g_installDimSavedBrightness = currentBrightness;
                g_installDimSavedBrightnessValid = true;
            }
        }

        g_installDimSessionActive = true;
        g_installDimThread = std::thread([dimDelay]() {
            while (!g_installDimStopRequested) {
                {
                    std::lock_guard<std::mutex> lock(g_installDimMutex);
                    if (!g_installDimSessionActive)
                        break;
                    if (std::chrono::steady_clock::now() - g_installDimLastTouchAt >= dimDelay)
                        EnsureInstallDimmedLocked();
                }
                std::this_thread::sleep_for(kInstallDimPollInterval);
            }
        });
    }

    static void StopInstallDimSession()
    {
        g_installDimStopRequested = true;
        if (g_installDimThread.joinable())
            g_installDimThread.join();

        std::lock_guard<std::mutex> lock(g_installDimMutex);
        RestoreInstallBrightnessLocked();
        if (g_installDimLblReady)
            lblExit();
        g_installDimLblReady = false;
        g_installDimSavedBrightnessValid = false;
        g_installDimSessionActive = false;
    }

    static void NotifyInstallTouchActivity()
    {
        std::lock_guard<std::mutex> lock(g_installDimMutex);
        if (!g_installDimSessionActive)
            return;

        g_installDimLastTouchAt = std::chrono::steady_clock::now();
        RestoreInstallBrightnessLocked();
    }

    instPage::instPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::config::colorBackground));
        if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else this->SetBackgroundImage("romfs:/images/background.jpg");
        const auto infoColor = COLOR(inst::config::colorBotBar);
        const auto botColor = COLOR(inst::config::colorBotBar);
        this->infoRect = Rectangle::New(0, 75, 1280, 60, infoColor);
        this->botRect = Rectangle::New(0, 660, 1280, 60, botColor);
        this->pageInfoText = TextBlock::New(10, 89, "", 30);
        this->pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        const auto panelColor = COLOR(inst::config::colorTopBar);
        this->installPanel = Rectangle::New(0, 536, 1280, 120, panelColor);
        this->installInfoText = TextBlock::New(20, 548, "", 22);
        this->installInfoText->SetColor(COLOR("#FFFFFFFF"));
        this->installBar = pu::ui::elm::ProgressBar::New(20, 592, 1240, 36, 100.0f);
        this->installBar->SetColor(COLOR("#FFFFFF22"));
        this->installBar->SetProgressColor(COLOR("#FF4D4DFF"));
        this->hintText = TextBlock::New(0, 678, " Back", 20);
        this->hintText->SetColor(COLOR("#FFFFFFFF"));
        this->hintText->SetX(10);
        this->hintText->SetVisible(false);
        this->bottomHintSegments = BuildBottomHintSegments(this->hintText->GetText(), this->hintText->GetX(), 20);
        this->progressText = TextBlock::New(0, 340, "", 30);
        this->progressText->SetColor(COLOR("#FFFFFFFF"));
        this->progressText->SetVisible(false);
        this->progressDetailText = TextBlock::New(0, 678, "", 20);
        this->progressDetailText->SetColor(COLOR("#FFFFFFFF"));
        this->progressDetailText->SetVisible(false);
        this->installIconImage = Image::New(kInstallIconX, kInstallIconY, "romfs:/images/icons/exit-run.png");
        this->installIconImage->SetWidth(kInstallIconSize);
        this->installIconImage->SetHeight(kInstallIconSize);
        this->installIconImage->SetVisible(false);
        this->statusBar = StatusBar::New(StatusBar::Mode::Full, ""); this->statusBar->Attach(this);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->pageInfoText);
        this->Add(this->installPanel);
        this->Add(this->installInfoText);
        this->Add(this->installBar);
        this->Add(this->progressText);
        this->Add(this->progressDetailText);
        this->consoleIdText = TextBlock::New(0, 681, "", 18);
        auto consoleIdColor = StatusBar::SurfaceTextColor(botColor);
        consoleIdColor.A = 0x99;
        this->consoleIdText->SetColor(consoleIdColor);
        this->consoleIdText->SetVisible(false);
        if (appletGetAppletType() == AppletType_LibraryApplet) {
            auto probe = TextBlock::New(0, 0, "v" + inst::config::appVersion + " | Applet Mode", 18);
            this->consoleIdReservedW = probe->GetTextWidth() + 12;
        }
        this->Add(this->hintText);
        this->Add(this->consoleIdText);
        this->Add(this->installIconImage);
    }

    void instPage::setTopInstInfoText(std::string ourText){
        mainApp->instpage->pageInfoText->SetText(ourText);
        mainApp->CallForRender();
    }

    void instPage::setInstInfoText(std::string ourText){
        mainApp->instpage->installInfoText->SetText(ourText);
        const int textW = mainApp->instpage->installInfoText->GetTextWidth();
        mainApp->instpage->installInfoText->SetX((1280 - textW) / 2);
        mainApp->CallForRender();
    }

    void instPage::setInstBarPerc(double ourPercent){
        mainApp->instpage->installPanel->SetVisible(true);
        mainApp->instpage->installBar->SetVisible(true);
        mainApp->instpage->installBar->SetProgress(ourPercent);
        if (ourPercent > 0.0 && !isInstallCancelRequested()) {
            mainApp->UpdateButtons();
            if (mainApp->GetButtonsHeld() & HidNpadButton_B) {
                const int choice = mainApp->CreateShowDialog(
                    "Cancel install?",
                    "Stop the current install and clean up partial data?",
                    {"Cancel Install", "Go Back"},
                    false);
                if (choice == 0) {
                    requestInstallCancel();
                }
            }
        }
        mainApp->CallForRender();
    }

    void instPage::setProgressDetailText(const std::string& ourText){
        mainApp->instpage->progressDetailText->SetText(ourText);
        mainApp->instpage->progressDetailText->SetX((1280 - mainApp->instpage->progressDetailText->GetTextWidth()) / 2);
        mainApp->instpage->progressDetailText->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::clearProgressDetailText(){
        mainApp->instpage->progressDetailText->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::setInstallIconFromTitleId(u64 titleId){
        if (titleId == 0) {
            return;
        }

        NsApplicationControlData appControlData{};
        size_t sizeRead = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, &appControlData, sizeof(NsApplicationControlData), &sizeRead);
        if (R_FAILED(rc) || sizeRead <= sizeof(appControlData.nacp)) {
            return;
        }

        const size_t iconSize = sizeRead - sizeof(appControlData.nacp);
        if (iconSize == 0) {
            return;
        }

        mainApp->instpage->installIconImage->SetJpegImage(appControlData.icon, iconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::setInstallIcon(const std::string& imagePath){
        if (imagePath.empty()) {
            clearInstallIcon();
            return;
        }
        mainApp->instpage->installIconImage->SetImage(imagePath);
        mainApp->instpage->installIconImage->SetX(kInstallIconX);
        mainApp->instpage->installIconImage->SetY(kInstallIconY);
        mainApp->instpage->installIconImage->SetWidth(kInstallIconSize);
        mainApp->instpage->installIconImage->SetHeight(kInstallIconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::setInstallIconData(const void* imageData, std::uint32_t imageSize){
        if (imageData == nullptr || imageSize == 0) {
            clearInstallIcon();
            return;
        }
        mainApp->instpage->installIconImage->SetJpegImage(const_cast<void*>(imageData), static_cast<s32>(imageSize));
        mainApp->instpage->installIconImage->SetX(kInstallIconX);
        mainApp->instpage->installIconImage->SetY(kInstallIconY);
        mainApp->instpage->installIconImage->SetWidth(kInstallIconSize);
        mainApp->instpage->installIconImage->SetHeight(kInstallIconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::clearInstallIcon(){
        mainApp->instpage->installIconImage->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::loadMainMenu(){
        StopInstallDimSession();
        mainApp->instpage->consoleIdText->SetVisible(false);
        if (lastLayoutBeforeInstall != nullptr && lastLayoutBeforeInstall != mainApp->instpage)
            mainApp->LoadLayout(lastLayoutBeforeInstall);
        else
            mainApp->LoadLayout(mainApp->mainPage);
    }

    void instPage::loadInstallScreen(){
        auto currentLayout = mainApp->GetCurrentLayout();
        if (currentLayout != nullptr && currentLayout != mainApp->instpage)
            lastLayoutBeforeInstall = currentLayout;
        mainApp->instpage->pageInfoText->SetText("");
        mainApp->instpage->installInfoText->SetText("");
        mainApp->instpage->installPanel->SetVisible(false);
        mainApp->instpage->installBar->SetProgress(0);
        mainApp->instpage->installBar->SetVisible(false);
        mainApp->instpage->hintText->SetText(" Cancel");
        mainApp->instpage->hintText->SetX(10);
        mainApp->instpage->hintText->SetVisible(true);
        mainApp->instpage->bottomHintSegments = BuildBottomHintSegments(mainApp->instpage->hintText->GetText(), mainApp->instpage->hintText->GetX(), 20);
        mainApp->instpage->progressText->SetVisible(false);
        mainApp->instpage->progressDetailText->SetVisible(false);
        mainApp->instpage->installIconImage->SetVisible(false);
        mainApp->instpage->touchWakeActive = false;
        {
            const std::string& activeId = !quark::GetNetConsoleId().empty()
                ? quark::GetNetConsoleId() : quark::GetConsoleId();
            if (!activeId.empty()) {
                mainApp->instpage->consoleIdText->SetText("[" + activeId + "]");
                mainApp->instpage->consoleIdText->SetX(1280 - 10 - mainApp->instpage->consoleIdReservedW - mainApp->instpage->consoleIdText->GetTextWidth());
                mainApp->instpage->consoleIdText->SetVisible(true);
            }
        }
        g_installCancelRequested.store(false);
        StartInstallDimSession();
        mainApp->LoadLayout(mainApp->instpage);
        mainApp->CallForRender();
    }

    void instPage::requestInstallCancel(){
        g_installCancelRequested.store(true);
    }

    bool instPage::isInstallCancelRequested(){
        return g_installCancelRequested.load();
    }

    void instPage::clearInstallCancel(){
        g_installCancelRequested.store(false);
    }

    void instPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        if (!Pos.IsEmpty()) {
            if (!this->touchWakeActive) {
                this->touchWakeActive = true;
                NotifyInstallTouchActivity();
            }
        } else {
            this->touchWakeActive = false;
        }

        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX)) {
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        }
        inst::util::playNavigationClickIfNeeded(Down);
        if (Down & HidNpadButton_B) {
            if (this->installBar->IsVisible()) {
                if (isInstallCancelRequested()) {
                    setInstInfoText("Cancelling install...");
                    return;
                }
                const int choice = mainApp->CreateShowDialog(
                    "Cancel install?",
                    "Stop the current install and clean up partial data?",
                    {"Cancel Install", "Go Back"},
                    false
                );
                if (choice == 0) {
                    requestInstallCancel();
                    setInstInfoText("Cancelling install...");
                }
                return;
            }
            if (this->hintText->IsVisible()) {
                loadMainMenu();
            }
        }
    }
}

