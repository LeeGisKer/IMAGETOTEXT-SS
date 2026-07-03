// LeeOCR — persisted settings. A LeeGStudios.com project.
#include "config.h"
#include <windows.h>
#include <shlobj.h>       // SHGetFolderPathW, SHCreateDirectoryExW
#include <cctype>
#include <fstream>
#include <sstream>

namespace leeocr {
namespace {

std::wstring configDir() {
    wchar_t buf[MAX_PATH];
    std::wstring dir = L".";
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buf)))
        dir = buf;
    return dir + L"\\LeeOCR";
}

std::string readFile(const std::wstring& path) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool getString(const std::string& j, const std::string& key, std::string& out) {
    size_t k = j.find("\"" + key + "\"");
    if (k == std::string::npos) return false;
    size_t colon = j.find(':', k);
    if (colon == std::string::npos) return false;
    size_t q1 = j.find('"', colon);
    if (q1 == std::string::npos) return false;
    size_t q2 = j.find('"', q1 + 1);
    if (q2 == std::string::npos) return false;
    out = j.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

bool getNumber(const std::string& j, const std::string& key, double& out) {
    size_t k = j.find("\"" + key + "\"");
    if (k == std::string::npos) return false;
    size_t colon = j.find(':', k);
    if (colon == std::string::npos) return false;
    size_t i = colon + 1;
    while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) ++i;
    size_t start = i;
    while (i < j.size() && (std::isdigit((unsigned char)j[i]) || j[i] == '-' ||
                            j[i] == '+' || j[i] == '.' || j[i] == 'e' || j[i] == 'E'))
        ++i;
    if (i == start) return false;
    try { out = std::stod(j.substr(start, i - start)); }
    catch (...) { return false; }
    return true;
}

bool getBool(const std::string& j, const std::string& key, bool& out) {
    size_t k = j.find("\"" + key + "\"");
    if (k == std::string::npos) return false;
    size_t colon = j.find(':', k);
    if (colon == std::string::npos) return false;
    size_t i = colon + 1;
    while (i < j.size() && (j[i] == ' ' || j[i] == '\t')) ++i;
    if (j.compare(i, 4, "true") == 0)  { out = true;  return true; }
    if (j.compare(i, 5, "false") == 0) { out = false; return true; }
    return false;
}

}  // namespace

std::wstring configPath() { return configDir() + L"\\config.json"; }

Config loadConfig() {
    Config c;
    std::string j = readFile(configPath());
    if (j.empty()) return c;
    std::string s;
    // Only accept a known language tag; a malformed/hand-edited value falls back
    // to the default rather than being passed on to Tesseract's Init.
    if (getString(j, "language", s) && (s == "eng" || s == "spa" || s == "eng+spa"))
        c.language = s;
    double d;
    if (getNumber(j, "hotkeyMods", d)) c.hotkeyMods = static_cast<unsigned>(d);
    if (getNumber(j, "hotkeyVk", d))   c.hotkeyVk   = static_cast<unsigned>(d);
    bool b;
    if (getBool(j, "autostart", b))    c.autostart  = b;
    return c;
}

bool saveConfig(const Config& c) {
    SHCreateDirectoryExW(nullptr, configDir().c_str(), nullptr);
    std::ostringstream o;
    o << "{\n";
    o << "  \"language\": \"" << c.language << "\",\n";
    o << "  \"hotkeyMods\": " << c.hotkeyMods << ",\n";
    o << "  \"hotkeyVk\": "   << c.hotkeyVk   << ",\n";
    o << "  \"autostart\": "  << (c.autostart ? "true" : "false") << "\n";
    o << "}\n";
    std::ofstream f(configPath().c_str(), std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << o.str();
    return static_cast<bool>(f);
}

}  // namespace leeocr
