// LeeOCR — screen capture. A LeeGStudios.com project.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>

namespace leeocr {

struct Rect { int x = 0, y = 0, w = 0, h = 0; };

// Bounds of the whole virtual desktop (all monitors), in physical pixels.
Rect virtualScreenBounds();

// Build a 24-bit BMP (in-memory .bmp file bytes) from a device bitmap that is
// NOT currently selected into any DC. Returns an empty vector on failure.
std::vector<uint8_t> hbitmapToBmp24(HBITMAP hbm, int w, int h);

}  // namespace leeocr
