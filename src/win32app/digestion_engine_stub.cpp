// Build-compat shim for legacy digestion stub references.
// Production digestion engines live under src/digestion/.

namespace {
static unsigned g_digestionEngineStubHits = 0;
}

extern "C" void RawrXD_DigestionEngineStubAnchor() {
    g_digestionEngineStubHits += 1;
}
