RawrXD — legacy Qt quarantine (2026)

PRODUCTION IDE (Qt-free)
  src/win32app/     — Win32 IDE entry (main_win32.cpp, Win32IDE.*, etc.)
  CMake: RawrXD-Win32IDE when RAWRXD_BUILD_WIN32IDE=ON

LEGACY / NOT PRODUCTION
  src/legacy/qtapp/ — former src/qtapp; full Qt widget IDE (archived, not default-built)
  src/legacy/qt_support/*.cpp — Qt-dependent units moved out of src/ root
  include/legacy_qt/*.h       — Qt headers paired with qt_support

NEW WORK
  Add features only under src/win32app/ (or shared non-UI libs), never under legacy/qtapp.

NOTES
  include/legacy_qt/autonomous_resource_manager.h was added to pair with qt_support/autonomous_resource_manager.cpp.
  Legacy Qt sources expect -I <repo>/include and (for MOC) Qt6; they are not built by the default Win32IDE CMake graph.
