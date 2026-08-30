#include "ui/netInstPage.hpp"
#include "ui/MainApplication.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/lang.hpp"
#include "netInstall.hpp"
#include "ui/bottomHint.hpp"
#include "quark/quark_net.hpp"
#include "quark/quark_net_cmd.hpp"
#include "quark/quark_id.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <threads.h>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {

    extern MainApplication *mainApp;

    netInstPage::netInstPage() : Layout::Layout() {
        this->SetBackgroundColor(COLOR(inst::config::colorBackground));
        if (std::filesystem::exists(inst::config::appDir + "/background.png"))
            this->SetBackgroundImage(inst::config::appDir + "/background.png");
        else
            this->SetBackgroundImage("romfs:/images/background.jpg");

        const auto infoColor = COLOR(inst::config::colorBotBar);
        const auto botColor  = COLOR(inst::config::colorBotBar);

        m_infoRect = Rectangle::New(0,  75, 1280,  60, infoColor);
        m_botRect  = Rectangle::New(0, 660, 1280,  60, botColor);



        pageInfoText = TextBlock::New(10, 89, "", 30);
        pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        m_butText = TextBlock::New(10, 678, "", 20);
        m_butText->SetColor(COLOR("#FFFFFFFF"));
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
        m_menu->SetOnFocusColor(COLOR(inst::config::menuHighlightColorCapped()));
        m_menu->SetScrollbarColor(COLOR(inst::config::colorBotBar));

        m_infoImage = Image::New(460, 332, "romfs:/images/icons/usb-connection-waiting.png");
        m_infoImage->SetVisible(false);

        this->statusBar = StatusBar::New(StatusBar::Mode::Full, "main.menu.usb"_lang); this->statusBar->Attach(this);
        this->Add(m_infoRect);
        this->Add(m_botRect);
        this->Add(m_butText);
        this->Add(m_consoleIdText);
        this->Add(pageInfoText);
        this->Add(m_menu);
        this->Add(m_infoImage);
    }

    void netInstPage::setButtonsText(const std::string& text) {
        m_butText->SetText(text);
        m_bottomHintSegments = BuildBottomHintSegments(text, 10, 20);
    }

    void netInstPage::buildHostMenu() {
        m_state = State::HostSelect;
        m_menu->ClearItems();
        pageInfoText->SetText("inst.net.ip.select_desc"_lang);
        setButtonsText("inst.net.host.buttons"_lang);

        for (const auto& h : inst::config::savedHosts) {
            auto item = pu::ui::elm::MenuItem::New(h.name + "  [" + h.ip + "]");
            item->SetColor(COLOR("#FFFFFFFF"));
            item->SetIcon("romfs:/images/icons/net-install.png");
            m_menu->AddItem(item);
        }

        auto addItem = pu::ui::elm::MenuItem::New("inst.net.new_host"_lang);
        addItem->SetColor(COLOR("#FFFFFFFF"));
        addItem->SetIcon("romfs:/images/icons/plus.png");
        m_menu->AddItem(addItem);

        m_menu->SetSelectedIndex(0);
        m_infoImage->SetVisible(false);
        m_menu->SetVisible(true);
    }

    void netInstPage::addNewHost() {
        const std::string ip = inst::util::softwareKeyboard("inst.net.ip.desc"_lang, "", 64);
        if (ip.empty()) return;

        const std::string name = inst::util::softwareKeyboard("inst.net.ip.name_desc"_lang, ip, 32);
        if (name.empty()) return;

        inst::config::QuarkHost h;
        h.name = name;
        h.ip   = ip;
        inst::config::savedHosts.push_back(h);
        inst::config::saveHosts();

        buildHostMenu();
        const int newIdx = static_cast<int>(inst::config::savedHosts.size()) - 1;
        m_menu->SetSelectedIndex(newIdx);
    }

    void netInstPage::deleteHost(int index) {
        if (index < 0 || index >= (int)inst::config::savedHosts.size()) return;
        const std::string name = inst::config::savedHosts[index].name;
        const int confirm = mainApp->CreateShowDialog(
            "inst.net.host.delete_title"_lang,
            inst::util::formatString("inst.net.host.delete_desc"_lang, name),
            {"common.ok"_lang, "common.cancel"_lang}, true);
        if (confirm != 0) return;
        inst::config::savedHosts.erase(inst::config::savedHosts.begin() + index);
        inst::config::saveHosts();
        buildHostMenu();
        const int newCount = static_cast<int>(m_menu->GetItems().size());
        m_menu->SetSelectedIndex(std::min(index, newCount - 1));
    }

    void netInstPage::connectToHost(const std::string& ip) {
        m_menu->SetVisible(false);
        m_infoImage->SetVisible(true);
        pageInfoText->SetText("inst.net.connecting"_lang);
        setButtonsText("inst.net.connect_cancel"_lang);
        mainApp->CallForRender();

        std::atomic<bool> connectDone{false};
        std::atomic<bool> connectResult{false};
        thrd_t connectThrd;
        struct CArgs { std::string ip; std::atomic<bool>* done; std::atomic<bool>* result; };
        CArgs cargs{ ip, &connectDone, &connectResult };
        thrd_create(&connectThrd, [](void* arg) -> int {
            auto* a = static_cast<CArgs*>(arg);
            a->result->store(quark::net::Connect(a->ip));
            a->done->store(true);
            return 0;
        }, &cargs);

        while (!connectDone.load()) {
            mainApp->UpdateButtons();
            if (mainApp->GetButtonsHeld() & HidNpadButton_B) {
                quark::net::CancelConnect();
                thrd_join(connectThrd, nullptr);
                quark::net::Disconnect();
                m_infoImage->SetVisible(false);
                buildHostMenu();
                return;
            }
            svcSleepThread(15'000'000ull);
        }
        thrd_join(connectThrd, nullptr);

        if (!connectResult.load()) {
            m_infoImage->SetVisible(false);
            mainApp->CreateShowDialog("inst.net.connect_fail.title"_lang,
                                     "inst.net.connect_fail.desc"_lang,
                                     {"common.ok"_lang}, true);
            buildHostMenu();
            return;
        }

        quark::InitNetConsoleId();
        quark::net::cmd::AnnounceConsoleId(quark::GetNetConsoleId());

        std::atomic<bool> listDone{false};
        thrd_t listThrd;
        struct Args { netInstPage* self; std::atomic<bool>* done; };
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
                quark::net::Disconnect();
                thrd_join(listThrd, nullptr);
                buildHostMenu();
                return;
            }
            svcSleepThread(15'000'000ull);
        }
        thrd_join(listThrd, nullptr);

        if (m_menuEntries.empty()) {
            mainApp->CreateShowDialog("inst.net.top_info"_lang,
                                     "inst.net.no_drives"_lang,
                                     {"common.ok"_lang}, true);
            quark::net::Disconnect();
            buildHostMenu();
            return;
        }

        m_state = State::FileBrowse;
        m_infoImage->SetVisible(false);
        pageInfoText->SetText("inst.net.top_info2"_lang);
        m_consoleIdText->SetText("[" + quark::GetNetConsoleId() + "]");
        m_consoleIdText->SetX(1280 - 10 - m_consoleIdReservedW - m_consoleIdText->GetTextWidth());
        m_consoleIdText->SetVisible(true);
        setButtonsText("inst.net.buttons2"_lang);
        drawMenuItems(true);
        m_menu->SetSelectedIndex(0);
        m_menu->SetVisible(true);
    }

    bool netInstPage::isDirectory(int index) const {
        if (index < 0 || index >= (int)m_isDirectory.size()) return false;
        return m_isDirectory[index];
    }

    void netInstPage::drawMenuItems(bool clearSelection) {
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

    void netInstPage::toggleSelection(int index) {
        if (index < 0 || index >= (int)m_menuPaths.size()) return;
        if (isDirectory(index)) return;
        const std::string& path = m_menuPaths[index];
        auto it = std::find(m_selectedPaths.begin(), m_selectedPaths.end(), path);
        if (it != m_selectedPaths.end()) m_selectedPaths.erase(it);
        else m_selectedPaths.push_back(path);
        drawMenuItems(false);
    }

    void netInstPage::buildRootList() {
        m_menuEntries.clear();
        m_menuPaths.clear();
        m_isDirectory.clear();

        uint32_t spCount = 0;
        quark::net::cmd::GetSpecialPathCount(spCount);
        for (uint32_t i = 0; i < spCount; i++) {
            std::string name, path;
            if (quark::net::cmd::GetSpecialPath(i, name, path)) {
                m_menuEntries.push_back(name);
                m_menuPaths.push_back(path);
                m_isDirectory.push_back(true);
            }
        }

        uint32_t driveCount = 0;
        quark::net::cmd::GetDriveCount(driveCount);
        for (uint32_t i = 0; i < driveCount; i++) {
            std::string label, path;
            uint64_t total = 0, free = 0;
            if (quark::net::cmd::GetDriveInfo(i, label, path, total, free)) {
                m_menuEntries.push_back(label + " (" + path + ")");
                m_menuPaths.push_back(path);
                m_isDirectory.push_back(true);
            }
        }
    }

    void netInstPage::refreshCurrentDirectory() {
        m_menuEntries.clear();
        m_menuPaths.clear();
        m_isDirectory.clear();

        if (!m_pathStack.empty()) {
            m_menuEntries.push_back("..");
            m_menuPaths.push_back("..");
            m_isDirectory.push_back(true);
        }

        uint32_t dirCount = 0;
        quark::net::cmd::GetDirectoryCount(m_currentPath, dirCount);
        for (uint32_t i = 0; i < dirCount; i++) {
            std::string name;
            if (quark::net::cmd::GetDirectory(m_currentPath, i, name)) {
                if (inst::util::isMacMetadataName(name)) continue;
                m_menuEntries.push_back(name);
                m_menuPaths.push_back(m_currentPath + "/" + name);
                m_isDirectory.push_back(true);
            }
        }

        uint32_t fileCount = 0;
        quark::net::cmd::GetFileCount(m_currentPath, fileCount);
        for (uint32_t i = 0; i < fileCount; i++) {
            std::string name;
            if (quark::net::cmd::GetFile(m_currentPath, i, name)) {
                if (inst::util::isMacMetadataName(name)) continue;
                m_menuEntries.push_back(name);
                m_menuPaths.push_back(m_currentPath + "/" + name);
                m_isDirectory.push_back(false);
            }
        }
    }

    void netInstPage::navigateTo(const std::string& remotePath, bool pushHistory) {
        if (pushHistory) m_pathStack.push_back(m_currentPath);
        m_currentPath = remotePath;
        refreshCurrentDirectory();
        drawMenuItems(false);
        m_menu->SetSelectedIndex(0);
    }

    void netInstPage::navigateBack() {
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

    void netInstPage::startInstall() {
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
        netInstStuff::installTitleNet(m_selectedPaths, dialogResult);
    }

    void netInstPage::startNet() {
        m_selectedPaths.clear();
        m_pathStack.clear();
        m_currentPath.clear();
        inst::config::loadHosts();
        mainApp->LoadLayout(mainApp->netinstPage);
        buildHostMenu();
    }

    void netInstPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        (void)Up; (void)Held;

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
                    Down |= HidNpadButton_A;
                    m_hasLastTap   = false;
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

        if (m_state == State::HostSelect) {
            const int idx = m_menu->GetSelectedIndex();
            const bool isAddItem = (idx == (int)inst::config::savedHosts.size());

            if (Down & HidNpadButton_B) {
                quark::net::Disconnect();
                mainApp->LoadLayout(mainApp->mainPage);
                return;
            }

            if (Down & HidNpadButton_A) {
                if (isAddItem) {
                    addNewHost();
                } else {
                    connectToHost(inst::config::savedHosts[idx].ip);
                }
                return;
            }

            if (Down & HidNpadButton_Y) {
                addNewHost();
                return;
            }

            if ((Down & HidNpadButton_X) && !isAddItem) {
                deleteHost(idx);
                return;
            }
            return;
        }

        if (Down & HidNpadButton_B) {
            if (m_currentPath.empty() && m_pathStack.empty()) {
                quark::net::Disconnect();
                inst::config::loadHosts();
                buildHostMenu();
            } else {
                navigateBack();
            }
            return;
        }

        const int idx = m_menu->GetSelectedIndex();

        if (Down & HidNpadButton_A) {
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
            return;
        }

        if (Down & HidNpadButton_Y) {
            bool allSelected = true;
            for (int i = 0; i < (int)m_menuPaths.size(); i++) {
                if (!isDirectory(i) &&
                    std::find(m_selectedPaths.begin(), m_selectedPaths.end(),
                              m_menuPaths[i]) == m_selectedPaths.end()) {
                    allSelected = false; break;
                }
            }
            if (allSelected) drawMenuItems(true);
            else {
                for (int i = 0; i < (int)m_menuPaths.size(); i++) {
                    if (!isDirectory(i) &&
                        std::find(m_selectedPaths.begin(), m_selectedPaths.end(),
                                  m_menuPaths[i]) == m_selectedPaths.end())
                        m_selectedPaths.push_back(m_menuPaths[i]);
                }
                drawMenuItems(false);
            }
            return;
        }

        if (Down & HidNpadButton_Plus) {
            if (m_selectedPaths.empty()) {
                if (!isDirectory(idx)) {
                    toggleSelection(idx);
                    startInstall();
                }
                return;
            }
            startInstall();
            return;
        }
    }

}
