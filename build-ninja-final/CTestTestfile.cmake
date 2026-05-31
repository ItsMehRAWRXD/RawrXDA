# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd/build-ninja-final
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[agent_workflow_orchestrator_smoke]=] "D:/rawrxd/build-ninja-final/bin/agent_workflow_orchestrator_smoke.exe")
set_tests_properties([=[agent_workflow_orchestrator_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;5588;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src")
subdirs("src/reverse_engineering")
subdirs("src/tools/tokenizer_roundtrip_test")
subdirs("tests")
subdirs("src/tools")
