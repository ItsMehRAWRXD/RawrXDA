# C/C++ for Cursor

## Version 2.0.2
* Add back the setting `C_Cpp.intelliSenseEngine` for compatibility with ClangD.

## Version 2.0.1
* Fixed compatibility with extensions that depend on `ms-vscode.cpptools` (requires Cursor v0.52 or greater)

## Version 2.0.0
* Added support for Alpine remote hosts
* Bundling [CodeLLDB](https://github.com/vadimcn/codelldb) for debugger support on Windows.
* Bundling [CMakeTools](https://github.com/microsoft/vscode-cmake-tools)
* Removed support for `cppvsdbg` debugging configurations. This debugger is not supported within Cursor.

## Version 1.25.0
* Initial Release. Forked from https://github.com/microsoft/vscode-cpptools, version 1.25.0
