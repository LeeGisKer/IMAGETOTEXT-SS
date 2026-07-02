// LeeOCR — persisted settings. A LeeGStudios.com project.
#pragma once
#include <string>

namespace leeocr {

struct Config {
    std::string language = "eng";        // "eng", "spa", or "eng+spa"
    unsigned hotkeyMods  = 0x0002 | 0x0004;  // MOD_CONTROL | MOD_SHIFT
    unsigned hotkeyVk    = 0x54;          // 'T'
    bool autostart       = false;
};

// Path to %APPDATA%\LeeOCR\config.json.
std::wstring configPath();

// Loads config, returning defaults for a missing/partial/corrupt file.
Config loadConfig();

// Writes config.json (creating %APPDATA%\LeeOCR if needed). Returns true on success.
bool saveConfig(const Config& c);

}  // namespace leeocr
