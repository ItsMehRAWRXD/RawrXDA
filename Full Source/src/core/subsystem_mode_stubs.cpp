// subsystem_mode_stubs.cpp — No-op stubs for SubsystemRegistry when RawrXD_IDE_unified.asm is not linked.
// RawrXD-Win32IDE can link this to satisfy extern "C" references from rawrxd_subsystem_api.cpp.

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#endif

extern "C" {

void CompileMode(void) {}
void EncryptMode(void) {}
void InjectMode(void) {}
void UACBypassMode(void) {}
void PersistenceMode(void) {}
void SideloadMode(void) {}
void AVScanMode(void) {}
void EntropyMode(void) {}
void StubGenMode(void) {}
void TraceEngineMode(void) {}
void AgenticMode(void) {}
void BasicBlockCovMode(void) {}
void CovFusionMode(void) {}
void DynTraceMode(void) {}
void AgentTraceMode(void) {}
void GapFuzzMode(void) {}
void IntelPTMode(void) {}
void DiffCovMode(void) {}

}
