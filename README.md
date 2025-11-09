QtPdfView (Qt 6 Widgets)

Minimal, fast PDF viewer using Qt6 QPdfView with:
- In-PDF search highlighting (type in the top toolbar)
- Text selection and copy (Ctrl+C)
- Opens `license.pdf` from the working directory on startup

Build
- Ensure Qt 6 with `Widgets` and `Pdf` modules is installed and in PATH (or use your Qt Developer Command Prompt / env.bat).
- CMake (Ninja): `cmake -S . -B build -G "Ninja" && cmake --build build --config Release`
- CMake (MSVC): `cmake -S . -B build -G "Visual Studio 17 2022" && cmake --build build --config Release`

Run
- Windows: `build/Release/QtPdfView.exe` (MSVC) or `build/QtPdfView.exe` (Ninja/MinGW)
- It automatically opens `license.pdf` from the current directory.

Notes
- Search: type into the toolbar; matches are highlighted.
- Copy: select text with mouse, press Ctrl+C to copy.
- Defaults to single-page, fit-to-width for fast rendering.
