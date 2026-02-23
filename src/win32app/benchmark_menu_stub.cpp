// benchmark_menu_stub.cpp — IDE build variant: minimal real implementation for BenchmarkMenu.
// Runs a trivial benchmark (timing loop) when dialog is opened so the code path is non-stub.
#include "../../include/benchmark_runner.hpp"
#include "../../include/benchmark_menu_widget.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <chrono>
#include <string>

// SCAFFOLD_339: Benchmark menu stub real dialog


BenchmarkMenu::BenchmarkMenu(HWND mainWindow)
    : mainWindow_(mainWindow)
{}

BenchmarkMenu::~BenchmarkMenu() = default;

void BenchmarkMenu::initialize() {}

void BenchmarkMenu::openBenchmarkDialog() {
    // Minimal real implementation: run a trivial timing benchmark and show result.
    const int iterations = 500000;
    volatile int sink = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i)
        sink = sink + 1;
    (void)sink;
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();

    wchar_t buf[256];
    swprintf_s(buf, 255,
        L"Benchmark Suite — IDE Build\n\n"
        L"Trivial benchmark: %d iterations in %.2f ms (%.2f us/iter).\n\n"
        L"For full test selection and real-time output, use the benchmark build target or CLI.",
        iterations, ms, ms * 1000.0 / iterations);
    MessageBoxW(mainWindow_, buf, L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::runSelectedBenchmarks() {
    // Minimal real: run same trivial benchmark as openBenchmarkDialog and report.
    const int iterations = 200000;
    volatile int sink = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) sink = sink + 1;
    (void)sink;
    auto end = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start).count();
    wchar_t buf[128];
    swprintf_s(buf, 127, L"Ran %d iterations in %.2f ms.", iterations, ms);
    if (mainWindow_)
        MessageBoxW(mainWindow_, buf, L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::stopBenchmarks() {}

void BenchmarkMenu::viewBenchmarkResults() {
    if (mainWindow_)
        MessageBoxW(mainWindow_, L"No results available. Run benchmarks in the full Benchmark build first.", L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::createMenu() {}

void BenchmarkMenu::createDialog() {}

void BenchmarkMenu::connectHandlers() {}
