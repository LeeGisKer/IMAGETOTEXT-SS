// LeeOCR — editable preview window. A LeeGStudios.com project.
#include "preview.h"
#include "winutil.h"
#include <windows.h>
#include <string>

namespace leeocr {
namespace {

const wchar_t* kPreviewClass = L"LeeOCR_Preview";
constexpr int ID_EDIT      = 101;
constexpr int ID_COPY      = IDOK;       // 1  (default button -> Enter)
constexpr int ID_DISCARD   = IDCANCEL;   // 2  (Esc)
constexpr int ID_RECAPTURE = 100;

struct PreviewState {
    HWND edit = nullptr;
    HFONT font = nullptr;
    bool done = false;
    PreviewAction action = PreviewAction::Discard;
    std::string text;
};
PreviewState* g_pv = nullptr;

std::string readEditText(HWND edit) {
    int len = GetWindowTextLengthW(edit);
    if (len <= 0) return "";
    std::wstring w(len + 1, L'\0');            // room for the null terminator
    int got = GetWindowTextW(edit, w.data(), len + 1);
    w.resize(got);                             // trim to the actual copied length
    return wideToUtf8(w);
}

LRESULT CALLBACK PreviewProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PreviewState* s = g_pv;
    switch (msg) {
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_COPY:
                    s->action = PreviewAction::Copy;
                    s->text = readEditText(s->edit);
                    DestroyWindow(hwnd);
                    return 0;
                case ID_DISCARD:
                    s->action = PreviewAction::Discard;
                    DestroyWindow(hwnd);
                    return 0;
                case ID_RECAPTURE:
                    s->action = PreviewAction::Recapture;
                    DestroyWindow(hwnd);
                    return 0;
            }
            return 0;
        case WM_CLOSE:
            s->action = PreviewAction::Discard;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            s->done = true;          // the modal loop exits on this flag
            PostMessageW(nullptr, WM_NULL, 0, 0);  // wake loop; do NOT PostQuitMessage
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

PreviewResult showPreview(const std::string& initialTextUtf8) {
    PreviewState st;
    g_pv = &st;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PreviewProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kPreviewClass;
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
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, kPreviewClass,
                                L"LeeOCR — recognized text", style,
                                mi.rcWork.left + 100, mi.rcWork.top + 100, 100, 100,
                                nullptr, nullptr, wc.hInstance, nullptr);
    if (!hwnd) {                             // bail before building children/loop so
        UnregisterClassW(kPreviewClass, wc.hInstance);  // a failed create can't hang
        g_pv = nullptr;
        return {};                           // default action == Discard
    }

    double sc = dpiForWindow(hwnd) / 96.0;
    auto S = [&](int v) { return static_cast<int>(v * sc + 0.5); };

    RECT rc{0, 0, S(560), S(372)};
    AdjustWindowRectEx(&rc, style, FALSE, WS_EX_TOPMOST);
    int winW = rc.right - rc.left, winH = rc.bottom - rc.top;
    int workW = mi.rcWork.right - mi.rcWork.left, workH = mi.rcWork.bottom - mi.rcWork.top;
    int x = mi.rcWork.left + (workW - winW) / 2;
    int y = mi.rcWork.top + (workH - winH) / 2;
    SetWindowPos(hwnd, nullptr, x, y, winW, winH, SWP_NOZORDER);

    st.font = CreateFontW(-S(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                          DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                          CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HWND edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
        utf8ToWide(initialTextUtf8).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
        S(12), S(12), S(536), S(280), hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_EDIT)), wc.hInstance, nullptr);
    st.edit = edit;

    HWND bRecap = CreateWindowExW(0, L"BUTTON", L"Recapture",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(12), S(304), S(120), S(32), hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RECAPTURE)), wc.hInstance, nullptr);
    HWND bDiscard = CreateWindowExW(0, L"BUTTON", L"Discard",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
        S(300), S(304), S(112), S(32), hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_DISCARD)), wc.hInstance, nullptr);
    HWND bCopy = CreateWindowExW(0, L"BUTTON", L"Copy",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        S(424), S(304), S(124), S(32), hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_COPY)), wc.hInstance, nullptr);
    HWND foot = CreateWindowExW(0, L"STATIC", L"LeeOCR — LeeGStudios.com",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        S(12), S(344), S(320), S(20), hwnd, nullptr, wc.hInstance, nullptr);

    HWND ctrls[] = {edit, bRecap, bDiscard, bCopy, foot};
    for (HWND c : ctrls) SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(st.font), TRUE);

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    SetFocus(edit);
    SendMessageW(edit, EM_SETSEL, 0, -1);   // select all

    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        // Plain Enter = Copy. A multiline edit would otherwise swallow it, and
        // IsDialogMessage's default-button routing doesn't fire reliably here.
        // Ctrl+Enter falls through so the edit can insert a line break.
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN
            && (GetKeyState(VK_CONTROL) & 0x8000) == 0) {
            SendMessageW(hwnd, WM_COMMAND, MAKEWPARAM(ID_COPY, 0), 0);
            continue;
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (st.font) DeleteObject(st.font);
    UnregisterClassW(kPreviewClass, wc.hInstance);
    g_pv = nullptr;

    PreviewResult res;
    res.action = st.action;
    res.text = st.text;
    return res;
}

}  // namespace leeocr
