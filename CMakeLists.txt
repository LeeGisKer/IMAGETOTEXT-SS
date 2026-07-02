# LeeOCR — a LeeGStudios.com project
# Local screen-region OCR utility for Windows (MSYS2 UCRT64 + GCC).
cmake_minimum_required(VERSION 3.20)
project(LeeOCR VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# --- Dependencies (prebuilt via MSYS2 pacman: mingw-w64-ucrt-x86_64-tesseract-ocr) ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESSERACT REQUIRED IMPORTED_TARGET tesseract)
pkg_check_modules(LEPTONICA REQUIRED IMPORTED_TARGET lept)

add_executable(LeeOCR WIN32
  src/main.cpp
  src/ocr.cpp
  src/capture.cpp
  src/clipboard.cpp
  src/overlay.cpp
  src/preview.cpp
  src/config.cpp
  src/settings.cpp
  src/resource.rc
)

target_compile_definitions(LeeOCR PRIVATE
  UNICODE _UNICODE
  WINVER=0x0A00 _WIN32_WINNT=0x0A00   # Windows 10
  NOMINMAX WIN32_LEAN_AND_MEAN
)

target_link_libraries(LeeOCR PRIVATE
  PkgConfig::TESSERACT
  PkgConfig::LEPTONICA
  gdi32 user32 shell32 comctl32 ole32 advapi32 msimg32
)

# Copy the tessdata_best models next to the exe so dev builds can run in place.
add_custom_command(TARGET LeeOCR POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/tessdata" "$<TARGET_FILE_DIR:LeeOCR>/tessdata"
  COMMENT "Copying tessdata_best models next to LeeOCR.exe")
