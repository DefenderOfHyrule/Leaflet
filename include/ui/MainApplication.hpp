#pragma once
#include <pu/Plutonium>
#include "ui/mainPage.hpp"
#include "ui/sdInstPage.hpp"
#include "ui/usbInstPage.hpp"
#include "ui/netInstPage.hpp"
#include "ui/hddInstPage.hpp"
#include "ui/instPage.hpp"
#include "ui/optionsPage.hpp"
#include "ui/fileBrowserPage.hpp"
#include "ui/gcInstPage.hpp"

#include "ui/themeManagerPage.hpp"

namespace inst::ui {
    class MainApplication : public pu::ui::Application {
        public:
            using Application::Application;
            PU_SMART_CTOR(MainApplication)
            void OnLoad() override;
            void rebuildLayouts(int restoreSection = 1);
            pu::ui::Layout::Ref GetCurrentLayout() const { return this->lyt; }
            MainPage::Ref mainPage;
            sdInstPage::Ref sdinstPage;
            usbInstPage::Ref usbinstPage;
    netInstPage::Ref netinstPage;
            hddInstPage::Ref hddinstPage;
            instPage::Ref instpage;
            optionsPage::Ref optionspage;
            fileBrowserPage::Ref filebrowserPage;
            gcInstPage::Ref gcinstPage;
            themeManagerPage::Ref m_themePage;
    };

    // global application instance defined in MainApplication.cpp
    extern MainApplication* mainApp;
}
