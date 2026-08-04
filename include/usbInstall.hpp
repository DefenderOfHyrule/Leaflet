#pragma once
#include <vector>
#include <string>

namespace usbInstStuff {
    std::vector<std::string> BrowseRemote();
    void installTitleUsb(std::vector<std::string> ourNspList, int ourStorage);
}
