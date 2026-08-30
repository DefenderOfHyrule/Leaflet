#pragma once
#include <string>

namespace quark {

    const std::string& GetConsoleId();
    void InitConsoleId();
    void RegenerateConsoleId();
    bool SetCustomConsoleId(const std::string& id);

    const std::string& GetNetConsoleId();
    void InitNetConsoleId();
    void ClearNetConsoleId();

}
