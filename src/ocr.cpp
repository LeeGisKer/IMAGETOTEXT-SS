// LeeOCR — Tesseract OCR wrapper. A LeeGStudios.com project.
#include "ocr.h"
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

namespace leeocr {

Ocr::~Ocr() {
    if (api_) {
        api_->End();
        delete api_;
        api_ = nullptr;
    }
}

bool Ocr::init(const std::string& datapath, const std::string& lang) {
    if (api_) { api_->End(); delete api_; api_ = nullptr; }
    api_ = new tesseract::TessBaseAPI();
    if (api_->Init(datapath.c_str(), lang.c_str()) != 0) {
        delete api_;
        api_ = nullptr;
        return false;
    }
    api_->SetPageSegMode(tesseract::PSM_AUTO);  // PSM 3
    return true;
}

// Preprocess a captured region to boost OCR accuracy on small, anti-aliased
// screen text: grayscale -> upscale (~3x) -> Otsu adaptive threshold.
// Returns a new Pix the caller must destroy, or nullptr to fall back to src.
static Pix* preprocess(Pix* src) {
    Pix* gray = pixConvertTo8(src, 0);          // any depth -> 8bpp gray
    if (!gray) return nullptr;

    l_int32 w = pixGetWidth(gray), h = pixGetHeight(gray);
    long long px = static_cast<long long>(w) * h;
    float scale = 3.0f;                         // cap for large selections
    if (px > 9000000LL) scale = 1.5f;
    else if (px > 4000000LL) scale = 2.0f;

    // PRD 4.4 specifies bicubic upscaling; Leptonica exposes no bicubic grayscale
    // scaler, so we use linear interpolation (pixScaleGrayLI) as the closest
    // practical equivalent. Deliberate substitution, not an oversight.
    Pix* scaled = pixScaleGrayLI(gray, scale, scale);
    pixDestroy(&gray);
    if (!scaled) return nullptr;

    Pix* bin = nullptr;
    if (pixOtsuAdaptiveThreshold(scaled, 2000, 2000, 0, 0, 0.1f, nullptr, &bin) == 0
        && bin) {
        pixDestroy(&scaled);
        return bin;                             // 1bpp binarized
    }
    return scaled;                              // fall back to gray+scaled
}

std::string Ocr::recognizeBmp(const std::vector<uint8_t>& bmp) {
    if (!api_ || bmp.empty()) return "";
    Pix* src = pixReadMemBmp(bmp.data(), bmp.size());
    if (!src) return "";

    Pix* proc = preprocess(src);
    Pix* use = proc ? proc : src;
    api_->SetImage(use);
    api_->SetSourceResolution(300);   // we upscaled; avoid low-DPI warnings
    char* out = api_->GetUTF8Text();
    std::string text = out ? out : "";
    delete[] out;                     // Tesseract allocates with new[]
    api_->Clear();

    if (proc && proc != src) pixDestroy(&proc);
    pixDestroy(&src);
    return text;
}

}  // namespace leeocr
