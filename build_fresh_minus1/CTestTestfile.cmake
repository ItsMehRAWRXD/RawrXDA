# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd/build_fresh_minus1
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[agentic_orchestrator_smoke]=] "D:/rawrxd/build_fresh_minus1/bin/agentic_orchestrator_smoke_test.exe")
set_tests_properties([=[agentic_orchestrator_smoke]=] PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4910;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[agentic_hotpatch_orchestrator_smoke]=] "D:/rawrxd/build_fresh_minus1/bin/agentic_hotpatch_orchestrator_smoke_test.exe")
set_tests_properties([=[agentic_hotpatch_orchestrator_smoke]=] PROPERTIES  PASS_REGULAR_EXPRESSION "ALL TESTS PASSED" _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;4918;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src/reverse_engineering")
subdirs("tests")
subdirs("src/tools")
