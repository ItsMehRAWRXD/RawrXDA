// Build-compat shim for legacy benchmark menu stub references.
// The production implementation lives in src/benchmark_menu_widget.cpp.

namespace {
static unsigned g_benchmarkMenuStubHits = 0;
}

extern "C" void RawrXD_BenchmarkMenuStubAnchor() {
    g_benchmarkMenuStubHits += 1;
}
