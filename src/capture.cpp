// LeeOCR — screen capture. A LeeGStudios.com project.
#include "capture.h"
#include <windows.h>

namespace leeocr {

Rect virtualScreenBounds() {
    Rect r;
    r.x = GetSystemMetrics(SM_XVIRTUALSCREEN);
    r.y = GetSystemMetrics(SM_YVIRTUALSCREEN);
    r.w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    r.h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return r;
}

// Build a 24-bit BMP (in-memory .bmp file bytes) from a device bitmap that is
// NOT currently selected into any DC. Used to crop the overlay snapshot.
std::vector<uint8_t> hbitmapToBmp24(HBITMAP hbm, int w, int h) {
    std::vector<uint8_t> result;
    if (w <= 0 || h <= 0 || !hbm) return result;

    HDC dc = GetDC(nullptr);
    const int stride = ((w * 3 + 3) & ~3);
    const int imageSize = stride * h;

    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = h;          // positive => bottom-up DIB
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;
    bi.bmiHeader.biSizeImage = imageSize;

    std::vector<uint8_t> pixels(imageSize);
    int scanned = GetDIBits(dc, hbm, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    if (scanned == 0) return result;

    BITMAPFILEHEADER fh{};
    fh.bfType = 0x4D42;  // "BM"
    fh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fh.bfSize = fh.bfOffBits + imageSize;

    result.reserve(fh.bfSize);
    const uint8_t* fhp = reinterpret_cast<const uint8_t*>(&fh);
    result.insert(result.end(), fhp, fhp + sizeof(BITMAPFILEHEADER));
    const uint8_t* bihp = reinterpret_cast<const uint8_t*>(&bi.bmiHeader);
    result.insert(result.end(), bihp, bihp + sizeof(BITMAPINFOHEADER));
    result.insert(result.end(), pixels.begin(), pixels.end());
    return result;
}

}  // namespace leeocr
