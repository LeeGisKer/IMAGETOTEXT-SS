// LeeOCR — screen-region OCR to clipboard.  A LeeGStudios.com project.
// Slice 0: tray icon + global hotkey -> full-screen grab -> Tesseract -> clipboard.
#include <windows.h>
#include <shellapi.h>
#include <cstdint>
#include <string>
#include <vector>

#include "ocr.h"
#include "capture.h"
#include "clipboard.h"
#include "overlay.h"
#include "preview.h"
#include "config.h"
#include "settings.h"
#include "winutil.h"

namespace {

const wchar_t* kWndClass = L"LeeOCR_HiddenWindow";
const wchar_t* kAppName  = L"LeeOCR";

constexpr UINT WM_TRAYICON = WM_APP + 1;
constexpr int  HOTKEY_ID   = 1;
constexpr UINT IDM_CAPTURE   = 1001;
constexpr UINT IDM_ABOUT     = 1002;
constexpr UINT IDM_QUIT      = 1003;
constexpr UINT IDM_SETTINGS  = 1004;
constexpr UINT IDM_AUTOSTART = 1005;
constexpr UINT IDM_LANG_ENG    = 1010;
constexpr UINT IDM_LANG_SPA    = 1011;
constexpr UINT IDM_LANG_ENGSPA = 1012;

leeocr::Ocr     g_ocr;
leeocr::Config  g_config;
NOTIFYICONDATAW g_nid{};
bool            g_hotkeyOk = false;
bool            g_modalBusy = false;     // guards re-entrant capture / settings modal

std::wstring exeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t slash = p.find_last_of(L"\\/");
    return (slash == std::wstring::npos) ? L"." : p.substr(0, slash);
}

void trayBalloon(const wchar_t* msg, DWORD iconFlag = NIIF_INFO) {
    g_nid.uFlags = NIF_INFO;
    lstrcpynW(g_nid.szInfoTitle, kAppName, ARRAYSIZE(g_nid.szInfoTitle));
    lstrcpynW(g_nid.szInfo, msg, ARRAYSIZE(g_nid.szInfo));
    g_nid.dwInfoFlags = iconFlag;
    Shell_NotifyIconW(NIM_MODIFY, &g_nid);
}

// Reduce DLL-planting exposure for the portable install: drop the current
// working directory and PATH from the search path used by any DLL the app loads
// at runtime (LoadLibrary / delay-load), so a malicious DLL dropped in an
// untrusted CWD can't be preloaded ahead of the ones shipped beside the exe.
// Limitation: statically imported DLLs (tesseract, leptonica, MinGW runtime)
// are resolved by the loader before WinMain runs; those rely on the standard
// application-directory-first order plus running the app from a trusted folder.
void hardenDllSearchPath() {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    SetDllDirectoryW(L"");   // remove the current working directory from the search order
}

void enablePerMonitorV2Dpi() {
    HMODULE u = GetModuleHandleW(L"user32.dll");
    if (!u) return;
    using SetCtxFn = BOOL(WINAPI*)(void*);
    auto fn = reinterpret_cast<SetCtxFn>(
        GetProcAddress(u, "SetProcessDpiAwarenessContext"));
    if (fn) {
        // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 == (HANDLE)-4
        fn(reinterpret_cast<void*>(static_cast<intptr_t>(-4)));
    }
}

bool initOcrLang(const std::string& lang) {
    std::wstring base = exeDir();
    // Tesseract 5 treats datapath as the tessdata directory itself; try the
    // tessdata subfolder first, then the exe dir as a fallback.
    if (g_ocr.init(leeocr::wideToUtf8(base + L"\\tessdata"), lang)) return true;
    if (g_ocr.init(leeocr::wideToUtf8(base), lang)) return true;
    return false;
}

bool ensureOcrReady() {
    if (g_ocr.ready()) return true;
    return initOcrLang(g_config.language);
}

void setLanguage(const char* lang) {
    g_config.language = lang;
    leeocr::saveConfig(g_config);
    if (initOcrLang(g_config.language))
        trayBalloon(L"OCR language updated.");
    else
        trayBalloon(L"Could not load models for the selected language.", NIIF_ERROR);
}

void applyAutostart(bool enable) {
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
            L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
            0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    if (enable) {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::wstring q = L"\"";
        q += path;
        q += L"\"";
        RegSetValueExW(key, kAppName, 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(q.c_str()),
                       static_cast<DWORD>((q.size() + 1) * sizeof(wchar_t)));
    } else {
        RegDeleteValueW(key, kAppName);
    }
    RegCloseKey(key);
}

// Register the configured global hotkey. MOD_NOREPEAT stops a held-down chord
// from auto-repeating WM_HOTKEY (which would otherwise re-trigger capture).
bool registerHotkey(HWND hwnd) {
    if (g_hotkeyOk) UnregisterHotKey(hwnd, HOTKEY_ID);
    g_hotkeyOk = RegisterHotKey(hwnd, HOTKEY_ID,
                                g_config.hotkeyMods | MOD_NOREPEAT, g_config.hotkeyVk);
    return g_hotkeyOk;
}

void registerHotkeyFromConfig(HWND hwnd) {
    registerHotkey(hwnd);
    trayBalloon(g_hotkeyOk
                    ? L"Hotkey updated."
                    : L"That hotkey is in use. Capture still works from the tray icon.",
                g_hotkeyOk ? NIIF_INFO : NIIF_WARNING);
}

void openSettings(HWND hwnd) {
    // Same modal guard as doCapture: the tray menu stays reachable while a capture
    // or Settings modal loop is pumping, so a second IDM_SETTINGS would re-enter
    // showSettings and overwrite its file-scope state singleton (g_ss) -> crash.
    if (g_modalBusy) return;
    g_modalBusy = true;
    struct BusyGuard { ~BusyGuard() { g_modalBusy = false; } } busyGuard;

    leeocr::Config before = g_config;
    if (!leeocr::showSettings(g_config)) return;   // cancelled
    leeocr::saveConfig(g_config);
    if (g_config.hotkeyMods != before.hotkeyMods || g_config.hotkeyVk != before.hotkeyVk)
        registerHotkeyFromConfig(hwnd);
    if (g_config.language != before.language)
        initOcrLang(g_config.language);
    if (g_config.autostart != before.autostart)
        applyAutostart(g_config.autostart);
}

void doCapture(HWND) {
    // Re-entrancy guard: a held hotkey (auto-repeat), a second WM_HOTKEY, or the
    // tray "Capture" item clicked while a preview/overlay modal loop is pumping
    // would otherwise recursively re-enter here and corrupt the file-scope state
    // singletons (g_st/g_pv/g_ss). Ignore triggers while a capture is in flight.
    if (g_modalBusy) return;
    g_modalBusy = true;
    struct BusyGuard { ~BusyGuard() { g_modalBusy = false; } } busyGuard;

    if (!ensureOcrReady()) {
        trayBalloon(L"Could not load OCR models. Is the tessdata folder present?",
                    NIIF_ERROR);
        return;
    }
    for (;;) {
        leeocr::SelectionResult sel = leeocr::selectRegion();
        if (!sel.ok || sel.bmp.empty()) return;   // user cancelled the overlay
        std::string text = g_ocr.recognizeBmp(sel.bmp);
        while (!text.empty()) {
            char c = text.back();
            if (c == '\n' || c == '\r' || c == ' ' || c == '\t' || c == '\f')
                text.pop_back();
            else
                break;
        }
        if (text.empty()) {
            trayBalloon(L"No text found.");
            return;
        }
        leeocr::PreviewResult pr = leeocr::showPreview(text);
        if (pr.action == leeocr::PreviewAction::Recapture) continue;  // drag again
        if (pr.action == leeocr::PreviewAction::Copy) {
            bool copied = leeocr::setClipboardTextUtf8(pr.text);
            trayBalloon(copied ? L"Text copied to clipboard." : L"Failed to set clipboard.",
                        copied ? NIIF_INFO : NIIF_ERROR);
        }
        return;   // Copy or Discard -> done
    }
}

void showTrayMenu(HWND hwnd) {
    POINT pt;
    GetCursorPos(&pt);
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, IDM_CAPTURE, L"Capture  (Ctrl+Shift+T)");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    HMENU lang = CreatePopupMenu();
    AppendMenuW(lang, MF_STRING | (g_config.language == "eng" ? MF_CHECKED : 0),
                IDM_LANG_ENG, L"English");
    AppendMenuW(lang, MF_STRING | (g_config.language == "spa" ? MF_CHECKED : 0),
                IDM_LANG_SPA, L"Spanish");
    AppendMenuW(lang, MF_STRING | (g_config.language == "eng+spa" ? MF_CHECKED : 0),
                IDM_LANG_ENGSPA, L"English + Spanish");
    AppendMenuW(m, MF_POPUP, reinterpret_cast<UINT_PTR>(lang), L"Language");
    AppendMenuW(m, MF_STRING | (g_config.autostart ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(m, MF_STRING, IDM_SETTINGS, L"Settings…");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, IDM_ABOUT, L"About LeeOCR");
    AppendMenuW(m, MF_STRING, IDM_QUIT, L"Quit");
    SetForegroundWindow(hwnd);  // so the menu dismisses on focus loss
    TrackPopupMenu(m, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(m);
    PostMessageW(hwnd, WM_NULL, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_LBUTTONUP || LOWORD(lp) == WM_LBUTTONDBLCLK)
                doCapture(hwnd);         // PRD 4.1: single- OR double-click captures
            else if (LOWORD(lp) == WM_RBUTTONUP || LOWORD(lp) == WM_CONTEXTMENU)
                showTrayMenu(hwnd);
            return 0;
        case WM_HOTKEY:
            if (wp == HOTKEY_ID) doCapture(hwnd);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case IDM_CAPTURE: doCapture(hwnd); break;
                case IDM_LANG_ENG:    setLanguage("eng"); break;
                case IDM_LANG_SPA:    setLanguage("spa"); break;
                case IDM_LANG_ENGSPA: setLanguage("eng+spa"); break;
                case IDM_SETTINGS: openSettings(hwnd); break;
                case IDM_AUTOSTART:
                    g_config.autostart = !g_config.autostart;
                    leeocr::saveConfig(g_config);
                    applyAutostart(g_config.autostart);
                    break;
                case IDM_ABOUT:
                    MessageBoxW(hwnd,
                        L"LeeOCR 0.1\nScreen-region OCR to clipboard.\n\n"
                        L"A LeeGStudios.com project.",
                        L"About LeeOCR", MB_OK | MB_ICONINFORMATION);
                    break;
                case IDM_QUIT: DestroyWindow(hwnd); break;
            }
            return 0;
        case WM_DESTROY:
            Shell_NotifyIconW(NIM_DELETE, &g_nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    hardenDllSearchPath();
    enablePerMonitorV2Dpi();
    g_config = leeocr::loadConfig();
    applyAutostart(g_config.autostart);   // keep the Run key in sync with config

    // Single instance.
    HANDLE mtx = CreateMutexW(nullptr, TRUE, L"LeeOCR_SingleInstance_Mutex");
    if (mtx && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mtx);
        MessageBoxW(nullptr,
            L"LeeOCR is already running (see the system tray).",
            kAppName, MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = kWndClass;
    RegisterClassExW(&wc);

    // Hidden, never-shown top-level window to host the tray icon + messages.
    HWND hwnd = CreateWindowExW(0, kWndClass, kAppName, WS_OVERLAPPED,
                                0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;

    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = hwnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    lstrcpynW(g_nid.szTip, L"LeeOCR — Ctrl+Shift+T to capture",
              ARRAYSIZE(g_nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g_nid);

    registerHotkey(hwnd);
    trayBalloon(g_hotkeyOk
                    ? L"LeeOCR is running. Press Ctrl+Shift+T to capture."
                    : L"Hotkey Ctrl+Shift+T is in use. Use the tray icon to capture.",
                g_hotkeyOk ? NIIF_INFO : NIIF_WARNING);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hotkeyOk) UnregisterHotKey(hwnd, HOTKEY_ID);
    if (mtx) { ReleaseMutex(mtx); CloseHandle(mtx); }
    return 0;
}
