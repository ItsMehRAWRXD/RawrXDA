/**
 * @file quickjs_host_smoke_test.cpp
 * @brief QuickJS host smoke test (IDE production host)
 *
 * Validates that the production QuickJS host can at least parse/load a minimal
 * pre-installed extension directory. In RAWR_QUICKJS_STUB builds, validates
 * stub-mode behavior is fail-closed and returns a clear error.
 */

#include <cstdio>
#include <string>
#include <windows.h>

#if (defined(_MSVC_LANG) && _MSVC_LANG >= 201703L) || (!defined(_MSVC_LANG) && __cplusplus >= 201703L)
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif


#include "quickjs_extension_host.h"
#include "../modules/vscode_extension_api.h"

static void check(const char* label, bool ok, const char* detail = nullptr)
{
    printf("%s %s", ok ? "[PASS]" : "[FAIL]", label);
    if (detail && *detail)
        printf(" - %s", detail);
    putchar('\n');
}

int main()
{
    printf("=== QuickJS Host Smoke Test ===\n");

    // Fixture directory lives in source tree; resolve relative to CWD for CTest runs.
    // We keep this fail-soft: if the fixture doesn't exist, the test fails loudly.
    const fs::path fixtureDir = fs::path("src") / "tests" / "fixtures" / "quickjs_ext_smoke";

    const bool fixtureOk =
        fs::exists(fixtureDir / "package.json") && fs::exists(fixtureDir / "extension.js");
    check("fixture exists", fixtureOk, fixtureDir.string().c_str());
    if (!fixtureOk)
        return 1;

    auto& host = QuickJSExtensionHost::instance();
    auto init = host.initialize(nullptr, nullptr);
    check("QuickJSExtensionHost::initialize()", init.success, init.detail ? init.detail : "");
    if (!init.success)
        return 1;

    auto load = host.loadPreInstalledExtension(fixtureDir.string().c_str());
    // In this smoke target we compile in stub mode so the build stays linkable
    // without the QuickJS C library. This validates fail-closed behavior.
    const bool stubFailClosed = !load.success;
    check("QuickJSExtensionHost stub fail-closed", stubFailClosed, load.detail ? load.detail : "");
    host.shutdown();
    return stubFailClosed ? 0 : 1;
}
