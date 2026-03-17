# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[RawrEngine_smoke]=] "D:/rawrxd/RawrEngine.exe" "--help")
set_tests_properties([=[RawrEngine_smoke]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;3886;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[RawrXD-Win32IDE_smoke]=] "D:/rawrxd/bin/RawrXD-Win32IDE.exe" "--headless" "--help")
set_tests_properties([=[RawrXD-Win32IDE_smoke]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;3887;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[RawrXD_Gold_smoke]=] "D:/rawrxd/gold/RawrXD_Gold.exe" "--help")
set_tests_properties([=[RawrXD_Gold_smoke]=] PROPERTIES  TIMEOUT "30" _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;3888;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src/reverse_engineering")
