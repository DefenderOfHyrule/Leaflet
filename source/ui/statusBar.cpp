#include <algorithm>
#include <cstdio>
#include <switch.h>
#include "ui/statusBar.hpp"
#include "util/config.hpp"

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    std::vector<StatusBar*> StatusBar::s_instances;

    namespace {
        constexpr int kFullHeight = 74;
        constexpr int kSlimHeight = 46;
        constexpr int kTitleX = 16;
        constexpr int kRightMargin = 10;
        constexpr int kCardWidth = 180;
        constexpr int kCardGap = 12;
        constexpr int kFullTimeY = 24;
        constexpr int kFullIconY = 50;
        constexpr int kFullCardsTopY = 24;
        constexpr int kFullBarY = 42;
        constexpr int kFullFreeY = 52;
        constexpr int kSlimTimeY = 12;
        constexpr int kSlimIconY = 17;
        constexpr int kNetSize = 6;
        constexpr int kWifiBarW = 4;
        constexpr int kWifiBarGap = 2;
        constexpr int kWifiWidth = (kWifiBarW * 3) + (kWifiBarGap * 2);
        constexpr int kWifiMaxH = 10;
        constexpr int kBatteryW = 24;
        constexpr int kBatteryH = 12;
        constexpr int kBatteryCapW = 3;
        constexpr int kIconGap = 6;
        constexpr int kIconsWidth = kNetSize + kIconGap + kWifiWidth + kIconGap + kBatteryW + kBatteryCapW;

        pu::ui::Color WithAlpha(pu::ui::Color c, u8 a) {
            return pu::ui::Color(c.R, c.G, c.B, a);
        }

        bool IsLightColor(pu::ui::Color c) {
            const float lum = (0.299f * c.R) + (0.587f * c.G) + (0.114f * c.B);
            return lum > 150.0f;
        }

        pu::ui::Color TextColorFor(pu::ui::Color surface) {
            return IsLightColor(surface) ? pu::ui::Color(26, 26, 26, 255) : pu::ui::Color(255, 255, 255, 255);
        }

        std::string FitText(const std::string& text, int maxWidth, int fontSize) {
            if (text.empty() || maxWidth <= 0)
                return text;
            static pu::ui::elm::TextBlock::Ref probe;
            static int probeFontSize = 0;
            if (!probe || probeFontSize != fontSize) {
                probe = pu::ui::elm::TextBlock::New(0, 0, text, fontSize);
                probeFontSize = fontSize;
            } else {
                probe->SetText(text);
            }
            if (probe->GetTextWidth() <= maxWidth)
                return text;
            std::string trimmed = text;
            while (!trimmed.empty()) {
                probe->SetText(trimmed + "...");
                if (probe->GetTextWidth() <= maxWidth)
                    return trimmed + "...";
                trimmed.pop_back();
            }
            return "...";
        }
    }

    StatusBar::Ref StatusBar::New(Mode mode, const std::string& title) {
        return Ref(new StatusBar(mode, title));
    }

    StatusBar::StatusBar(Mode mode, const std::string& title) : m_mode(mode), m_title(title) {
        const bool oled = inst::config::oledMode;
        const auto barColor = oled ? COLOR("#000000FF") : COLOR(inst::config::colorTopBar);
        const auto accentColor = oled ? COLOR("#FF4D4DFF") : WithAlpha(COLOR(inst::config::colorTileHighlight), 0xFF);
        this->m_textColor = TextColorFor(barColor);
        this->m_textDimColor = WithAlpha(this->m_textColor, 0xB4);

        const int height = this->GetHeight();
        const int titleFontSize = (this->m_mode == Mode::Full) ? 26 : 22;
        const int titleY = (this->m_mode == Mode::Full) ? 20 : 10;
        const int timeY = (this->m_mode == Mode::Full) ? kFullTimeY : kSlimTimeY;

        this->m_barRect = pu::ui::elm::Rectangle::New(0, 0, 1280, height, barColor);
        this->m_titleText = pu::ui::elm::TextBlock::New(kTitleX, titleY, "", titleFontSize);
        this->m_titleText->SetColor(this->m_textColor);
        this->m_timeText = pu::ui::elm::TextBlock::New(0, timeY, "--:--", (this->m_mode == Mode::Full) ? 22 : 20);
        this->m_timeText->SetColor(this->m_textColor);
        this->m_netIndicator = pu::ui::elm::Rectangle::New(0, 0, kNetSize, kNetSize, COLOR("#FF3B30FF"), 3);
        this->m_wifiBar1 = pu::ui::elm::Rectangle::New(0, 0, kWifiBarW, 4, WithAlpha(this->m_textColor, 0x55));
        this->m_wifiBar2 = pu::ui::elm::Rectangle::New(0, 0, kWifiBarW, 7, WithAlpha(this->m_textColor, 0x55));
        this->m_wifiBar3 = pu::ui::elm::Rectangle::New(0, 0, kWifiBarW, 10, WithAlpha(this->m_textColor, 0x55));
        this->m_batteryOutline = pu::ui::elm::Rectangle::New(0, 0, kBatteryW, kBatteryH, WithAlpha(this->m_textColor, 0x66));
        this->m_batteryFill = pu::ui::elm::Rectangle::New(0, 0, 0, kBatteryH - 2, COLOR("#4CD964FF"));
        this->m_batteryCap = pu::ui::elm::Rectangle::New(0, 0, kBatteryCapW, 6, WithAlpha(this->m_textColor, 0x66));

        if (this->m_mode == Mode::Full) {
            this->m_ipText = pu::ui::elm::TextBlock::New(0, kFullCardsTopY, "IP: --", 16);
            this->m_ipText->SetColor(this->m_textDimColor);
            this->m_sysLabelText = pu::ui::elm::TextBlock::New(0, 6, "System Memory", 16);
            this->m_sysLabelText->SetColor(this->m_textColor);
            this->m_sysFreeText = pu::ui::elm::TextBlock::New(0, kFullFreeY, "Free --", 16);
            this->m_sysFreeText->SetColor(this->m_textDimColor);
            this->m_sdLabelText = pu::ui::elm::TextBlock::New(0, 6, "microSD Card", 16);
            this->m_sdLabelText->SetColor(this->m_textColor);
            this->m_sdFreeText = pu::ui::elm::TextBlock::New(0, kFullFreeY, "Free --", 16);
            this->m_sdFreeText->SetColor(this->m_textDimColor);
            this->m_sysBarBack = pu::ui::elm::Rectangle::New(0, kFullBarY, kCardWidth, 6, WithAlpha(this->m_textColor, 0x33));
            this->m_sysBarFill = pu::ui::elm::Rectangle::New(0, kFullBarY, 0, 6, accentColor);
            this->m_sdBarBack = pu::ui::elm::Rectangle::New(0, kFullBarY, kCardWidth, 6, WithAlpha(this->m_textColor, 0x33));
            this->m_sdBarFill = pu::ui::elm::Rectangle::New(0, kFullBarY, 0, 6, accentColor);
        }

        if (appletGetAppletType() == AppletType_LibraryApplet) {
            const auto botColor = oled ? COLOR("#000000FF") : COLOR(inst::config::colorBotBar);
            this->m_appletText = pu::ui::elm::TextBlock::New(0, 681, "v" + inst::config::appVersion + " | Applet Mode", 18);
            this->m_appletText->SetColor(WithAlpha(TextColorFor(botColor), 0xC8));
            this->m_appletText->SetX(1280 - 10 - this->m_appletText->GetTextWidth());
        }

        this->applyTitle();
        s_instances.push_back(this);
    }

    StatusBar::~StatusBar() {
        s_instances.erase(std::remove(s_instances.begin(), s_instances.end(), this), s_instances.end());
    }

    int StatusBar::GetHeight() const {
        return (this->m_mode == Mode::Full) ? kFullHeight : kSlimHeight;
    }

    void StatusBar::Attach(pu::ui::Layout* layout) {
        if (!layout) return;
        layout->Add(this->m_barRect);
        layout->Add(this->m_titleText);
        layout->Add(this->m_timeText);
        layout->Add(this->m_netIndicator);
        layout->Add(this->m_wifiBar1);
        layout->Add(this->m_wifiBar2);
        layout->Add(this->m_wifiBar3);
        layout->Add(this->m_batteryOutline);
        layout->Add(this->m_batteryFill);
        layout->Add(this->m_batteryCap);
        if (this->m_appletText)
            layout->Add(this->m_appletText);
        if (this->m_mode == Mode::Full) {
            layout->Add(this->m_ipText);
            layout->Add(this->m_sysLabelText);
            layout->Add(this->m_sysFreeText);
            layout->Add(this->m_sdLabelText);
            layout->Add(this->m_sdFreeText);
            layout->Add(this->m_sysBarBack);
            layout->Add(this->m_sysBarFill);
            layout->Add(this->m_sdBarBack);
            layout->Add(this->m_sdBarFill);
        }
    }

    void StatusBar::SetTitle(const std::string& title) {
        this->m_title = title;
        this->applyTitle();
    }

    void StatusBar::applyTitle() {
        const int maxWidth = (this->m_mode == Mode::Full) ? 520 : 1000;
        const int fontSize = (this->m_mode == Mode::Full) ? 26 : 22;
        this->m_titleText->SetText(FitText(this->m_title, maxWidth, fontSize));
    }

    pu::ui::Color StatusBar::SurfaceTextColor(pu::ui::Color surface) {
        return TextColorFor(surface);
    }

    void StatusBar::UpdateAll(const Status& status) {
        for (auto* bar : s_instances)
            bar->apply(status);
    }

    void StatusBar::apply(const Status& status) {
        const int right = 1280 - kRightMargin;
        const int timeY = (this->m_mode == Mode::Full) ? kFullTimeY : kSlimTimeY;
        const int iconY = (this->m_mode == Mode::Full) ? kFullIconY : kSlimIconY;

        this->m_timeText->SetText(status.timeText);
        this->m_timeText->SetY(timeY);
        const int timeW = this->m_timeText->GetTextWidth();
        const int timeX = right - timeW;
        this->m_timeText->SetX(timeX);

        int iconsX;
        if (this->m_mode == Mode::Full) {
            iconsX = (timeW > 0) ? (timeX + (timeW - kIconsWidth) / 2) : (right - kIconsWidth);
        } else {
            iconsX = timeX - kCardGap - kIconsWidth;
        }
        if (iconsX < 0) iconsX = 0;
        const int netX = iconsX;
        const int wifiX = netX + kNetSize + kIconGap;
        const int batteryX = wifiX + kWifiWidth + kIconGap;

        this->m_netIndicator->SetX(netX);
        this->m_netIndicator->SetY(iconY + 2);
        this->m_netIndicator->SetColor(status.internetUp ? COLOR("#4CD964FF") : COLOR("#FF3B30FF"));

        const int wifiBaseY = iconY + kWifiMaxH;
        this->m_wifiBar1->SetX(wifiX);
        this->m_wifiBar1->SetY(wifiBaseY - 4);
        this->m_wifiBar2->SetX(wifiX + kWifiBarW + kWifiBarGap);
        this->m_wifiBar2->SetY(wifiBaseY - 7);
        this->m_wifiBar3->SetX(wifiX + (kWifiBarW + kWifiBarGap) * 2);
        this->m_wifiBar3->SetY(wifiBaseY - 10);
        const auto wifiOn = this->m_textColor;
        const auto wifiOff = WithAlpha(this->m_textColor, 0x55);
        const bool show = status.wifiConnected && status.wifiStrength > 0;
        this->m_wifiBar1->SetColor((show && status.wifiStrength >= 1) ? wifiOn : wifiOff);
        this->m_wifiBar2->SetColor((show && status.wifiStrength >= 2) ? wifiOn : wifiOff);
        this->m_wifiBar3->SetColor((show && status.wifiStrength >= 3) ? wifiOn : wifiOff);

        this->m_batteryOutline->SetX(batteryX);
        this->m_batteryOutline->SetY(iconY);
        this->m_batteryCap->SetX(batteryX + kBatteryW + 1);
        this->m_batteryCap->SetY(iconY + 3);
        int fillWidth = 0;
        if (status.batteryPct >= 0) {
            double ratio = std::max(0.0, std::min(1.0, static_cast<double>(status.batteryPct) / 100.0));
            fillWidth = static_cast<int>((kBatteryW - 2) * ratio);
            if (fillWidth < 2 && ratio > 0.0) fillWidth = 2;
        }
        this->m_batteryFill->SetX(batteryX + 1);
        this->m_batteryFill->SetY(iconY + 1);
        this->m_batteryFill->SetWidth(fillWidth);
        this->m_batteryFill->SetColor((status.batteryPct >= 0 && status.batteryPct <= 20) ? COLOR("#FF3B30FF") : COLOR("#4CD964FF"));

        if (this->m_mode != Mode::Full)
            return;

        const int cardsRight = timeX - kCardGap;
        const int sdX = cardsRight - kCardWidth;
        const int sysX = sdX - kCardGap - kCardWidth;

        std::string ip = status.ip;
        if (ip == "1.0.0.127") ip = "--";
        this->m_ipText->SetText("IP: " + ip);
        this->m_ipText->SetX((1280 - this->m_ipText->GetTextWidth()) / 2);
        this->m_ipText->SetY(kFullCardsTopY);

        this->m_sysLabelText->SetX(sysX);
        this->m_sysLabelText->SetY(kFullCardsTopY);
        this->m_sysFreeText->SetX(sysX);
        this->m_sysFreeText->SetY(kFullFreeY);
        this->m_sysBarBack->SetX(sysX);
        this->m_sysBarFill->SetX(sysX);
        this->m_sdLabelText->SetX(sdX);
        this->m_sdLabelText->SetY(kFullCardsTopY);
        this->m_sdFreeText->SetX(sdX);
        this->m_sdFreeText->SetY(kFullFreeY);
        this->m_sdBarBack->SetX(sdX);
        this->m_sdBarFill->SetX(sdX);

        auto setCard = [](pu::ui::elm::TextBlock::Ref freeText, pu::ui::elm::Rectangle::Ref barFill, s64 freeBytes, s64 totalBytes) {
            char buf[64] = {0};
            if (totalBytes > 0) {
                double freeGb = static_cast<double>(freeBytes) / (1024.0 * 1024.0 * 1024.0);
                std::snprintf(buf, sizeof(buf), "Free Space %.1f GB", freeGb);
            } else {
                std::snprintf(buf, sizeof(buf), "Free Space --");
            }
            freeText->SetText(buf);
            int width = 0;
            if (totalBytes > 0) {
                double used = static_cast<double>(totalBytes - freeBytes);
                if (used < 0.0) used = 0.0;
                double ratio = std::max(0.0, std::min(1.0, used / static_cast<double>(totalBytes)));
                width = static_cast<int>(kCardWidth * ratio);
                if (width < 2 && ratio > 0.0) width = 2;
            }
            barFill->SetWidth(width);
        };
        setCard(this->m_sysFreeText, this->m_sysBarFill, status.systemFree, status.systemTotal);
        setCard(this->m_sdFreeText, this->m_sdBarFill, status.sdFree, status.sdTotal);
    }
}
