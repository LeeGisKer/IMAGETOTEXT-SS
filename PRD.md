# LeeOCR — Product Requirements Document

> Local Windows screen-region OCR utility. Press a hotkey, drag a box over any
> text on screen, get clean editable text ready to paste.
>
> Built by **[LeeGStudios.com](https://leegstudios.com)**

---

## 1. Summary

LeeOCR is a lightweight system-tray application for Windows 10/11. It lives
silently in the tray, launches at login, and listens for a global hotkey. When
triggered, it freezes the screen, lets the user drag a rectangle over any text,
runs Tesseract OCR on that region, and shows the recognized text in a small
editable preview window. The user fixes any slips and confirms — the text lands
on the clipboard, ready to paste anywhere.

No cloud, no network, no telemetry. Everything runs locally.

## 2. Goals / Non-goals

**Goals**
- One-keystroke capture of on-screen text from *anywhere* (any app, any monitor).
- Accurate OCR on typical screen text (UI labels, code, docs, chat).
- Zero-friction "copy and paste" output with a chance to correct errors first.
- Small, self-contained, portable folder install. No installer required.

**Non-goals (v1)**
- No PDF/file batch OCR — this is screen-region only.
- No cloud OCR / online services.
- No handwriting or heavy document-layout reconstruction.
- No installer/MSI (portable folder is the ship target).

## 3. Users & primary flow

Single local user (the app owner). Primary loop:

1. User presses the global hotkey (default `Ctrl+Shift+T`) from any app.
2. Screen dims to a frozen snapshot; cursor becomes a crosshair.
3. User drags a rectangle over the target text; releases mouse.
4. LeeOCR pre-processes and OCRs that region.
5. An editable preview window appears with the recognized text (auto-focused,
   select-all). User optionally edits.
6. User presses `Enter` / clicks **Copy** → text goes to clipboard, window closes.
   `Esc` discards. **Recapture** re-opens the overlay.
7. User pastes with `Ctrl+V` wherever they want.

## 4. Functional requirements

### 4.1 Tray & lifecycle
- Runs as a background tray app (`Shell_NotifyIcon`); no taskbar window at rest.
- Right-click tray menu: **Language** submenu (English / Spanish / English+Spanish),
  **Start with Windows** toggle, **Settings…**, **About**, **Quit**.
- Left-click / double-click tray icon triggers a capture (same as hotkey).
- Autostart via `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (toggleable).
- Single-instance: a second launch focuses/notifies the existing instance instead
  of registering a duplicate hotkey.

### 4.2 Global hotkey
- Default `Ctrl+Shift+T`, registered via `RegisterHotKey`.
- Rebindable in Settings (capture a key chord).
- **Graceful fallback:** if registration fails (hotkey already taken by another
  app), show a tray balloon warning and keep running; capture still works from
  the tray icon. Settings surfaces the conflict so the user can pick another key.

### 4.3 Region-capture overlay (fiddliest subsystem)
- On trigger: snapshot the **entire virtual screen** (all monitors) to a frozen
  bitmap via `BitBlt`, so selection happens against a static image (no flicker,
  no live-content race).
- Display a full-virtual-screen layered/topmost window showing the dimmed snapshot.
- Crosshair cursor; live rubber-band rectangle follows the drag.
- Release → crop the selected rectangle from the snapshot → hand to OCR.
- `Esc` or right-click cancels with no clipboard change.
- **Per-monitor DPI-v2 aware** (`SetProcessDpiAwarenessContext`) so pixel
  coordinates and captured pixels are true across mixed-DPI monitors.

### 4.4 OCR
- Engine: **Tesseract** (bundled, dynamically linked).
- Trained data: **`tessdata_best`** float-LSTM models for `eng` and `spa`
  (NOT the pacman `tesseract-data` v4.1 standard packages — those are a different,
  lower-accuracy variant than the "best" model chosen for this project).
- Active language(s) selectable at runtime: `eng`, `spa`, or combined `eng+spa`.
- Page-seg mode: PSM 3 (auto).
- Pre-processing pipeline (Leptonica) applied to the cropped region before OCR:
  1. Upscale ~3× (bicubic).
  2. Convert to grayscale.
  3. Adaptive/Otsu threshold to clean anti-aliasing.
- If OCR yields no/empty text: tray balloon "No text found", clipboard untouched.

### 4.5 Preview / output window
- Small always-on-top window centered on the active monitor.
- Multiline edit control, pre-filled with recognized text, focused + select-all.
- `Enter` or **Copy** button → set clipboard (UTF-16 Unicode) → close.
- `Esc` or **Discard** → close, clipboard untouched.
- **Recapture** button → close preview, re-open the capture overlay.
- Footer credit: "LeeOCR — LeeGStudios.com".

### 4.6 Settings window
- Rebind hotkey (press-a-chord capture).
- Default language selector (eng / spa / eng+spa).
- "Start with Windows" toggle (writes/removes the Run key).
- OCR options (PSM, model note) — advanced, optional.
- Persisted to `%APPDATA%\LeeOCR\config.json`.

### 4.7 Config
- File: `%APPDATA%\LeeOCR\config.json`.
- Keys: `language`, `hotkeyMods`, `hotkeyVk`, `autostart`.
- Created with sane defaults on first run; tolerant of missing/partial files.

## 5. Non-functional requirements
- Capture-to-preview latency: < ~1s for a typical small region.
- Idle footprint: minimal CPU, modest RAM (Tesseract API lazily initialized).
- Fully offline; no network calls ever.
- Portable: runs from any folder with its DLLs + `tessdata\` beside the exe.

## 6. Architecture & toolchain

**Toolchain (resolved):** MSYS2 **UCRT64 + GCC 15.2** with **CMake + Ninja**.
> Rationale: Visual Studio 2022 is installed but `MSVC + vcpkg` would require a
> long first-time source build of Tesseract + Leptonica (the #1 risk in the
> original spec). MSYS2/pacman provides prebuilt Tesseract + Leptonica binaries,
> eliminating that risk. Every API this app uses is classic Win32 and builds
> cleanly under GCC. This supersedes the earlier `MSVC+vcpkg` default (which
> was never a locked user decision). **One toolchain end-to-end** — all MinGW;
> MSYS2 GCC-built libs are not ABI-compatible with an MSVC exe.

**Modules**
| File | Responsibility |
|---|---|
| `main.cpp` | WinMain, DPI setup, tray icon, hotkey, message loop, orchestration |
| `capture.{h,cpp}` | Virtual-screen snapshot, overlay window, rubber-band selection, crop |
| `ocr.{h,cpp}` | Tesseract wrapper: init, Leptonica preprocessing, recognize |
| `clipboard.{h,cpp}` | Set clipboard Unicode text |
| `preview.{h,cpp}` | Editable preview window |
| `settings.{h,cpp}` | Settings dialog |
| `config.{h,cpp}` | Load/save `config.json` |
| `app.manifest` | Per-monitor-v2 DPI awareness, common-controls v6 |

**Dependencies**
- `tesseract` (+ `leptonica`) — via `pacman -S mingw-w64-ucrt-x86_64-tesseract-ocr`.
- `tessdata_best` `eng.traineddata`, `spa.traineddata` — downloaded into `tessdata\`.
- Win32 / GDI / Common Controls — from the toolchain.

## 7. Ship target
Portable folder:
```
LeeOCR\
  LeeOCR.exe
  libtesseract-*.dll, libleptonica-*.dll  (+ transitive DLLs)
  libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll  (MinGW runtime)
  tessdata\
    eng.traineddata
    spa.traineddata
```
Runtime DLL set enumerated with `ntldd -R LeeOCR.exe` so the folder is portable
to a machine without MSYS2 installed.

## 8. Build order (incremental, verify each slice)
1. **Slice 0 — end-to-end OCR path:** tray + hotkey → full-virtual-screen grab →
   Tesseract OCR → copy to clipboard + balloon. *Verify it copies real text.*
2. **Slice 1 — region overlay:** replace full-screen grab with dim + drag-to-select.
3. **Slice 2 — preview window:** editable result + Copy/Discard/Recapture.
4. **Slice 3 — preprocessing:** Leptonica upscale/grayscale/threshold.
5. **Slice 4 — config + multi-language + tray menu picker.**
6. **Slice 5 — settings window** (hotkey rebind, autostart toggle).
7. **Slice 6 — package** portable folder, enumerate DLLs, README.

## 9. Risks
- **Overlay + mixed-DPI multi-monitor** is the hardest code — validated early on
  its own slice.
- **Global hotkey conflict** — graceful fallback + Settings rebind.
- **DLL completeness** for portability — resolved via `ntldd -R` enumeration.
- **Model mismatch** — must ship `tessdata_best`, not pacman's standard data.

---

*LeeOCR is a [LeeGStudios.com](https://leegstudios.com) project.*
