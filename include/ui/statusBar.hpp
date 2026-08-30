#pragma once
#include <memory>
#include <string>
#include <vector>
#include <pu/Plutonium>

namespace inst::ui {
    class StatusBar {
        public:
            enum class Mode { Full, Slim };
            using Ref = std::shared_ptr<StatusBar>;

            struct Status {
                std::string timeText = "--:--";
                std::string ip = "--";
                bool internetUp = false;
                bool wifiConnected = false;
                u32 wifiStrength = 0;
                int batteryPct = -1;
                s64 systemTotal = 0;
                s64 systemFree = 0;
                s64 sdTotal = 0;
                s64 sdFree = 0;
            };

            static Ref New(Mode mode, const std::string& title);
            ~StatusBar();

            void Attach(pu::ui::Layout* layout);
            void SetTitle(const std::string& title);
            int GetHeight() const;

            static void UpdateAll(const Status& status);
            static pu::ui::Color SurfaceTextColor(pu::ui::Color surface);

        private:
            StatusBar(Mode mode, const std::string& title);
            void applyTitle();
            void apply(const Status& status);

            Mode m_mode;
            std::string m_title;
            pu::ui::Color m_textColor;
            pu::ui::Color m_textDimColor;

            pu::ui::elm::Rectangle::Ref m_barRect;
            pu::ui::elm::TextBlock::Ref m_titleText;
            pu::ui::elm::TextBlock::Ref m_appletText;
            pu::ui::elm::TextBlock::Ref m_timeText;
            pu::ui::elm::TextBlock::Ref m_ipText;
            pu::ui::elm::TextBlock::Ref m_sysLabelText;
            pu::ui::elm::TextBlock::Ref m_sysFreeText;
            pu::ui::elm::TextBlock::Ref m_sdLabelText;
            pu::ui::elm::TextBlock::Ref m_sdFreeText;
            pu::ui::elm::Rectangle::Ref m_sysBarBack;
            pu::ui::elm::Rectangle::Ref m_sysBarFill;
            pu::ui::elm::Rectangle::Ref m_sdBarBack;
            pu::ui::elm::Rectangle::Ref m_sdBarFill;
            pu::ui::elm::Rectangle::Ref m_netIndicator;
            pu::ui::elm::Rectangle::Ref m_wifiBar1;
            pu::ui::elm::Rectangle::Ref m_wifiBar2;
            pu::ui::elm::Rectangle::Ref m_wifiBar3;
            pu::ui::elm::Rectangle::Ref m_batteryOutline;
            pu::ui::elm::Rectangle::Ref m_batteryFill;
            pu::ui::elm::Rectangle::Ref m_batteryCap;

            static std::vector<StatusBar*> s_instances;
    };
}
