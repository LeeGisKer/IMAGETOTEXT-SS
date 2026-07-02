// LeeOCR — screen-region selection overlay. A LeeGStudios.com project.
#pragma once
#include <cstdint>
#include <vector>

namespace leeocr {

struct SelectionResult {
    bool ok = false;
    std::vector<uint8_t> bmp;   // 24-bit BMP of the selected region (empty if cancelled)
};

// Shows a full-screen, dimmed, frozen snapshot of the virtual desktop and lets
// the user drag a selection rectangle (crosshair cursor; Esc / right-click
// cancels). Returns ok=true with the cropped region as a 24-bit BMP, or
// ok=false if the user cancelled.
SelectionResult selectRegion();

}  // namespace leeocr
