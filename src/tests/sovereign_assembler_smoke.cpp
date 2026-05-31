// Smoke checks: instruction encoding + PE import dir / FF15→IAT + run ExitProcess(0) stub.
// Build: cmake --build . --target sovereign_assembler_smoke
#include <windows.h>

#include "agentic/SovereignAssembler.h"

#include <cstring>
#include <cwchar>
#include <fstream>
#include <string>
#include <vector>

namespace
{

bool expectBytes(const std::vector<uint8_t>& code, const uint8_t* expected, size_t n)
{
    return code.size() >= n && std::memcmp(code.data(), expected, n) == 0;
}

bool rvaToFileOffset(const std::vector<uint8_t>& pe, const IMAGE_NT_HEADERS64* nt, uint32_t rva, DWORD& outOff)
{
    const WORD nsec = nt->FileHeader.NumberOfSections;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nsec; ++i)
    {
        const IMAGE_SECTION_HEADER& s = sec[i];
        const uint32_t va = s.VirtualAddress;
        const uint32_t span = s.Misc.VirtualSize != 0 ? s.Misc.VirtualSize : s.SizeOfRawData;
        if (rva >= va && rva < va + span && s.PointerToRawData != 0)
        {
            outOff = s.PointerToRawData + (rva - va);
            return true;
        }
    }
    return false;
}

/// Returns 0 on success; non-zero codes for each failure branch (see main return 100 + code).
int verifyExitProcessPeImport(const std::vector<uint8_t>& pe)
{
    if (pe.size() < sizeof(IMAGE_DOS_HEADER))
    {
        return 1;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(pe.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return 2;
    }
    const size_t peOff = static_cast<size_t>(dos->e_lfanew);
    if (peOff + sizeof(IMAGE_NT_HEADERS64) > pe.size())
    {
        return 3;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(pe.data() + peOff);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != 0x20B)  // PE32+ (x64)
    {
        return 4;
    }

    const IMAGE_DATA_DIRECTORY& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress == 0 || impDir.Size == 0)
    {
        return 5;
    }

    DWORD importDescOff = 0;
    if (!rvaToFileOffset(pe, nt, impDir.VirtualAddress, importDescOff))
    {
        return 6;
    }

    uint32_t exitProcessIatSlotRva = 0;
    bool foundKernel = false;

    for (size_t off = importDescOff;; off += sizeof(IMAGE_IMPORT_DESCRIPTOR))
    {
        if (off + sizeof(IMAGE_IMPORT_DESCRIPTOR) > pe.size())
        {
            return 7;
        }
        const auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(pe.data() + off);
        if (d->OriginalFirstThunk == 0 && d->Name == 0 && d->FirstThunk == 0)
        {
            break;
        }

        DWORD dllNameOff = 0;
        if (!rvaToFileOffset(pe, nt, d->Name, dllNameOff))
        {
            return 8;
        }
        const char* dll = reinterpret_cast<const char*>(pe.data() + dllNameOff);
        if (_stricmp(dll, "kernel32.dll") != 0)
        {
            continue;
        }
        foundKernel = true;

        DWORD iatOff = 0;
        if (!rvaToFileOffset(pe, nt, d->FirstThunk, iatOff))
        {
            return 9;
        }
        if (iatOff + 8 > pe.size())
        {
            return 10;
        }
        uint64_t iatEnt = 0;
        std::memcpy(&iatEnt, pe.data() + iatOff, sizeof(iatEnt));

        const uint32_t hintNameRva = static_cast<uint32_t>(iatEnt & 0xFFFFFFFFu);
        DWORD hintOff = 0;
        if (!rvaToFileOffset(pe, nt, hintNameRva, hintOff))
        {
            return 11;
        }
        if (hintOff + 3 > pe.size())
        {
            return 12;
        }
        const char* sym = reinterpret_cast<const char*>(pe.data() + hintOff + 2);
        if (std::strcmp(sym, "ExitProcess") != 0)
        {
            return 13;
        }
        exitProcessIatSlotRva = d->FirstThunk;
        break;
    }
    if (!foundKernel || exitProcessIatSlotRva == 0)
    {
        return 14;
    }

    const WORD nsec = nt->FileHeader.NumberOfSections;
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    DWORD textRaw = 0;
    uint32_t textVa = 0;
    DWORD textRawSize = 0;
    for (WORD i = 0; i < nsec; ++i)
    {
        const IMAGE_SECTION_HEADER& s = sec[i];
        char name[IMAGE_SIZEOF_SHORT_NAME + 1] = {};
        std::memcpy(name, s.Name, IMAGE_SIZEOF_SHORT_NAME);
        if (std::strncmp(name, ".text", 5) == 0)
        {
            textRaw = s.PointerToRawData;
            textVa = s.VirtualAddress;
            textRawSize = s.SizeOfRawData;
            break;
        }
    }
    if (textRaw == 0 || textRaw + textRawSize > pe.size())
    {
        return 15;
    }

    bool foundFf15 = false;
    int32_t rel = 0;
    DWORD dispStartInText = 0;
    for (DWORD i = 0; i + 6 <= textRawSize; ++i)
    {
        if (pe[textRaw + i] == 0xFF && pe[textRaw + i + 1] == 0x15)
        {
            std::memcpy(&rel, pe.data() + textRaw + i + 2, sizeof(rel));
            dispStartInText = i + 2;
            foundFf15 = true;
            break;
        }
    }
    if (!foundFf15)
    {
        return 16;
    }

    const uint32_t insnEndRva = textVa + dispStartInText + 4u;
    const int64_t target = static_cast<int64_t>(insnEndRva) + static_cast<int64_t>(rel);
    if (target < 0 || target != static_cast<int64_t>(exitProcessIatSlotRva))
    {
        return 17;
    }

    return 0;
}

}  // namespace

int main()
{
    using SovereignAssembler::AssembleAndLink;
    using SovereignAssembler::AssembleToBuffer;
    using SovereignAssembler::AssemblyResult;
    using SovereignAssembler::VerifyPEChecksum;

    std::string err;
    AssemblyResult r;

    // add rcx, rdx  -> 48 01 D1 (ADD r/m64, r64)
    if (!AssembleToBuffer("add rcx, rdx", r, err) ||
        !expectBytes(r.code, reinterpret_cast<const uint8_t*>("\x48\x01\xD1"), 3))
    {
        return 1;
    }

    // add rax, 0x11223344 -> REX.W + 81 /0 id (does not fit imm8); 7 bytes total
    if (!AssembleToBuffer("add rax, 0x11223344", r, err) || r.code.size() < 7)
    {
        return 2;
    }
    const uint8_t addImm[] = {0x48, 0x81, 0xC0, 0x44, 0x33, 0x22, 0x11};
    if (!expectBytes(r.code, addImm, sizeof(addImm)))
    {
        return 3;
    }

    // add rax, 10 -> REX.W + 83 /0 (dense imm8)
    if (!AssembleToBuffer("add rax, 10", r, err) ||
        !expectBytes(r.code, reinterpret_cast<const uint8_t*>("\x48\x83\xC0\x0A"), 4))
    {
        return 4;
    }

    // movzx rax, al -> 48 0F B6 C0
    if (!AssembleToBuffer("movzx rax, al", r, err) ||
        !expectBytes(r.code, reinterpret_cast<const uint8_t*>("\x48\x0F\xB6\xC0"), 4))
    {
        return 5;
    }

    // lea rax, [rcx+rdx*4+8] — REX.W + 8D + ModRM/SIB (+ optional disp)
    if (!AssembleToBuffer("lea rax, [rcx+rdx*4+8]", r, err) || r.code.size() < 4 || r.code[0] != 0x48 ||
        r.code[1] != 0x8D)
    {
        return 6;
    }

    // test rbx, rbx -> 48 85 DB
    if (!AssembleToBuffer("test rbx, rbx", r, err) ||
        !expectBytes(r.code, reinterpret_cast<const uint8_t*>("\x48\x85\xDB"), 3))
    {
        return 7;
    }

    // test rax, 0x11223344 -> REX.W + F7 /0 imm32
    if (!AssembleToBuffer("test rax, 0x11223344", r, err) || r.code.size() < 7)
    {
        return 24;
    }
    const uint8_t testImm[] = {0x48, 0xF7, 0xC0, 0x44, 0x33, 0x22, 0x11};
    if (!expectBytes(r.code, testImm, sizeof(testImm)))
    {
        return 25;
    }

    // movsxd rax, edx -> 48 63 C2
    if (!AssembleToBuffer("movsxd rax, edx", r, err) ||
        !expectBytes(r.code, reinterpret_cast<const uint8_t*>("\x48\x63\xC2"), 3))
    {
        return 8;
    }

    // jmp label: fixup rel32 = target - (fixupOffset + 4); label at 0, fixup at byte 1 -> -5
    if (!AssembleToBuffer("start: jmp start", r, err) || !r.success)
    {
        return 9;
    }
    if (r.code.size() < 5 || r.code[0] != 0xE9)
    {
        return 10;
    }
    int32_t jmpRel = 0;
    std::memcpy(&jmpRel, r.code.data() + 1, sizeof(jmpRel));
    if (jmpRel != -5)
    {
        return 11;
    }

    // je start — 0F 84 rel32; disp at offset 2; rel = 0 - (2+4) = -6
    if (!AssembleToBuffer("start: je start", r, err) || !r.success || r.code.size() < 6)
    {
        return 12;
    }
    if (r.code[0] != 0x0F || r.code[1] != 0x84)
    {
        return 13;
    }
    int32_t jeRel = 0;
    std::memcpy(&jeRel, r.code.data() + 2, sizeof(jeRel));
    if (jeRel != -6)
    {
        return 14;
    }

    // cmp rax, rcx; jne start — near jcc rel32; next insn after jne at 9 → rel = -9
    if (!AssembleToBuffer("start:\ncmp rax, rcx\njne start", r, err) || !r.success || r.code.size() < 9)
    {
        return 26;
    }
    if (r.code[0] != 0x48 || r.code[1] != 0x39 || r.code[2] != 0xC8 || r.code[3] != 0x0F || r.code[4] != 0x85)
    {
        return 27;
    }
    int32_t jneRel = 0;
    std::memcpy(&jneRel, r.code.data() + 5, sizeof(jneRel));
    if (jneRel != -9)
    {
        return 28;
    }

    // lea rax, [rip+0] — RIP-relative disp32 only
    if (!AssembleToBuffer("lea rax, [rip+0]", r, err) || r.code.size() < 7)
    {
        return 15;
    }
    const uint8_t leaRip[] = {0x48, 0x8D, 0x05, 0x00, 0x00, 0x00, 0x00};
    if (!expectBytes(r.code, leaRip, sizeof(leaRip)))
    {
        return 16;
    }

    // --- PE + import table + ExitProcess(0): end-to-end (idata, FF15 fixup, optional header import dir) ---
    wchar_t tmpDir[MAX_PATH];
    wchar_t tmpExe[MAX_PATH];
    if (GetTempPathW(MAX_PATH, tmpDir) == 0)
    {
        return 17;
    }
    if (GetTempFileNameW(tmpDir, L"sve", 0, tmpExe) == 0)
    {
        return 18;
    }
    DeleteFileW(tmpExe);
    // GetTempFileNameW returns a ".tmp" name; use a normal .exe suffix so the loader and AV behave consistently.
    {
        wchar_t* dot = wcsrchr(tmpExe, L'.');
        if (dot != nullptr)
        {
            *dot = L'\0';
        }
        wcscat_s(tmpExe, MAX_PATH, L".exe");
    }

    // x64 Microsoft ABI: 32-byte shadow + keep stack 16-byte aligned before indirect calls (see MSDN x64 calling).
    constexpr const char* kExitProcAsm = "import kernel32.dll, ExitProcess\n"
                                         "start:\n"
                                         "sub rsp, 0x28\n"
                                         "xor ecx, ecx\n"
                                         "call ExitProcess\n";

    if (!AssembleAndLink(kExitProcAsm, tmpExe, err))
    {
        return 19;
    }
    std::vector<uint8_t> peBytes;
    {
        std::ifstream in(tmpExe, std::ios::binary | std::ios::ate);
        if (!in)
        {
            DeleteFileW(tmpExe);
            return 20;
        }
        const auto sz = static_cast<size_t>(in.tellg());
        peBytes.resize(sz);
        in.seekg(0, std::ios::beg);
        in.read(reinterpret_cast<char*>(peBytes.data()), static_cast<std::streamsize>(sz));
    }
    if (!VerifyPEChecksum(peBytes))
    {
        DeleteFileW(tmpExe);
        return 21;
    }
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(tmpExe, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        DeleteFileW(tmpExe);
        return 22;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0xFFFFFFFF;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteFileW(tmpExe);

    if (exitCode != 0)
    {
        return 23;
    }

    return 0;
}
