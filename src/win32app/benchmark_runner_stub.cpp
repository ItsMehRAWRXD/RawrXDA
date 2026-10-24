// Build-compat shim for legacy benchmark runner stub references.
// The production implementation lives in src/benchmark_runner.cpp.

namespace {
static unsigned g_benchmarkRunnerStubHits = 0;
}

extern "C" void RawrXD_BenchmarkRunnerStubAnchor() {
    g_benchmarkRunnerStubHits += 1;
}
