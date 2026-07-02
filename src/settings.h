// LeeOCR — settings dialog. A LeeGStudios.com project.
#pragma once
#include "config.h"

namespace leeocr {

// Shows the modal Settings dialog initialized from `cfg`. If the user clicks
// Save, updates `cfg` in place (hotkey, language, autostart) and returns true.
// Returns false on Cancel/close.
bool showSettings(Config& cfg);

}  // namespace leeocr
