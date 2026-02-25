// benchmark_menu_stub.cpp — IDE build variant: parity stub for BenchmarkMenu (stub holder; see UNFINISHED_FEATURES.md).
// Win32 IDE shows real Benchmark dialog when menu invoked; this satisfies the linker.
// Include full BenchmarkRunner so unique_ptr<BenchmarkRunner> can be destroyed in ~BenchmarkMenu.
#include "../../include/benchmark_runner.hpp"
#include "../../include/benchmark_menu_widget.hpp"

BenchmarkMenu::BenchmarkMenu(HWND mainWindow)
    : mainWindow_(mainWindow)
{}

BenchmarkMenu::~BenchmarkMenu() = default;

void BenchmarkMenu::initialize() {}

void BenchmarkMenu::openBenchmarkDialog() {
    MessageBoxW(mainWindow_,
        L"Benchmark Suite — IDE Build\n\n"
        L"The full benchmark dialog with test selection and real-time output is available "
        L"in the benchmark build target. This IDE build uses a minimal implementation.\n\n"
        L"To run benchmarks: build with the benchmark target or use the CLI benchmark tools.",
        L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::runSelectedBenchmarks() {
    if (mainWindow_)
        MessageBoxW(mainWindow_, L"No benchmarks selected. Use the full Benchmark build for test selection.", L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::stopBenchmarks() {}

void BenchmarkMenu::viewBenchmarkResults() {
    if (mainWindow_)
        MessageBoxW(mainWindow_, L"No results available. Run benchmarks in the full Benchmark build first.", L"Benchmark", MB_OK | MB_ICONINFORMATION);
}

void BenchmarkMenu::createMenu() {}

void BenchmarkMenu::createDialog() {}

void BenchmarkMenu::connectHandlers() {}
