// Build-compat shim for legacy WIN32IDE_SOURCES wiring.
// The production implementation lives in src/multi_file_search.cpp.

namespace {
static unsigned g_multiFileSearchStubHits = 0;
}

extern "C" void RawrXD_MultiFileSearchStubAnchor() {
    g_multiFileSearchStubHits += 1;
}
