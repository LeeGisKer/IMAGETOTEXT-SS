// LeeOCR — small Win32 helpers shared across modules. A LeeGStudios.com project.
#pragma once
#include <windows.h>
#include <string>

namespace leeocr {

// UTF-8 -> UTF-16.
inline std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

// UTF-16 -> UTF-8.
inline std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                                nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(),
                        s.data(), n, nullptr, nullptr);
    return s;
}

// Per-window DPI. GetDpiForWindow is Win10 1607+; fall back to 96 (100%).
inline int dpiForWindow(HWND h) {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    using Fn = UINT(WINAPI*)(HWND);
    auto fn = reinterpret_cast<Fn>(GetProcAddress(u, "GetDpiForWindow"));
    return fn ? static_cast<int>(fn(h)) : 96;
}

}  // namespace leeocr
