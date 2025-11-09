cmake_minimum_required(VERSION 3.16)

project(QtPdfView LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets Pdf PdfWidgets PrintSupport)

add_executable(QtPdfView
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SelectablePdfView.h
    src/SelectablePdfView.cpp
)

target_link_libraries(QtPdfView PRIVATE Qt6::Widgets Qt6::Pdf Qt6::PdfWidgets Qt6::PrintSupport)

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

# Enable LTO and strip in Release when supported
include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output)
if(ipo_supported)
  set_property(TARGET QtPdfView PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

if(MINGW)
  string(APPEND CMAKE_CXX_FLAGS_RELEASE " -O3 -ffunction-sections -fdata-sections -flto")
  string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " -s -Wl,--gc-sections")
endif()
