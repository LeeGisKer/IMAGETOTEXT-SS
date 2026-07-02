// LeeOCR — clipboard helper. A LeeGStudios.com project.
#include "clipboard.h"
#include <windows.h>

namespace leeocr {

bool setClipboardTextUtf8(const std::string& utf8) {
    // UTF-8 -> UTF-16.
    int wlen = 0;
    if (!utf8.empty()) {
        wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                                   static_cast<int>(utf8.size()), nullptr, 0);
        if (wlen <= 0) return false;
    }

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE,
                               static_cast<SIZE_T>(wlen + 1) * sizeof(wchar_t));
    if (!hMem) return false;

    wchar_t* dst = static_cast<wchar_t*>(GlobalLock(hMem));
    if (!dst) { GlobalFree(hMem); return false; }
    if (wlen > 0) {
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
                            static_cast<int>(utf8.size()), dst, wlen);
    }
    dst[wlen] = L'\0';
    GlobalUnlock(hMem);

    if (!OpenClipboard(nullptr)) { GlobalFree(hMem); return false; }
    EmptyClipboard();
    if (!SetClipboardData(CF_UNICODETEXT, hMem)) {
        CloseClipboard();
        GlobalFree(hMem);
        return false;
    }
    // On success the clipboard owns hMem; do not free it.
    CloseClipboard();
    return true;
}

}  // namespace leeocr
