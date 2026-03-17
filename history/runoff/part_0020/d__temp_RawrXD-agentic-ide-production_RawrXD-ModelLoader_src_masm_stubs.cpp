// Minimal stubs to allow linking when MASM DLLs are not present in this target
extern "C" {
int InitializeAgenticLoop() { return 0; }
int StartAgenticLoop(const char*) { return 0; }
int StopAgenticLoop() { return 0; }
int CleanupAgenticLoop() { return 0; }
int GGUF_IDE_RegisterLoader(const char*, int) { return 0; }
int GGUF_IDE_NotifyProgress(int, const char*) { return 0; }
}
