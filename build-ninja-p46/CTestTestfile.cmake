# CMake generated Testfile for 
# Source directory: D:/rawrxd
# Build directory: D:/rawrxd/build-ninja-p46
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[smoke_test_measurement_integration]=] "D:/rawrxd/build-ninja-p46/bin/smoke_test_measurement_integration.exe")
set_tests_properties([=[smoke_test_measurement_integration]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;5721;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[e2e_integration_test]=] "D:/rawrxd/build-ninja-p46/bin/e2e_integration_test.exe")
set_tests_properties([=[e2e_integration_test]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;5737;add_test;D:/rawrxd/CMakeLists.txt;0;")
add_test([=[agent_workflow_orchestrator_smoke]=] "D:/rawrxd/build-ninja-p46/bin/agent_workflow_orchestrator_smoke.exe")
set_tests_properties([=[agent_workflow_orchestrator_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/CMakeLists.txt;5765;add_test;D:/rawrxd/CMakeLists.txt;0;")
subdirs("src")
subdirs("src/reverse_engineering")
subdirs("src/tools/tokenizer_roundtrip_test")
subdirs("tests")
subdirs("src/tools")
