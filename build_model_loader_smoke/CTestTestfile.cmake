# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd/build_model_loader_smoke
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[address_resolver_native_smoke]=] "C:/Program Files/CMake/bin/cmake.exe" "-DRESOLVER_EXE=D:/rawrxd/build_model_loader_smoke/tools/address_resolver_native.exe" "-DMODULE_EXE=D:/rawrxd/build_model_loader_smoke/bin/RawrXD-Win32IDE.exe" "-DMAP_FILE=D:/rawrxd/build_model_loader_smoke/bin/RawrXD-Win32IDE.map" "-DALLOW_MISSING_ARTIFACTS=ON" "-P" "D:/rawrxd/tools/address_resolver_native_smoke.cmake")
set_tests_properties([=[address_resolver_native_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4914;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[address_resolver_native_smoke_minimal]=] "C:/Program Files/CMake/bin/cmake.exe" "-DRESOLVER_EXE=D:/rawrxd/build_model_loader_smoke/tools/address_resolver_native.exe" "-DMODULE_EXE=D:/rawrxd/build_model_loader_smoke/bin/RawrXD-Win32IDE.exe" "-DMAP_FILE=D:/rawrxd/build_model_loader_smoke/bin/RawrXD-Win32IDE.map" "-DFORCE_HELP_PROBE=ON" "-P" "D:/rawrxd/tools/address_resolver_native_smoke.cmake")
set_tests_properties([=[address_resolver_native_smoke_minimal]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4924;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src/reverse_engineering")
subdirs("tests")
subdirs("src/tools")
