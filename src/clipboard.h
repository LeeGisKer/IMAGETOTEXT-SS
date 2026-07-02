// LeeOCR — clipboard helper. A LeeGStudios.com project.
#pragma once
#include <string>

namespace leeocr {
// Puts the given UTF-8 text on the Windows clipboard as CF_UNICODETEXT.
// Returns true on success.
bool setClipboardTextUtf8(const std::string& utf8);
}
