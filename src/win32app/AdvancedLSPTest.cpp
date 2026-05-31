#include "AdvancedLSPClient.h"
#include <iostream>
#include <chrono>

using namespace RawrXD::LSP;

int main() {
    std::cout << "Starting Advanced LSP Integration Test...
";
    auto& lsp = AdvancedLSPClient::GetInstance();

    // 1. Test Workspace Symbol Search
    std::cout << "Testing Workspace Symbol Search...
";
    auto symbolFuture = lsp.QueryWorkspaceSymbols("Memory");
    auto symbols = symbolFuture.get();

    for (const auto& sym : symbols) {
        std::cout << "Found Symbol: " << sym.name << " in " << sym.location_uri << " at L" << sym.line << std::endl;
    }

    if (symbols.empty()) {
        std::cerr << "LSP Workspace Search FAILED.
";
        return 1;
    }

    // 2. Test Global Rename Preparation
    std::cout << "Testing Global Rename Preparation...
";
    auto renameFuture = lsp.PrepareGlobalRename("file:///D:/test.cpp", 10, 5, "NewSymbolName1);
    auto edit = renameFuture.get();

    if (edit.changes.empty()) {
        std::cerr << "LSP Global Rename FAILED.
";
        return 1;
    }

    std::cout << "Advanced LSP Features Verified: PASSED
";
    return 0;
}
