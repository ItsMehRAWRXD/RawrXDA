# CMake generated Testfile for 
# Source directory: D:/rawrxd/tests/unit
# Build directory: D:/rawrxd/build_smoke_verify2_ninja/tests/unit
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_xss_sanitizer]=] "D:/rawrxd/build_smoke_verify2_ninja/tests/test_xss_sanitizer.exe")
set_tests_properties([=[test_xss_sanitizer]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/unit/CMakeLists.txt;14;add_test;D:/rawrxd/tests/unit/CMakeLists.txt;19;add_unit_test;D:/rawrxd/tests/unit/CMakeLists.txt;0;")
add_test([=[test_security_manager]=] "D:/rawrxd/build_smoke_verify2_ninja/tests/test_security_manager.exe")
set_tests_properties([=[test_security_manager]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/unit/CMakeLists.txt;14;add_test;D:/rawrxd/tests/unit/CMakeLists.txt;20;add_unit_test;D:/rawrxd/tests/unit/CMakeLists.txt;0;")
add_test([=[test_additional_components]=] "D:/rawrxd/build_smoke_verify2_ninja/tests/test_additional_components.exe")
set_tests_properties([=[test_additional_components]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/unit/CMakeLists.txt;14;add_test;D:/rawrxd/tests/unit/CMakeLists.txt;24;add_unit_test;D:/rawrxd/tests/unit/CMakeLists.txt;0;")
add_test([=[test_distributed_trainer]=] "D:/rawrxd/build_smoke_verify2_ninja/tests/test_distributed_trainer.exe")
set_tests_properties([=[test_distributed_trainer]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/unit/CMakeLists.txt;14;add_test;D:/rawrxd/tests/unit/CMakeLists.txt;28;add_unit_test;D:/rawrxd/tests/unit/CMakeLists.txt;0;")
