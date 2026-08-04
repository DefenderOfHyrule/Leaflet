#pragma once
#include <string>

namespace quark {

    const std::string& GetConsoleId();
    void InitConsoleId();

    const std::string& GetNetConsoleId();
    void InitNetConsoleId();
    void ClearNetConsoleId();

}
