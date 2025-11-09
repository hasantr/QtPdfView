cmake_minimum_required(VERSION 3.16)

project(QtPdfView LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Pdf PdfWidgets)

add_executable(QtPdfView
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SelectablePdfView.h
    src/SelectablePdfView.cpp
)

target_link_libraries(QtPdfView PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets)

if(WIN32)
  # Avoid console window on Windows for GUI app
  set_target_properties(QtPdfView PROPERTIES WIN32_EXECUTABLE ON)

  # Locate windeployqt in the same Qt bin dir as Qt6Core.dll
  get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
  if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
  endif()
  if(_qt_core_location)
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    set(WINDEPLOYQT_EXECUTABLE "${_qt_bin_dir}/windeployqt.exe")
  endif()

  # Custom target to deploy all required Qt/Mingw DLLs next to the exe
  add_custom_target(deploy
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations --compiler-runtime "$<TARGET_FILE:QtPdfView>"
    DEPENDS QtPdfView
    COMMENT "Deploying Qt runtime and plugins with windeployqt"
    VERBATIM)

  # Also run deployment after building the app (optional convenience)
  add_custom_command(TARGET QtPdfView POST_BUILD
    COMMAND "${WINDEPLOYQT_EXECUTABLE}" --release --no-translations --compiler-runtime "$<TARGET_FILE:QtPdfView>"
    COMMENT "Post-build: windeployqt"
    VERBATIM)
endif()
