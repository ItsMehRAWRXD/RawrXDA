@echo off
set "MSVC_BIN=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
set "WIN_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64"

"%MSVC_BIN%\ml64.exe" /c /Fo entry_min.obj entry_min.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_panic.obj rawr_panic.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_mem.obj rawr_mem.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_str.obj rawr_str.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_log.obj rawr_log.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_time.obj rawr_time.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_ipc.obj rawr_ipc.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_cfg.obj rawr_cfg.asm
"%MSVC_BIN%\ml64.exe" /c /Fo rawr_main.obj rawr_main.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_ABI_Canary.obj RawrXD_ABI_Canary.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_IntegrationSpine.obj RawrXD_IntegrationSpine.asm
"%MSVC_BIN%\ml64.exe" /c /Fo SelfTest_All.obj SelfTest_All.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_Codex_Engine.obj RawrXD_Codex_Engine.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_NativeAgent_Engine.obj RawrXD_NativeAgent_Engine.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_CPUInference_Engine.obj RawrXD_CPUInference_Engine.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_CPUOps_Kernels.obj RawrXD_CPUOps_Kernels.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_PE_Writer.obj RawrXD_PE_Writer.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_TaskDispatcher_Engine.obj RawrXD_TaskDispatcher_Engine.asm
"%MSVC_BIN%\ml64.exe" /c /Fo RawrXD_DumpBin_Engine.obj RawrXD_DumpBin_Engine.asm
  entry_min.obj rawr_panic.obj rawr_mem.obj rawr_str.obj rawr_log.obj rawr_time.obj rawr_ipc.obj rawr_cfg.obj rawr_main.obj RawrXD_ABI_Canary.obj RawrXD_IntegrationSpine.obj SelfTest_All.obj ^
  RawrXD_Codex_Engine.obj RawrXD_NativeAgent_Engine.obj RawrXD_CPUInference_Engine.obj RawrXD_CPUOps_Kernels.obj RawrXD_TaskDispatcher_Engine.obj RawrXD_DumpBin_Engine.obj ^
  /LIBPATH:"%WIN_LIB%" kernel32.lib user32.lib /OUT:RawrXD_Infra.exe
