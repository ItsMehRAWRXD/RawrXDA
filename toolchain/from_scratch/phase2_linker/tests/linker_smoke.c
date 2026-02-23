/* Minimal C source to produce a COFF .obj for Phase 2 linker smoke test.
 * Compile with MSVC: cl /c linker_smoke.c
 * Then: rawrxd_link linker_smoke.obj -o linker_smoke.exe
 * Expected: linker_smoke.exe exits with code 42. */
int main(void) { return 42; }
