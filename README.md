# QtPdfView

A minimal, fast PDF viewer built with Qt 6 Widgets.

## Features

- Fast PDF rendering with multi-page view
- In-PDF search with highlighting
- Text selection and copy (Ctrl+C)
- Page thumbnails panel
- Zoom controls (fit to width, fit to page, custom zoom)
- Print support
- Save As functionality
- Drag and drop PDF files to open
- Single instance mode (new files open in existing window)
- Minimap with search result indicators

## Screenshots

*(Add screenshots here)*

## Requirements

- Qt 6.2+ with the following modules:
  - Widgets
  - Pdf
  - PdfWidgets
  - PrintSupport
  - Network
- CMake 3.16+
- C++17 compatible compiler

## Build

### Using CMake with Ninja (MinGW)

```bash
cmake -S . -B build -G "Ninja"
cmake --build build --config Release
```

### Using CMake with MSVC

```bash
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

## Run

```bash
# Windows (Ninja/MinGW)
build/QtPdfView.exe

# Windows (MSVC)
build/Release/QtPdfView.exe

# With a specific PDF file
QtPdfView.exe path/to/file.pdf
```

## Usage

- **Open PDF**: Drag and drop a PDF file onto the window, or pass it as command line argument
- **Search**: Type in the search box (minimum 2 characters)
- **Navigate results**: F3 (next) / Shift+F3 (previous)
- **Copy text**: Select with mouse, then Ctrl+C
- **Zoom**: Use toolbar buttons or Ctrl+/Ctrl-
- **Page navigation**: Page Up/Down keys or toolbar buttons

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+F | Focus search box |
| F3 | Next search result |
| Shift+F3 | Previous search result |
| Ctrl+C | Copy selected text |
| Ctrl++ | Zoom in |
| Ctrl+- | Zoom out |
| Page Up | Previous page |
| Page Down | Next page |
| Escape | Clear search |

## License

This project is licensed under the Apache License 2.0 - see the [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
