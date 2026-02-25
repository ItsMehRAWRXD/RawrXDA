# CMake generated Testfile for 
# Source directory: D:/rawrxd/tests
# Build directory: D:/rawrxd/build_qt_free/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[quant_scalar_smoke]=] "D:/rawrxd/build_qt_free/tests/quant_scalar_smoke.exe")
set_tests_properties([=[quant_scalar_smoke]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/CMakeLists.txt;219;add_test;D:/rawrxd/tests/CMakeLists.txt;0;")
add_test([=[test_kv_cache]=] "D:/rawrxd/build_qt_free/tests/test_kv_cache.exe")
set_tests_properties([=[test_kv_cache]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/CMakeLists.txt;224;add_test;D:/rawrxd/tests/CMakeLists.txt;0;")
add_test([=[test_streaming_gguf_loader]=] "D:/rawrxd/build_qt_free/tests/test_streaming_gguf_loader.exe")
set_tests_properties([=[test_streaming_gguf_loader]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/CMakeLists.txt;248;add_test;D:/rawrxd/tests/CMakeLists.txt;0;")
add_test([=[test_tool_registry]=] "D:/rawrxd/build_qt_free/tests/test_tool_registry.exe")
set_tests_properties([=[test_tool_registry]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/CMakeLists.txt;394;add_test;D:/rawrxd/tests/CMakeLists.txt;0;")
add_test([=[test_gpu_hybrid]=] "D:/rawrxd/build_qt_free/tests/test_gpu_hybrid.exe")
set_tests_properties([=[test_gpu_hybrid]=] PROPERTIES  _BACKTRACE_TRIPLES "D:/rawrxd/tests/CMakeLists.txt;629;add_test;D:/rawrxd/tests/CMakeLists.txt;0;")
subdirs("../_deps/googletest-build")
