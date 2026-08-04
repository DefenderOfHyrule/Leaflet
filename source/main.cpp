#include <thread>
#include "switch.h"
#include "util/debug.h"
#include "util/error.hpp"
#include "ui/MainApplication.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "gcInstall.hpp"

using namespace pu::ui::render;
int main()
{
    bool appInitialized = false;
    try {
        debugLogReset();
        inst::util::initApp();
        appInitialized = true;
        auto renderer = Renderer::New(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER,
            RendererInitOptions::RendererNoSound, RendererHardwareFlags);
        auto main = inst::ui::MainApplication::New(renderer);
        std::thread updateThread;
        if (inst::util::getIPAddress() != "1.0.0.127")
            updateThread = std::thread(inst::util::checkForAppUpdate);
        inst::gc::Init();
        main->Prepare();
        main->ShowWithFadeIn();
        if (updateThread.joinable())
            updateThread.join();
    } catch (std::exception& e) {
        LOG_DEBUG("An error occurred:\n%s", e.what());
    } catch (...) {
        LOG_DEBUG("An unknown error occurred during startup.");
    }
    inst::gc::Exit();
    if (appInitialized) {
        inst::util::deinitApp();
    }
    return 0;
}
