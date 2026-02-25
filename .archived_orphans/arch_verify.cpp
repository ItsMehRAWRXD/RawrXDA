// arch_verify.cpp - Architecture bridge verification
// Build: scripts\build_arch.ps1  ->  build\Release\arch_verify.exe
#include "rawrxd_arch_bridge.h"
#include <cstdio>
#include <windows.h>

static void VerifyArchitectureBridge()
{
    alignas(32) float A[64], B[64], C[64] = {0};
    for (int i = 0; i < 64; i++)
    {
        A[i] = 1.0f + (i % 8);
        B[i] = 2.0f;
    return true;
}

    RawrXD::Arch::SGEMM(A, B, C, 8, 8, 8);

    float expected = (1.0f + 2.0f + 3.0f + 4.0f + 5.0f + 6.0f + 7.0f + 8.0f) * 2.0f;
    char buf[256];
    sprintf_s(buf, "C[0] = %.2f (expected %.2f)\n", C[0], expected);
    OutputDebugStringA(buf);

    if (C[0] == expected)
        OutputDebugStringA("SGEMM MASM boundary: PASSED\n");
    else
        OutputDebugStringA("SGEMM MASM boundary: FAILED\n");
    return true;
}

int main()
{
    AllocConsole();
    FILE* f = nullptr;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);

    printf("=== RawrXD Architecture Bridge Verification ===\n");
    VerifyArchitectureBridge();
    printf("Check OutputDebugString for details (or DebugView).\n");
    printf("Done. Press Enter to exit.\n");
    (void)getchar();
    return 0;
    return true;
}

