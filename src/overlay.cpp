// LeeOCR — screen-region selection overlay. A LeeGStudios.com project.
#include "overlay.h"
#include "capture.h"       // virtualScreenBounds(), Rect
#include <windows.h>
#include <windowsx.h>      // GET_X_LPARAM / GET_Y_LPARAM

namespace leeocr {

namespace {

const wchar_t* kOverlayClass = L"LeeOCR_Overlay";

struct OverlayState {
    int vw = 0, vh = 0;               // client size == snapshot pixel size
    HDC snapDC = nullptr;             // bright frozen snapshot
    HBITMAP snapBmp = nullptr, oldSnap = nullptr;
    HDC dimDC = nullptr;              // pre-dimmed snapshot
    HBITMAP dimBmp = nullptr, oldDim = nullptr;
    HDC blackDC = nullptr;            // 1x1 black source for alpha dimming
    HBITMAP blackBmp = nullptr, oldBlack = nullptr;
    bool dragging = false;
    bool haveSel = false;
    POINT start{0, 0}, cur{0, 0};
    bool done = false;
    SelectionResult result;
};

OverlayState* g_st = nullptr;

RECT normalized(POINT a, POINT b) {
    RECT r;
    r.left   = a.x < b.x ? a.x : b.x;
    r.top    = a.y < b.y ? a.y : b.y;
    r.right  = a.x > b.x ? a.x : b.x;
    r.bottom = a.y > b.y ? a.y : b.y;
    return r;
}

SelectionResult cropSelection(int x, int y, int w, int h) {
    SelectionResult res;
    HDC sdc = GetDC(nullptr);
    HDC cropDC = CreateCompatibleDC(sdc);
    HBITMAP cropBmp = CreateCompatibleBitmap(sdc, w, h);
    HGDIOBJ old = SelectObject(cropDC, cropBmp);
    BitBlt(cropDC, 0, 0, w, h, g_st->snapDC, x, y, SRCCOPY);
    SelectObject(cropDC, old);                 // deselect before GetDIBits
    res.bmp = hbitmapToBmp24(cropBmp, w, h);
    res.ok = !res.bmp.empty();
    DeleteObject(cropBmp);
    DeleteDC(cropDC);
    ReleaseDC(nullptr, sdc);
    return res;
}

void paint(HWND hwnd) {
    OverlayState* s = g_st;
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    BitBlt(hdc, 0, 0, s->vw, s->vh, s->dimDC, 0, 0, SRCCOPY);  // dim everywhere
    if (s->dragging || s->haveSel) {
        RECT r = normalized(s->start, s->cur);
        int rw = r.right - r.left, rh = r.bottom - r.top;
        if (rw > 0 && rh > 0) {
            // Show the un-dimmed region inside the selection.
            BitBlt(hdc, r.left, r.top, rw, rh, s->snapDC, r.left, r.top, SRCCOPY);
            HPEN pen = CreatePen(PS_SOLID, 2, RGB(0, 160, 255));
            HGDIOBJ oldPen = SelectObject(hdc, pen);
            HGDIOBJ oldBrush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.left, r.top, r.right, r.bottom);
            SelectObject(hdc, oldPen);
            SelectObject(hdc, oldBrush);
            DeleteObject(pen);
        }
    }
    EndPaint(hwnd, &ps);
}

LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    OverlayState* s = g_st;
    switch (msg) {
        case WM_ERASEBKGND:
            return 1;                     // fully repainted in WM_PAINT; no flicker
        case WM_PAINT:
            paint(hwnd);
            return 0;
        case WM_LBUTTONDOWN:
            s->dragging = true;
            s->haveSel = false;
            s->start.x = GET_X_LPARAM(lp);
            s->start.y = GET_Y_LPARAM(lp);
            s->cur = s->start;
            SetCapture(hwnd);
            return 0;
        case WM_MOUSEMOVE:
            if (s->dragging) {
                s->cur.x = GET_X_LPARAM(lp);
                s->cur.y = GET_Y_LPARAM(lp);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            if (s->dragging) {
                s->dragging = false;
                ReleaseCapture();
                s->cur.x = GET_X_LPARAM(lp);
                s->cur.y = GET_Y_LPARAM(lp);
                RECT r = normalized(s->start, s->cur);
                int rw = r.right - r.left, rh = r.bottom - r.top;
                if (rw > 2 && rh > 2)
                    s->result = cropSelection(r.left, r.top, rw, rh);
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_RBUTTONUP:
            s->result.ok = false;
            DestroyWindow(hwnd);
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) {
                s->result.ok = false;
                DestroyWindow(hwnd);
            }
            return 0;
        case WM_DESTROY:
            s->done = true;               // the modal loop exits on this flag
            PostMessageW(nullptr, WM_NULL, 0, 0);  // wake loop; do NOT PostQuitMessage
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

SelectionResult selectRegion() {
    OverlayState st;
    g_st = &st;
    Rect vs = virtualScreenBounds();
    st.vw = vs.w;
    st.vh = vs.h;

    HDC screen = GetDC(nullptr);

    // Frozen bright snapshot of the whole virtual desktop.
    st.snapDC = CreateCompatibleDC(screen);
    st.snapBmp = CreateCompatibleBitmap(screen, vs.w, vs.h);
    st.oldSnap = static_cast<HBITMAP>(SelectObject(st.snapDC, st.snapBmp));
    BitBlt(st.snapDC, 0, 0, vs.w, vs.h, screen, vs.x, vs.y, SRCCOPY | CAPTUREBLT);

    // Pre-dimmed copy for the un-selected area.
    st.dimDC = CreateCompatibleDC(screen);
    st.dimBmp = CreateCompatibleBitmap(screen, vs.w, vs.h);
    st.oldDim = static_cast<HBITMAP>(SelectObject(st.dimDC, st.dimBmp));
    BitBlt(st.dimDC, 0, 0, vs.w, vs.h, st.snapDC, 0, 0, SRCCOPY);

    st.blackDC = CreateCompatibleDC(screen);
    st.blackBmp = CreateCompatibleBitmap(screen, 1, 1);
    st.oldBlack = static_cast<HBITMAP>(SelectObject(st.blackDC, st.blackBmp));
    RECT one{0, 0, 1, 1};
    FillRect(st.blackDC, &one, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.SourceConstantAlpha = 120;    // ~47% black veil
    AlphaBlend(st.dimDC, 0, 0, vs.w, vs.h, st.blackDC, 0, 0, 1, 1, bf);

    ReleaseDC(nullptr, screen);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = OverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kOverlayClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    // Not a true WS_EX_LAYERED window by design: the dim veil is pre-composited
    // into an offscreen DIB (dimDC) and BitBlt'd, giving an exact undimmed cutout
    // for the selection without per-pixel UpdateLayeredWindow overhead.
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kOverlayClass, L"LeeOCR",
        WS_POPUP, vs.x, vs.y, vs.w, vs.h, nullptr, nullptr, wc.hInstance, nullptr);
    if (hwnd) {                          // on creation failure, skip the loop so we
        ShowWindow(hwnd, SW_SHOW);       // never spin forever on a WM_DESTROY that
        SetForegroundWindow(hwnd);       // can't arrive (result stays ok=false)
        SetFocus(hwnd);

        MSG msg;
        while (!st.done && GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Cleanup GDI objects.
    if (st.snapDC)  { SelectObject(st.snapDC, st.oldSnap);   DeleteObject(st.snapBmp);  DeleteDC(st.snapDC); }
    if (st.dimDC)   { SelectObject(st.dimDC, st.oldDim);     DeleteObject(st.dimBmp);   DeleteDC(st.dimDC); }
    if (st.blackDC) { SelectObject(st.blackDC, st.oldBlack); DeleteObject(st.blackBmp); DeleteDC(st.blackDC); }
    UnregisterClassW(kOverlayClass, wc.hInstance);
    g_st = nullptr;
    return st.result;
}

}  // namespace leeocr
