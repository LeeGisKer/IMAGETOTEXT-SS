// LeeOCR — editable preview window. A LeeGStudios.com project.
#pragma once
#include <string>

namespace leeocr {

enum class PreviewAction { Copy, Discard, Recapture };

struct PreviewResult {
    PreviewAction action = PreviewAction::Discard;
    std::string text;   // the (possibly edited) UTF-8 text, valid when action == Copy
};

// Shows an always-on-top, editable preview of `initialTextUtf8`. The text box is
// focused with all text selected. Enter / Copy copies & closes; Esc / Discard
// closes without copying; Recapture asks the caller to re-run the overlay.
PreviewResult showPreview(const std::string& initialTextUtf8);

}  // namespace leeocr
