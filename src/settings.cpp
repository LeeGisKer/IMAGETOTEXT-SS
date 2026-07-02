// LeeOCR — settings dialog. A LeeGStudios.com project.
#include "settings.h"
#include "winutil.h"
#include <windows.h>
#include <commctrl.h>
#include <string>

namespace leeocr {
namespace {

const wchar_t* kSettingsClass = L"LeeOCR_Settings";
const wchar_t* kHotkeyClass   = L"msctls_hotkey32";
constexpr int ID_HOTKEY = 201;
constexpr int ID_LANG   = 202;
constexpr int ID_AUTO   = 203;
constexpr int ID_SAVE   = IDOK;      // 1 (default button)
constexpr int ID_CANCEL = IDCANCEL;  // 2
constexpr int ID_HOTKEY_PROBE = 0x7FFF;  // temp id for the pre-save availability probe

struct SettingsState {
    HFONT font = nullptr;
    HWND hk = nullptr, combo = nullptr, chk = nullptr;
    bool done = false;
    bool saved = false;
    Config* cfg = nullptr;
};
SettingsState* g_ss = nullptr;

// The RegisterHotKey MOD_* flags differ from the hotkey control's HOTKEYF_*.
unsigned modToHotkeyf(unsigned mods) {
    unsigned hf = 0;
    if (mods & MOD_SHIFT)   hf |= HOTKEYF_SHIFT;
    if (mods & MOD_CONTROL) hf |= HOTKEYF_CONTROL;
    if (mods & MOD_ALT)     hf |= HOTKEYF_ALT;
    return hf;
}
unsigned hotkeyfToMod(unsigned hf) {
    unsigned m = 0;
    if (hf & HOTKEYF_SHIFT)   m |= MOD_SHIFT;
    if (hf & HOTKEYF_CONTROL) m |= MOD_CONTROL;
    if (hf & HOTKEYF_ALT)     m |= MOD_ALT;
    return m;
}

void doSave(HWND hwnd) {
    SettingsState* s = g_ss;
    LRESULT hk = SendMessageW(s->hk, HKM_GETHOTKEY, 0, 0);
    BYTE vk = LOBYTE(LOWORD(hk));
    BYTE hf = HIBYTE(LOWORD(hk));
    if (vk != 0) {                       // keep old hotkey if none captured
        unsigned newMods = hotkeyfToMod(hf);
        if (vk != s->cfg->hotkeyVk || newMods != s->cfg->hotkeyMods) {
            // Probe availability before committing so a conflict is surfaced
            // inline (PRD 4.2) while the dialog is still open, not just via a
            // tray balloon after it closes. The live global hotkey lives on a
            // different window, so this probe tests the new chord honestly.
            if (!RegisterHotKey(hwnd, ID_HOTKEY_PROBE, newMods | MOD_NOREPEAT, vk)) {
                MessageBoxW(hwnd,
                    L"That key combination is already in use by another "
                    L"application. Please choose a different one.",
                    L"LeeOCR — Hotkey unavailable", MB_OK | MB_ICONWARNING);
                return;                  // keep the dialog open so the user retries
            }
            UnregisterHotKey(hwnd, ID_HOTKEY_PROBE);
        }
        s->cfg->hotkeyVk = vk;
        s->cfg->hotkeyMods = newMods;
    }
    int sel = static_cast<int>(SendMessageW(s->combo, CB_GETCURSEL, 0, 0));
    s->cfg->language = (sel == 1) ? "spa" : (sel == 2) ? "eng+spa" : "eng";
    s->cfg->autostart = (SendMessageW(s->chk, BM_GETCHECK, 0, 0) == BST_CHECKED);
    s->saved = true;
    DestroyWindow(hwnd);
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SettingsState* s = g_ss;
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_SAVE:   doSave(hwnd); return 0;
                case ID_CANCEL: s->saved = false; DestroyWindow(hwnd); return 0;
            }
            return 0;
        case WM_CLOSE:
            s->saved = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            s->done = true;
            PostMessageW(nullptr, WM_NULL, 0, 0);   // wake loop; no PostQuitMessage
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

bool showSettings(Config& cfg) {
    SettingsState st;
    st.cfg = &cfg;
    g_ss = &st;

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_HOTKEY_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kSettingsClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    POINT pc;
    GetCursorPos(&pc);
    HMONITOR mon = MonitorFromPoint(pc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(mon, &mi);

    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, kSettingsClass, L"LeeOCR — Settings",
                                style, mi.rcWork.left + 100, mi.rcWork.top + 100, 100, 100,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {                             // bail before building children/loop so
        UnregisterClassW(kSettingsClass, wc.hInstance);  // a failed create can't hang
        g_ss = nullptr;
        return false;
    }

    double sc = dpiForWindow(hwnd) / 96.0;
    auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };

    RECT rc{0, 0, S(430), S(250)};
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_TOPMOST);
    int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - winW) / 2;
    int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - winH) / 2;
    SetWindowPos(hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER);

    st.font = CreateFontW(-S(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HINSTANCE hi = wc.hInstance;

    CreateWindowExW(0, L"STATIC", L"Global hotkey:", WS_CHILD | WS_VISIBLE,
                    S(16), S(20), S(120), S(20), hwnd, nullptr, hi, nullptr);
    st.hk = CreateWindowExW(WS_EX_CLIENTEDGE, kHotkeyClass, L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                    S(150), S(16), S(250), S(26), hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_HOTKEY)), hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"Language:", WS_CHILD | WS_VISIBLE,
                    S(16), S(62), S(120), S(20), hwnd, nullptr, hi, nullptr);
    st.combo = CreateWindowExW(0, L"COMBOBOX", L"",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                    S(150), S(58), S(250), S(200), hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LANG)), hi, nullptr);
    st.chk = CreateWindowExW(0, L"BUTTON", L"Start LeeOCR with Windows",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    S(16), S(102), S(340), S(24), hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_AUTO)), hi, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Save",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    S(214), S(150), S(90), S(32), hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SAVE)), hi, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Cancel",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    S(310), S(150), S(90), S(32), hwnd,
                    reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CANCEL)), hi, nullptr);
    CreateWindowExW(0, L"STATIC", L"LeeOCR — LeeGStudios.com", WS_CHILD | WS_VISIBLE,
                    S(16), S(205), S(300), S(20), hwnd, nullptr, hi, nullptr);

    EnumChildWindows(hwnd, [](HWND c, LPARAM f) -> BOOL {
        SendMessageW(c, WM_SETFONT, static_cast<WPARAM>(f), TRUE);
        return TRUE;
    }, reinterpret_cast<LPARAM>(st.font));

    SendMessageW(st.hk, HKM_SETHOTKEY,
                 MAKEWORD(cfg.hotkeyVk, modToHotkeyf(cfg.hotkeyMods)), 0);
    SendMessageW(st.combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(st.combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Spanish"));
    SendMessageW(st.combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English + Spanish"));
    int sel = (cfg.language == "spa") ? 1 : (cfg.language == "eng+spa") ? 2 : 0;
    SendMessageW(st.combo, CB_SETCURSEL, sel, 0);
    SendMessageW(st.chk, BM_SETCHECK, cfg.autostart ? BST_CHECKED : BST_UNCHECKED, 0);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(st.hk);

    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (st.font) DeleteObject(st.font);
    UnregisterClassW(kSettingsClass, wc.hInstance);
    g_ss = nullptr;
    return st.saved;
}

}  // namespace leeocr
