@echo off
cd /d d:\rawrxd\src
if exist agent\quantum_agent_orchestrator_stubs.cpp del /f agent\quantum_agent_orchestrator_stubs.cpp
echo DELETED: agent\quantum_agent_orchestrator_stubs.cpp
if exist core\analyzer_distiller_stubs.cpp del /f core\analyzer_distiller_stubs.cpp
echo DELETED: core\analyzer_distiller_stubs.cpp
if exist core\enterprise_license_stubs.cpp del /f core\enterprise_license_stubs.cpp
echo DELETED: core\enterprise_license_stubs.cpp
if exist core\licensing_stubs.cpp del /f core\licensing_stubs.cpp
echo DELETED: core\licensing_stubs.cpp
if exist core\memory_patch_byte_search_stubs.cpp del /f core\memory_patch_byte_search_stubs.cpp
echo DELETED: core\memory_patch_byte_search_stubs.cpp
if exist core\mesh_brain_asm_stubs.cpp del /f core\mesh_brain_asm_stubs.cpp
echo DELETED: core\mesh_brain_asm_stubs.cpp
if exist core\missing_handler_stubs.cpp del /f core\missing_handler_stubs.cpp
echo DELETED: core\missing_handler_stubs.cpp
if exist core\monaco_core_stubs.cpp del /f core\monaco_core_stubs.cpp
echo DELETED: core\monaco_core_stubs.cpp
if exist core\sqlite3_stubs.cpp del /f core\sqlite3_stubs.cpp
echo DELETED: core\sqlite3_stubs.cpp
if exist core\streaming_orchestrator_stubs.cpp del /f core\streaming_orchestrator_stubs.cpp
echo DELETED: core\streaming_orchestrator_stubs.cpp
if exist core\stubs.cpp del /f core\stubs.cpp
echo DELETED: core\stubs.cpp
if exist core\subsystem_mode_stubs.cpp del /f core\subsystem_mode_stubs.cpp
echo DELETED: core\subsystem_mode_stubs.cpp
if exist core\swarm_network_stubs.cpp del /f core\swarm_network_stubs.cpp
echo DELETED: core\swarm_network_stubs.cpp
if exist core\test_masm_stubs.cpp del /f core\test_masm_stubs.cpp
echo DELETED: core\test_masm_stubs.cpp
if exist core\universal_stub.cpp del /f core\universal_stub.cpp
echo DELETED: core\universal_stub.cpp
if exist core\WebView2Container_stubs.cpp del /f core\WebView2Container_stubs.cpp
echo DELETED: core\WebView2Container_stubs.cpp
if exist core\win32_ide_link_stubs.cpp del /f core\win32_ide_link_stubs.cpp
echo DELETED: core\win32_ide_link_stubs.cpp
if exist engine\core_generator_stubs.cpp del /f engine\core_generator_stubs.cpp
echo DELETED: engine\core_generator_stubs.cpp
if exist engine\inference_kernels_stubs.cpp del /f engine\inference_kernels_stubs.cpp
echo DELETED: engine\inference_kernels_stubs.cpp
if exist inference\vulkan_compute_min_stub.cpp del /f inference\vulkan_compute_min_stub.cpp
echo DELETED: inference\vulkan_compute_min_stub.cpp
if exist qtapp\compliance_logger_stub.hpp del /f qtapp\compliance_logger_stub.hpp
echo DELETED: qtapp\compliance_logger_stub.hpp
if exist qtapp\gguf_loader_stub.hpp del /f qtapp\gguf_loader_stub.hpp
echo DELETED: qtapp\gguf_loader_stub.hpp
if exist stubs\production_link_stubs.cpp del /f stubs\production_link_stubs.cpp
echo DELETED: stubs\production_link_stubs.cpp
if exist telemetry\ai_metrics_stub.cpp del /f telemetry\ai_metrics_stub.cpp
echo DELETED: telemetry\ai_metrics_stub.cpp
if exist win32app\digestion_engine_stub.cpp del /f win32app\digestion_engine_stub.cpp
echo DELETED: win32app\digestion_engine_stub.cpp
if exist win32app\link_stubs_ssot_handlers.cpp del /f win32app\link_stubs_ssot_handlers.cpp
echo DELETED: win32app\link_stubs_ssot_handlers.cpp
if exist win32app\link_stubs_win32ide_methods.cpp del /f win32app\link_stubs_win32ide_methods.cpp
echo DELETED: win32app\link_stubs_win32ide_methods.cpp
if exist win32app\multi_file_search_stub.cpp del /f win32app\multi_file_search_stub.cpp
echo DELETED: win32app\multi_file_search_stub.cpp
if exist win32app\reverse_engineered_stubs.cpp del /f win32app\reverse_engineered_stubs.cpp
echo DELETED: win32app\reverse_engineered_stubs.cpp
if exist win32app\Win32IDE_CircularArchStub.cpp del /f win32app\Win32IDE_CircularArchStub.cpp
echo DELETED: win32app\Win32IDE_CircularArchStub.cpp
if exist minimal_hub_stub.hpp del /f minimal_hub_stub.hpp
echo DELETED: minimal_hub_stub.hpp
if exist rawrxd_cli_stub.cpp del /f rawrxd_cli_stub.cpp
echo DELETED: rawrxd_cli_stub.cpp
if exist vulkan_compute_stub.cpp del /f vulkan_compute_stub.cpp
echo DELETED: vulkan_compute_stub.cpp
echo === DONE ===
