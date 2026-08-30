#include "ui/MainApplication.hpp"
#include "ui/statusBar.hpp"
#include "util/lang.hpp"
#include "util/config.hpp"
#include "util/util.hpp"
#include "util/install_diagnostics.hpp"
#include <pu/ui/ui_Dialog.hpp>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <thread>
#include "gcInstall.hpp"
#include "switch.h"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    MainApplication *mainApp;


    static std::string FormatOneDecimal(double value) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.1f", value);
        std::string out = buf;
        if (out.find('.') != std::string::npos) {
            while (!out.empty() && out.back() == '0') out.pop_back();
            if (!out.empty() && out.back() == '.') out.pop_back();
        }
        return out;
    }

    static void applyDialogTheme() {
        pu::ui::Dialog::SetDefaultColors(
            COLOR(inst::config::dialogBackgroundColorFloored()),
            COLOR(inst::config::colorDialogBorder),
            COLOR(inst::config::menuHighlightColorCapped()));
    }

    void MainApplication::rebuildLayouts(int restoreSection) {
        // recreate all layouts so theme changes take effect immediately
        applyDialogTheme();
        this->mainPage       = MainPage::New();
        this->filebrowserPage = fileBrowserPage::New();
        this->sdinstPage  = sdInstPage::New();
        this->usbinstPage = usbInstPage::New();
        this->netinstPage = netInstPage::New();
        this->hddinstPage = hddInstPage::New();
        this->instpage    = instPage::New();
        this->optionspage = optionsPage::New();
        this->gcinstPage  = gcInstPage::New();
        this->m_themePage = inst::ui::themeManagerPage::New();
        { std::weak_ptr<inst::ui::themeManagerPage> w = this->m_themePage; this->m_themePage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }

        { std::weak_ptr<MainPage>        w = this->mainPage;       this->mainPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<fileBrowserPage> w = this->filebrowserPage; this->filebrowserPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<netInstPage>     w = this->netinstPage;     this->netinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<sdInstPage>  w = this->sdinstPage;  this->sdinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<usbInstPage> w = this->usbinstPage; this->usbinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<hddInstPage> w = this->hddinstPage; this->hddinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<instPage>    w = this->instpage;    this->instpage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<optionsPage> w = this->optionspage; this->optionspage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<gcInstPage>  w = this->gcinstPage;  this->gcinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }

        this->LoadLayout(this->optionspage);
        if (restoreSection >= 0)
            this->optionspage->setSection(restoreSection);
    }

    void MainApplication::OnLoad() {
        mainApp = this;

        Language::Load();
        applyDialogTheme();

        this->mainPage       = MainPage::New();
        this->filebrowserPage = fileBrowserPage::New();
        this->sdinstPage  = sdInstPage::New();
        this->usbinstPage = usbInstPage::New();
        this->netinstPage = netInstPage::New();
        this->hddinstPage = hddInstPage::New();
        this->instpage    = instPage::New();
        this->optionspage = optionsPage::New();
        this->gcinstPage  = gcInstPage::New();
        this->m_themePage = inst::ui::themeManagerPage::New();
        { std::weak_ptr<inst::ui::themeManagerPage> w = this->m_themePage; this->m_themePage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }

        { std::weak_ptr<MainPage>        w = this->mainPage;       this->mainPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<fileBrowserPage> w = this->filebrowserPage; this->filebrowserPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<netInstPage>     w = this->netinstPage;     this->netinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<sdInstPage>  w = this->sdinstPage;  this->sdinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<usbInstPage> w = this->usbinstPage; this->usbinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<hddInstPage> w = this->hddinstPage; this->hddinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<instPage>    w = this->instpage;    this->instpage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<optionsPage> w = this->optionspage; this->optionspage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        { std::weak_ptr<gcInstPage>  w = this->gcinstPage;  this->gcinstPage->SetOnInput([w](u64 d,u64 u,u64 h,pu::ui::Touch t){ if(auto p=w.lock()) p->onInput(d,u,h,t); }); }
        this->LoadLayout(this->mainPage);

        this->AddThread([this]() {
            static AppletFocusState last_focus = AppletFocusState_InFocus;
            static u64 last_check_tick = 0;
            const u64 now = armGetSystemTick();
            const u64 freq = armGetSystemTickFreq();
            if (last_check_tick != 0 && (now - last_check_tick) < (freq / 2))
                return;
            last_check_tick = now;

            AppletFocusState focus = appletGetFocusState();
            if (focus != last_focus && focus == AppletFocusState_InFocus) {
                padConfigureInput(1, HidNpadStyleSet_NpadStandard);
                padInitializeDefault(&this->input_pad);
            }
            last_focus = focus;

            if (focus == AppletFocusState_InFocus && !padIsConnected(&this->input_pad)) {
                padConfigureInput(1, HidNpadStyleSet_NpadStandard);
                padInitializeDefault(&this->input_pad);
            }
        });

        this->AddThread([this]() {
            static u64 lastTick = 0;
            const u64 now = armGetSystemTick();
            const u64 freq = armGetSystemTickFreq();
            if (lastTick != 0 && (now - lastTick) < freq)
                return;
            lastTick = now;

            std::string timeText = "--:--";
            if (R_SUCCEEDED(timeInitialize())) {
                u64 posix = 0;
                if (R_SUCCEEDED(timeGetCurrentTime(TimeType_LocalSystemClock, &posix))) {
                    std::time_t t = static_cast<std::time_t>(posix);
                    std::tm* local = std::localtime(&t);
                    char buf[16] = {0};
                    if (local && std::strftime(buf, sizeof(buf), "%I:%M %p", local) > 0)
                        timeText = buf;
                }
                timeExit();
            }

            bool internetUp = false;
            bool wifiConnected = false;
            u32 wifiStrength = 0;
            if (R_SUCCEEDED(nifmInitialize(NifmServiceType_User))) {
                NifmInternetConnectionStatus status = static_cast<NifmInternetConnectionStatus>(0);
                NifmInternetConnectionType type = static_cast<NifmInternetConnectionType>(0);
                if (R_SUCCEEDED(nifmGetInternetConnectionStatus(&type, &wifiStrength, &status))) {
                    internetUp = (status == NifmInternetConnectionStatus_Connected);
                    wifiConnected = internetUp && (type == NifmInternetConnectionType_WiFi);
                }
                nifmExit();
            }

            int batteryPct = -1;
            if (R_SUCCEEDED(psmInitialize())) {
                u32 pct = 0;
                if (R_SUCCEEDED(psmGetBatteryChargePercentage(&pct)))
                    batteryPct = static_cast<int>(pct);
                psmExit();
            }

            s64 systemTotal = 0, systemFree = 0, sdTotalBytes = 0, sdFreeBytes = 0;
            if (R_SUCCEEDED(ncmInitialize())) {
                NcmContentStorage storage{};
                if (R_SUCCEEDED(ncmOpenContentStorage(&storage, NcmStorageId_BuiltInUser))) {
                    ncmContentStorageGetTotalSpaceSize(&storage, &systemTotal);
                    ncmContentStorageGetFreeSpaceSize(&storage, &systemFree);
                    ncmContentStorageClose(&storage);
                }
                if (R_SUCCEEDED(ncmOpenContentStorage(&storage, NcmStorageId_SdCard))) {
                    ncmContentStorageGetTotalSpaceSize(&storage, &sdTotalBytes);
                    ncmContentStorageGetFreeSpaceSize(&storage, &sdFreeBytes);
                    ncmContentStorageClose(&storage);
                }
                ncmExit();
            }

            StatusBar::Status status;
            status.timeText = timeText;
            status.ip = inst::util::getIPAddress();
            status.internetUp = internetUp;
            status.wifiConnected = wifiConnected;
            status.wifiStrength = wifiStrength;
            status.batteryPct = batteryPct;
            status.systemTotal = systemTotal;
            status.systemFree = systemFree;
            status.sdTotal = sdTotalBytes;
            status.sdFree = sdFreeBytes;
            StatusBar::UpdateAll(status);
        });
    }
}