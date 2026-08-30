#pragma once

#include <string>
#include <vector>

namespace Language {
    void Load();
    std::string LanguageEntry(std::string key);
    std::vector<std::string> LanguageArray(std::string key);
    std::string GetRandomMsg();
    std::string GetShopHeaderLanguage();
}

inline std::string operator ""_lang (const char* key, size_t size) {
    return Language::LanguageEntry(std::string(key, size));
}
