# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd/build_smoke_auto
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[monaco_ghost_bridge_smoke]=] "D:/rawrxd/build_smoke_auto/tests/test_monaco_bridge.exe")
set_tests_properties([=[monaco_ghost_bridge_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4086;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[sovereign_compose_lab]=] "D:/rawrxd/build_smoke_auto/tests/test_sovereign_compose_lab.exe")
set_tests_properties([=[sovereign_compose_lab]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4107;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[sovereign_global_dce]=] "D:/rawrxd/build_smoke_auto/tests/test_sovereign_global_dce.exe")
set_tests_properties([=[sovereign_global_dce]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4126;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src/reverse_engineering")
