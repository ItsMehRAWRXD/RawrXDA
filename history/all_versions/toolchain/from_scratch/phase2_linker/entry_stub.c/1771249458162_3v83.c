/*==========================================================================
 * Phase 5a: Entry Point Stub - Fully Reverse Engineered
 *
 * Provides the default CRT entry point machine code that the linker
 * injects when no explicit entry point is found in the object files.
 *
 * The stub follows the Microsoft x64 CRT initialization pattern:
 *   1. Align stack to 16-byte boundary (sub rsp, 0x28)
 *   2. Call __security_init_cookie (if available)
 *   3. Call mainCRTStartup -> main()
 *   4. Call ExitProcess with return value
 *
 * For a minimal executable (no CRT), the stub simplifies to:
 *   sub rsp, 0x28          ; shadow space + alignment
 *   call <entry>           ; call user's main/WinMain
 *   mov ecx, eax           ; exit code = return value
 *   call ExitProcess       ; terminate
 *   int 3                  ; safety breakpoint
 *
 * Machine code for x64:
 *   48 83 EC 28             sub rsp, 0x28
 *   E8 xx xx xx xx          call <main>           (REL32 placeholder)
 *   89 C1                   mov ecx, eax
 *   48 83 C4 28             add rsp, 0x28
 *   E8 xx xx xx xx          call <ExitProcess>    (REL32 placeholder)
 *   CC                      int 3
 *
 * Total: 20 bytes
 *
 * The linker patches the two CALL instructions with proper REL32
 * displacements to main() and ExitProcess() during relocation.
 *=========================================================================*/
#include <stdint.h>
#include <stddef.h>

/*---- Primary entry stub: calls main then ExitProcess ----*/
uint8_t entry_stub_code[] = {
  /* 0x00 */ 0x48, 0x83, 0xEC, 0x28,             /* sub rsp, 0x28 (shadow+align) */
  /* 0x04 */ 0xE8, 0x00, 0x00, 0x00, 0x00,       /* call main (rel32 at offset 5) */
  /* 0x09 */ 0x89, 0xC1,                          /* mov ecx, eax (exit code) */
  /* 0x0B */ 0x48, 0x83, 0xC4, 0x28,             /* add rsp, 0x28 */
  /* 0x0F */ 0xE8, 0x00, 0x00, 0x00, 0x00,       /* call ExitProcess (rel32 at offset 0x10) */
  /* 0x14 */ 0xCC,                                /* int 3 (safety trap) */
  /* 0x15 */ 0xCC,                                /* int 3 (padding) */
  /* 0x16 */ 0x90, 0x90,                          /* nop nop (align to 0x18) */
};

const size_t entry_stub_size = sizeof(entry_stub_code);

/* Offsets within the stub where REL32 relocations need to be patched */
const uint32_t entry_stub_main_reloc_offset = 0x05;      /* call main displacement */
const uint32_t entry_stub_exitproc_reloc_offset = 0x10;   /* call ExitProcess displacement */

/*---- Alternative stub: WinMain entry for GUI applications ----
 * Calls GetModuleHandleA(NULL) for hInstance, then WinMain(hInst, 0, "", SW_SHOW)
 */
uint8_t entry_stub_winmain[] = {
  /* Sub rsp for shadow space + alignment */
  0x48, 0x83, 0xEC, 0x48,                       /* sub rsp, 0x48 */
  /* GetModuleHandleA(NULL) */
  0x48, 0x31, 0xC9,                             /* xor rcx, rcx */
  0xE8, 0x00, 0x00, 0x00, 0x00,                 /* call GetModuleHandleA (patch) */
  /* WinMain(hInst=rax, hPrevInst=0, lpCmdLine="", nShowCmd=SW_SHOWDEFAULT) */
  0x48, 0x89, 0xC1,                             /* mov rcx, rax (hInstance) */
  0x48, 0x31, 0xD2,                             /* xor rdx, rdx (hPrevInstance) */
  0x4D, 0x31, 0xC0,                             /* xor r8, r8  (lpCmdLine) */
  0x41, 0xB9, 0x0A, 0x00, 0x00, 0x00,           /* mov r9d, 10 (SW_SHOWDEFAULT) */
  0xE8, 0x00, 0x00, 0x00, 0x00,                 /* call WinMain (patch) */
  /* ExitProcess(eax) */
  0x89, 0xC1,                                   /* mov ecx, eax */
  0x48, 0x83, 0xC4, 0x48,                       /* add rsp, 0x48 */
  0xE8, 0x00, 0x00, 0x00, 0x00,                 /* call ExitProcess (patch) */
  0xCC,                                          /* int 3 */
};

const size_t entry_stub_winmain_size = sizeof(entry_stub_winmain);

/*---- Minimal stub: just ret (for DLLs or testing) ----*/
uint8_t entry_stub_minimal[] = {
  0x48, 0x83, 0xEC, 0x28,  /* sub rsp, 0x28 */
  0xB8, 0x01, 0x00, 0x00, 0x00,  /* mov eax, 1 (TRUE for DllMain) */
  0x48, 0x83, 0xC4, 0x28,  /* add rsp, 0x28 */
  0xC3,                     /* ret */
};

const size_t entry_stub_minimal_size = sizeof(entry_stub_minimal);
