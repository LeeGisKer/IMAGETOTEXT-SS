// LeeOCR — Tesseract OCR wrapper. A LeeGStudios.com project.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace tesseract { class TessBaseAPI; }

namespace leeocr {

class Ocr {
public:
    Ocr() = default;
    ~Ocr();
    Ocr(const Ocr&) = delete;
    Ocr& operator=(const Ocr&) = delete;

    // datapath = directory that CONTAINS the "tessdata" folder.
    // lang = "eng", "spa", or a combination like "eng+spa".
    bool init(const std::string& datapath, const std::string& lang);
    bool ready() const { return api_ != nullptr; }

    // OCR an in-memory BMP image. Returns UTF-8 text (may be empty).
    std::string recognizeBmp(const std::vector<uint8_t>& bmp);

private:
    tesseract::TessBaseAPI* api_ = nullptr;
};

}  // namespace leeocr
