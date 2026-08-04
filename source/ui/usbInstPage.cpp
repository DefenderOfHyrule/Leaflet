#include "ui/usbInstPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include "usbInstall.hpp"
#include "ui/bottomHint.hpp"
#include "quark/quark_usb.hpp"
#include <usb/cf/cf_CommandFramework.hpp>
#include "quark/quark_cmd.hpp"
#include "quark/quark_id.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <threads.h>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {

    extern MainApplication *mainApp;

    usbInstPage::usbInstPage() : Layout::Layout() {
        const bool oled = inst::config::oledMode;

        if (oled) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR(inst::config::colorBackground));
            if (std::filesystem::exists(inst::config::appDir + "/background.png"))
                this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else
                this->SetBackgroundImage("romfs:/images/background.jpg");
        }

        const auto infoColor = oled ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
        const auto botColor  = oled ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);

        m_infoRect = Rectangle::New(0,  75, 1280,  60, infoColor);
        m_botRect  = Rectangle::New(0, 660, 1280,  60, botColor);



        pageInfoText = TextBlock::New(10, 89, "", 30);
        pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        m_butText = TextBlock::New(10, 678, "", 20);
        m_butText->SetColor(COLOR("#FFFFFFFF"));
        this->setButtonsText("inst.usb.buttons"_lang);
        m_consoleIdText = TextBlock::New(0, 681, "", 18);
        auto consoleIdColor = StatusBar::SurfaceTextColor(botColor);
        consoleIdColor.A = 0x99;
        m_consoleIdText->SetColor(consoleIdColor);
        m_consoleIdText->SetVisible(false);
        if (appletGetAppletType() == AppletType_LibraryApplet) {
            auto probe = TextBlock::New(0, 0, "v" + inst::config::appVersion + " | Applet Mode", 18);
            m_consoleIdReservedW = probe->GetTextWidth() + 12;
        }

        m_menu = pu::ui::elm::Menu::New(0, 136, 1280, COLOR("#FFFFFF00"), 50, 10);
        if (oled) {
            m_menu->SetOnFocusColor(COLOR("#FFFFFF33"));
            m_menu->SetScrollbarColor(COLOR("#FFFFFF66"));
        } else {
            m_menu->SetOnFocusColor(COLOR(inst::config::colorTileHighlight));
            m_menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));
        }

        m_infoImage = Image::New(460, 332, "romfs:/images/icons/usb-connection-waiting.png");

        this->statusBar = StatusBar::New(StatusBar::Mode::Full, "main.menu.usb"_lang); this->statusBar->Attach(this);
        this->Add(m_infoRect);
        this->Add(m_botRect);
        this->Add(m_butText);
        this->Add(m_consoleIdText);
        this->Add(pageInfoText);
        this->Add(m_menu);
        this->Add(m_infoImage);
    }

    bool usbInstPage::isDirectory(int index) const {
        if (index < 0 || index >= (int)m_isDirectory.size()) return false;
        return m_isDirectory[index];
    }

    void usbInstPage::setButtonsText(const std::string &text) {
        m_butText->SetText(text);
        m_bottomHintSegments = BuildBottomHintSegments(text, 10, 20);
    }

    void usbInstPage::drawMenuItems(bool clearSelection) {
        if (clearSelection) m_selectedPaths.clear();
        m_menu->ClearItems();

        for (int i = 0; i < (int)m_menuEntries.size(); i++) {
            std::string label = inst::util::shortenString(m_menuEntries[i], 56, true);
            auto item = pu::ui::elm::MenuItem::New(label);
            item->SetColor(COLOR("#FFFFFFFF"));
            if (isDirectory(i)) {
                item->SetIcon("romfs:/images/icons/folder.png");
            } else {
                bool selected = std::find(m_selectedPaths.begin(), m_selectedPaths.end(),
                                          m_menuPaths[i]) != m_selectedPaths.end();
                item->SetIcon(selected ? "romfs:/images/icons/check-box-outline.png"
                                       : "romfs:/images/icons/checkbox-blank-outline.png");
            }
            m_menu->AddItem(item);
        }
    }

    void usbInstPage::toggleSelection(int index) {
        if (index < 0 || index >= (int)m_menuPaths.size()) return;
        if (isDirectory(index)) return;
        const std::string &path = m_menuPaths[index];
        auto it = std::find(m_selectedPaths.begin(), m_selectedPaths.end(), path);
        if (it != m_selectedPaths.end()) m_selectedPaths.erase(it);
        else m_selectedPaths.push_back(path);
        drawMenuItems(false);
    }

    void usbInstPage::buildRootList() {
        m_menuEntries.clear();
        m_menuPaths.clear();
        m_isDirectory.clear();

        const u32 spCount = quark::cmd::GetSpecialPathCount();
        for (u32 i = 0; i < spCount; i++) {
            quark::cmd::SpecialPath sp;
            if (R_SUCCEEDED(quark::cmd::GetSpecialPath(i, sp))) {
                m_menuEntries.push_back(sp.name);
                m_menuPaths.push_back(sp.path);
                m_isDirectory.push_back(true);
            }
        }

        const u32 driveCount = quark::cmd::GetDriveCount();
        for (u32 i = 0; i < driveCount; i++) {
            quark::cmd::DriveInfo di;
            if (R_SUCCEEDED(quark::cmd::GetDriveInfo(i, di))) {
                m_menuEntries.push_back(di.label + " (" + di.path + ")");
                m_menuPaths.push_back(di.path);
                m_isDirectory.push_back(true);
            }
        }
    }

    void usbInstPage::refreshCurrentDirectory() {
        m_menuEntries.clear();
        m_menuPaths.clear();
        m_isDirectory.clear();

        if (!m_pathStack.empty()) {
            m_menuEntries.push_back("..");
            m_menuPaths.push_back("..");
            m_isDirectory.push_back(true);
        }

        for (const auto &dir : quark::cmd::GetDirectories(m_currentPath)) {
            m_menuEntries.push_back(dir);
            m_menuPaths.push_back(m_currentPath + "/" + dir);
            m_isDirectory.push_back(true);
        }

        for (const auto &file : quark::cmd::GetFiles(m_currentPath)) {
            m_menuEntries.push_back(file);
            m_menuPaths.push_back(m_currentPath + "/" + file);
            m_isDirectory.push_back(false);
        }
    }

    void usbInstPage::navigateTo(const std::string &remotePath, bool pushHistory) {
        if (pushHistory) m_pathStack.push_back(m_currentPath);
        m_currentPath = remotePath;
        refreshCurrentDirectory();
        drawMenuItems(false);
        m_menu->SetSelectedIndex(0);
    }

    void usbInstPage::navigateBack() {
        if (m_pathStack.empty()) {
            m_currentPath.clear();
            buildRootList();
        } else {
            m_currentPath = m_pathStack.back();
            m_pathStack.pop_back();
            if (m_currentPath.empty()) buildRootList();
            else refreshCurrentDirectory();
        }
        drawMenuItems(false);
        m_menu->SetSelectedIndex(0);
    }

    void usbInstPage::startUsb() {
        m_selectedPaths.clear();
        m_pathStack.clear();
        m_currentPath.clear();

        pageInfoText->SetText("inst.usb.top_info"_lang);
        this->setButtonsText("inst.usb.buttons"_lang);
        m_menu->SetVisible(false);
        m_menu->ClearItems();
        m_infoImage->SetVisible(true);
        mainApp->LoadLayout(mainApp->usbinstPage);
        mainApp->CallForRender();

        while (!quark::usb::IsConnected()) {
            mainApp->UpdateButtons();
            const u64 kHeld = mainApp->GetButtonsHeld();
            if (kHeld & HidNpadButton_B) {
                mainApp->LoadLayout(mainApp->mainPage);
                return;
            }
            if (mainApp->GetButtonsDown() & HidNpadButton_X)
                mainApp->CreateShowDialog("inst.usb.help.title"_lang, "inst.usb.help.desc"_lang,
                                          {"common.ok"_lang}, true);
            svcSleepThread(15'000'000ull);
        }

        usb::cf::AnnounceConsoleId(quark::GetConsoleId());

        {
            std::atomic<bool> listDone{false};
            thrd_t listThrd;
            struct Args { usbInstPage* self; std::atomic<bool>* done; };
            Args args{this, &listDone};
            thrd_create(&listThrd, [](void* arg) -> int {
                auto* a = static_cast<Args*>(arg);
                a->self->buildRootList();
                a->done->store(true);
                return 0;
            }, &args);

            while (!listDone.load()) {
                mainApp->UpdateButtons();
                if (mainApp->GetButtonsHeld() & HidNpadButton_B) {
                    quark::usb::Finalize();
                    thrd_join(listThrd, nullptr);
                    quark::usb::Initialize();
                    mainApp->LoadLayout(mainApp->mainPage);
                    return;
                }
                if (mainApp->GetButtonsDown() & HidNpadButton_X)
                    mainApp->CreateShowDialog("inst.usb.help.title"_lang, "inst.usb.help.desc"_lang,
                                              {"common.ok"_lang}, true);
                svcSleepThread(15'000'000ull);
            }
            thrd_join(listThrd, nullptr);
        }
        if (m_menuEntries.empty()) {
            mainApp->CreateShowDialog("inst.usb.top_info"_lang,
                                      "Quark reported no drives or special paths.",
                                      {"common.ok"_lang}, true);
            mainApp->LoadLayout(mainApp->mainPage);
            return;
        }

        pageInfoText->SetText("inst.usb.top_info2"_lang);
        this->setButtonsText("inst.usb.buttons2"_lang);
        m_consoleIdText->SetText("[" + quark::GetConsoleId() + "]");
        m_consoleIdText->SetX(1280 - 10 - m_consoleIdReservedW - m_consoleIdText->GetTextWidth());
        m_consoleIdText->SetVisible(true);
        drawMenuItems(true);
        m_menu->SetSelectedIndex(0);
        mainApp->CallForRender();
        m_infoImage->SetVisible(false);
        m_menu->SetVisible(true);
    }

    void usbInstPage::startInstall() {
        if (m_selectedPaths.empty()) return;

        int dialogResult = -1;
        if (m_selectedPaths.size() == 1)
            dialogResult = mainApp->CreateShowDialog(
                "inst.target.desc0"_lang +
                inst::util::shortenString(inst::util::formatUrlString(m_selectedPaths[0]), 32, true) +
                "inst.target.desc1"_lang,
                "common.cancel_desc"_lang,
                {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);
        else
            dialogResult = mainApp->CreateShowDialog(
                "inst.target.desc00"_lang + std::to_string(m_selectedPaths.size()) +
                "inst.target.desc01"_lang,
                "common.cancel_desc"_lang,
                {"inst.target.opt0"_lang, "inst.target.opt1"_lang}, false);

        if (dialogResult == -1) return;
        usbInstStuff::installTitleUsb(m_selectedPaths, dialogResult);
    }

    void usbInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Held;

        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, m_bottomHintTouch, 668, 52, bottomTapX))
            Down |= FindBottomHintButton(m_bottomHintSegments, bottomTapX);

        inst::util::playNavigationClickIfNeeded(Down);

        if (!Pos.IsEmpty()) {
            if (!m_touchTapActive) {
                m_touchTapActive = true;
                m_touchTapMoved  = false;
                m_touchTapStartX = Pos.X;
                m_touchTapStartY = Pos.Y;
            } else {
                if (std::abs(Pos.X - m_touchTapStartX) > 12 ||
                    std::abs(Pos.Y - m_touchTapStartY) > 12)
                    m_touchTapMoved = true;
            }
        } else if (m_touchTapActive) {
            if (!m_touchTapMoved) {
                const int idx = m_menu->GetSelectedIndex();
                const auto now = std::chrono::steady_clock::now();
                bool doubleTap = false;
                if (m_hasLastTap && m_lastTapIndex == idx) {
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - m_lastTapTime).count() <= 350)
                        doubleTap = true;
                }
                if (doubleTap) {
                    if (isDirectory(idx)) {
                        if (m_menuPaths[idx] == "..") navigateBack();
                        else navigateTo(m_menuPaths[idx], true);
                    } else {
                        toggleSelection(idx);
                        if (m_menu->GetItems().size() == 1 && m_selectedPaths.size() == 1)
                            startInstall();
                    }
                    m_hasLastTap = false;
                    m_lastTapIndex = -1;
                } else {
                    m_hasLastTap   = true;
                    m_lastTapIndex = idx;
                    m_lastTapTime  = now;
                }
            }
            m_touchTapActive = false;
            m_touchTapMoved  = false;
        }

        if (Down & HidNpadButton_B) {
            if (m_currentPath.empty() && m_pathStack.empty())
                mainApp->LoadLayout(mainApp->mainPage);
            else
                navigateBack();
        }

        if (Down & HidNpadButton_A) {
            const int idx = m_menu->GetSelectedIndex();
            if (isDirectory(idx)) {
                if (m_menuPaths[idx] == "..") navigateBack();
                else navigateTo(m_menuPaths[idx], true);
            } else {
                toggleSelection(idx);
                if (m_menu->GetItems().size() == 1 && m_selectedPaths.size() == 1)
                    startInstall();
            }
            m_hasLastTap   = false;
            m_lastTapIndex = -1;
        }

        if (Down & HidNpadButton_Y) {
            bool allSelected = true;
            for (int i = 0; i < (int)m_menuPaths.size(); i++) {
                if (!isDirectory(i) &&
                    std::find(m_selectedPaths.begin(), m_selectedPaths.end(),
                              m_menuPaths[i]) == m_selectedPaths.end()) {
                    allSelected = false;
                    break;
                }
            }
            if (allSelected) {
                drawMenuItems(true);
            } else {
                for (int i = 0; i < (int)m_menuPaths.size(); i++) {
                    if (!isDirectory(i) &&
                        std::find(m_selectedPaths.begin(), m_selectedPaths.end(),
                                  m_menuPaths[i]) == m_selectedPaths.end())
                        m_selectedPaths.push_back(m_menuPaths[i]);
                }
                drawMenuItems(false);
            }
        }

        if (Down & HidNpadButton_Plus) {
            if (m_selectedPaths.empty()) {
                const int idx = m_menu->GetSelectedIndex();
                if (!isDirectory(idx)) {
                    toggleSelection(idx);
                    startInstall();
                }
                return;
            }
            startInstall();
        }
    }

}
