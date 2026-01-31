# CMake generated Testfile for 
# Source directory: D:/rawrxd/Ship
# Build directory: D:/rawrxd/build_clean/Release
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[All_Tests]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe")
set_tests_properties([=[All_Tests]=] PROPERTIES  LABELS "integration" TIMEOUT "60" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;194;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[Core_Infrastructure]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "CoreInfra")
set_tests_properties([=[Core_Infrastructure]=] PROPERTIES  LABELS "core" TIMEOUT "30" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;200;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[Hidden_Logic]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "HiddenLogic")
set_tests_properties([=[Hidden_Logic]=] PROPERTIES  LABELS "logic" TIMEOUT "30" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;205;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[AI_Systems]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "AIEngine")
set_tests_properties([=[AI_Systems]=] PROPERTIES  LABELS "ai" TIMEOUT "45" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;210;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[Tools_Integration]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "Tools")
set_tests_properties([=[Tools_Integration]=] PROPERTIES  LABELS "tools" TIMEOUT "30" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;215;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[Performance]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "Performance")
set_tests_properties([=[Performance]=] PROPERTIES  LABELS "perf" TIMEOUT "45" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;220;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
add_test([=[Stress_Tests]=] "D:/rawrxd/build_clean/Release/RawrXD_TestRunner.exe" "--category" "Stress")
set_tests_properties([=[Stress_Tests]=] PROPERTIES  LABELS "stress" TIMEOUT "120" WORKING_DIRECTORY "D:/rawrxd/build_clean/Release" _BACKTRACE_TRIPLES "D:/rawrxd/Ship/CMakeLists.txt;225;add_test;D:/rawrxd/Ship/CMakeLists.txt;0;")
