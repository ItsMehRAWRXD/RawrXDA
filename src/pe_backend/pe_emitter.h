/*=============================================================================
 * RawrXD PE32+ Backend — pe_emitter.h
 * Monolithic PE32+ writer + x64 machine-code emitter
 * Pure backend library — no demos, no stubs
 * Zero runtime deps beyond kernel32.dll in generated output
 *
 * Build the companion .c with:
 *   C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe
 *     /O2 /W4 /c pe_emitter.c
 *===========================================================================*/
#ifndef RAWRXD_PE_EMITTER_H
#define RAWRXD_PE_EMITTER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*═══════════════════════════════════════════════════════════════════════════
 * PE32+ Structure Templates — exact on-disk layout
 *═══════════════════════════════════════════════════════════════════════════*/

#pragma pack(push, 1)

/* ── IMAGE_DOS_HEADER (64 bytes) ─────────────────────────────────────────── */
typedef struct {
    uint16_t e_magic;           /* 0x5A4D "MZ"                              */
    uint16_t e_cblp;            /* Bytes on last page of file               */
    uint16_t e_cp;              /* Pages in file                            */
    uint16_t e_crlc;            /* Relocations                              */
    uint16_t e_cparhdr;         /* Size of header in paragraphs             */
    uint16_t e_minalloc;        /* Minimum extra paragraphs needed          */
    uint16_t e_maxalloc;        /* Maximum extra paragraphs needed          */
    uint16_t e_ss;              /* Initial (relative) SS value              */
    uint16_t e_sp;              /* Initial SP value                         */
    uint16_t e_csum;            /* Checksum                                 */
    uint16_t e_ip;              /* Initial IP value                         */
    uint16_t e_cs;              /* Initial (relative) CS value              */
    uint16_t e_lfarlc;          /* File address of relocation table         */
    uint16_t e_ovno;            /* Overlay number                           */
    uint16_t e_res[4];          /* Reserved                                 */
    uint16_t e_oemid;           /* OEM identifier                           */
    uint16_t e_oeminfo;         /* OEM information                          */
    uint16_t e_res2[10];        /* Reserved                                 */
    uint32_t e_lfanew;          /* File offset to PE signature              */
} PE_DOS_HEADER;

/* ── COFF File Header (20 bytes) ─────────────────────────────────────────── */
typedef struct {
    uint16_t Machine;                   /* 0x8664 = IMAGE_FILE_MACHINE_AMD64 */
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;      /* 0 for images                      */
    uint32_t NumberOfSymbols;           /* 0 for images                      */
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} PE_COFF_HEADER;

/* Characteristics flags */
#define PE_CHAR_EXECUTABLE_IMAGE    0x0002
#define PE_CHAR_LARGE_ADDRESS_AWARE 0x0020
#define PE_CHAR_DLL                 0x2000

/* ── PE32+ Optional Header (112 bytes, excludes data directories) ────────── */
typedef struct {
    uint16_t Magic;                     /* 0x020B = PE32+                    */
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;       /* RVA of entry point                */
    uint32_t BaseOfCode;                /* RVA of .text                      */
    uint64_t ImageBase;                 /* Preferred load address            */
    uint32_t SectionAlignment;          /* In-memory alignment (0x1000)      */
    uint32_t FileAlignment;             /* On-disk alignment (0x200)         */
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;         /* Must be 0                         */
    uint32_t SizeOfImage;               /* Total virtual size, sec-aligned   */
    uint32_t SizeOfHeaders;             /* All headers, file-aligned         */
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;               /* Must be 0                         */
    uint32_t NumberOfRvaAndSizes;       /* 16                                */
} PE_OPTIONAL_HEADER_64;

/* Subsystem values */
#define PE_SUBSYSTEM_UNKNOWN           0
#define PE_SUBSYSTEM_NATIVE            1
#define PE_SUBSYSTEM_WINDOWS_GUI       2
#define PE_SUBSYSTEM_WINDOWS_CUI       3
#define PE_SUBSYSTEM_EFI_APPLICATION  10

/* DllCharacteristics flags */
#define PE_DLLCHAR_HIGH_ENTROPY_VA     0x0020
#define PE_DLLCHAR_DYNAMIC_BASE        0x0040
#define PE_DLLCHAR_FORCE_INTEGRITY     0x0080
#define PE_DLLCHAR_NX_COMPAT           0x0100
#define PE_DLLCHAR_NO_ISOLATION        0x0200
#define PE_DLLCHAR_NO_SEH              0x0400
#define PE_DLLCHAR_NO_BIND             0x0800
#define PE_DLLCHAR_APPCONTAINER        0x1000
#define PE_DLLCHAR_WDM_DRIVER          0x2000
#define PE_DLLCHAR_GUARD_CF            0x4000
#define PE_DLLCHAR_TERMINAL_SERVER     0x8000

/* ── Data Directory Entry (8 bytes) ──────────────────────────────────────── */
typedef struct {
    uint32_t VirtualAddress;
    uint32_t Size;
} PE_DATA_DIRECTORY;

/* Data directory indices */
#define PE_DIR_EXPORT           0
#define PE_DIR_IMPORT           1
#define PE_DIR_RESOURCE         2
#define PE_DIR_EXCEPTION        3
#define PE_DIR_SECURITY         4
#define PE_DIR_BASERELOC        5
#define PE_DIR_DEBUG            6
#define PE_DIR_ARCHITECTURE     7
#define PE_DIR_GLOBALPTR        8
#define PE_DIR_TLS              9
#define PE_DIR_LOAD_CONFIG     10
#define PE_DIR_BOUND_IMPORT    11
#define PE_DIR_IAT             12
#define PE_DIR_DELAY_IMPORT    13
#define PE_DIR_CLR_RUNTIME     14
#define PE_DIR_RESERVED        15
#define PE_NUM_DATA_DIRECTORIES 16

/* ── Section Header (40 bytes) ───────────────────────────────────────────── */
typedef struct {
    char     Name[8];
    uint32_t VirtualSize;               /* Actual data size in memory        */
    uint32_t VirtualAddress;            /* RVA of section start              */
    uint32_t SizeOfRawData;             /* File-aligned size on disk         */
    uint32_t PointerToRawData;          /* File offset of section data       */
    uint32_t PointerToRelocations;      /* 0 for images                      */
    uint32_t PointerToLinenumbers;      /* 0                                 */
    uint16_t NumberOfRelocations;       /* 0 for images                      */
    uint16_t NumberOfLinenumbers;       /* 0                                 */
    uint32_t Characteristics;
} PE_SECTION_HEADER;

/* Section characteristics */
#define PE_SCN_CNT_CODE                0x00000020
#define PE_SCN_CNT_INITIALIZED_DATA    0x00000040
#define PE_SCN_CNT_UNINITIALIZED_DATA  0x00000080
#define PE_SCN_MEM_EXECUTE             0x20000000
#define PE_SCN_MEM_READ                0x40000000
#define PE_SCN_MEM_WRITE               0x80000000u

/* ── Import Directory Table Entry (20 bytes) ─────────────────────────────── */
typedef struct {
    uint32_t OriginalFirstThunk;        /* RVA -> ILT (Import Lookup Table)  */
    uint32_t TimeDateStamp;             /* 0 until bound                     */
    uint32_t ForwarderChain;            /* -1 if none                        */
    uint32_t Name;                      /* RVA -> DLL name (ASCIIZ)          */
    uint32_t FirstThunk;                /* RVA -> IAT (Import Address Table) */
} PE_IMPORT_DESCRIPTOR;

/* ── Import Lookup / Address Table Entry — PE32+ (8 bytes) ───────────────── */
/* Bit 63 = 0: bits [0:30] = RVA to IMAGE_IMPORT_BY_NAME                     */
/* Bit 63 = 1: bits [0:15] = Ordinal number                                  */
typedef struct {
    uint64_t Value;
} PE_THUNK_DATA_64;

/* ── IMAGE_IMPORT_BY_NAME (variable) ─────────────────────────────────────── */
typedef struct {
    uint16_t Hint;
    /* char Name[]; — null-terminated, 2-byte aligned                        */
} PE_IMPORT_BY_NAME;

/* ── Base Relocation Block ───────────────────────────────────────────────── */
typedef struct {
    uint32_t VirtualAddress;            /* Page RVA                          */
    uint32_t SizeOfBlock;               /* Including header + entries        */
    /* uint16_t TypeOffset[];  — (type << 12) | offset                       */
} PE_BASE_RELOCATION;

#define PE_REL_BASED_ABSOLUTE  0
#define PE_REL_BASED_DIR64    10

/* ── DOS Stub machine code ───────────────────────────────────────────────── */
/* Prints "This program cannot be run in DOS mode.\r\r\n$" and exits         */
#define PE_DOS_STUB_SIZE 64
extern const uint8_t PE_DOS_STUB[PE_DOS_STUB_SIZE];

#pragma pack(pop)

/*═══════════════════════════════════════════════════════════════════════════
 * Emitter — capacity limits
 *═══════════════════════════════════════════════════════════════════════════*/

#define EM_MAX_CODE_SIZE    65536
#define EM_MAX_DATA_SIZE    65536
#define EM_MAX_BSS_SIZE     65536
#define EM_MAX_IMPORTS      256
#define EM_MAX_IMPORT_DLLS  32
#define EM_MAX_LABELS       1024
#define EM_MAX_FIXUPS       4096
#define EM_MAX_SECTIONS     16
#define EM_MAX_RELOCS       4096
#define EM_MAX_EXPORTS      256

/*═══════════════════════════════════════════════════════════════════════════
 * Emitter — internal types
 *═══════════════════════════════════════════════════════════════════════════*/

typedef struct {
    char     name[64];
    uint32_t offset;                    /* Byte offset within code buffer    */
    uint8_t  exported;                  /* 1 if marked for export            */
} EmLabel;

typedef enum {
    EM_FIXUP_RIP_REL32,                 /* RIP-relative disp32               */
    EM_FIXUP_RIP_REL32_DATA,            /* RIP-relative disp32 -> .rdata     */
    EM_FIXUP_RIP_REL32_IAT,             /* RIP-relative disp32 -> IAT slot   */
    EM_FIXUP_LABEL_REL32,               /* Intra-code rel32 (jmp/call)       */
    EM_FIXUP_ABS64_DATA,                /* Absolute 64-bit -> .rdata         */
    EM_FIXUP_ABS64_CODE,                /* Absolute 64-bit -> .text          */
} EmFixupKind;

typedef struct {
    EmFixupKind kind;
    uint32_t    patch_offset;           /* Offset in code[] to patch         */
    uint32_t    target_index;           /* Import ID / data offset / label   */
    int32_t     addend;
} EmFixup;

typedef struct {
    char     dll_name[64];
    char     func_names[EM_MAX_IMPORTS][64];
    uint32_t func_count;
    uint32_t iat_rva[EM_MAX_IMPORTS];   /* Resolved during layout            */
} EmImportDll;

typedef struct {
    char     name[64];
    uint32_t code_offset;               /* Label offset in .text             */
    uint16_t ordinal;                   /* Assigned during layout            */
} EmExportEntry;

typedef struct {
    /* ── Code section (.text) ────────────────────────────────────────────── */
    uint8_t  code[EM_MAX_CODE_SIZE];
    uint32_t code_len;

    /* ── Read-only data section (.rdata) ─────────────────────────────────── */
    uint8_t  rdata[EM_MAX_DATA_SIZE];
    uint32_t rdata_len;

    /* ── Read-write data section (.data) ─────────────────────────────────── */
    uint8_t  rwdata[EM_MAX_DATA_SIZE];
    uint32_t rwdata_len;

    /* ── BSS size (.bss) ─────────────────────────────────────────────────── */
    uint32_t bss_size;

    /* ── Labels ──────────────────────────────────────────────────────────── */
    EmLabel  labels[EM_MAX_LABELS];
    uint32_t label_count;

    /* ── Fixups ──────────────────────────────────────────────────────────── */
    EmFixup  fixups[EM_MAX_FIXUPS];
    uint32_t fixup_count;

    /* ── Imports ─────────────────────────────────────────────────────────── */
    EmImportDll imports[EM_MAX_IMPORT_DLLS];
    uint32_t    import_dll_count;

    /* ── Exports ─────────────────────────────────────────────────────────── */
    EmExportEntry exports[EM_MAX_EXPORTS];
    uint32_t      export_count;
    char          export_module_name[64];

    /* ── Base Relocations ────────────────────────────────────────────────── */
    uint32_t reloc_rvas[EM_MAX_RELOCS]; /* Code offsets needing relocation   */
    uint32_t reloc_count;

    /* ── PE layout configuration ─────────────────────────────────────────── */
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint16_t characteristics;           /* COFF characteristics              */
    uint32_t entry_point_label;         /* Label index for AddressOfEntryPoint */
    uint64_t stack_reserve;
    uint64_t stack_commit;
    uint64_t heap_reserve;
    uint64_t heap_commit;

    /* ── Resolved layout (filled by pe_layout) ───────────────────────────── */
    uint32_t text_rva;
    uint32_t rdata_rva;
    uint32_t rwdata_rva;
    uint32_t bss_rva;
    uint32_t idata_rva;
    uint32_t reloc_rva;
    uint32_t edata_rva;
    uint32_t headers_size;
    uint32_t image_size;

    /* ── Error state ─────────────────────────────────────────────────────── */
    int      error;
    char     error_msg[256];
} Emitter;

/*═══════════════════════════════════════════════════════════════════════════
 * API — Lifecycle
 *═══════════════════════════════════════════════════════════════════════════*/

/* Initialize emitter with defaults for x64 PE32+ console executable */
void em_init(Emitter *e);

/* Initialize emitter for DLL output */
void em_init_dll(Emitter *e, const char *module_name);

/* Reset without changing configuration */
void em_reset(Emitter *e);

/*═══════════════════════════════════════════════════════════════════════════
 * API — Configuration
 *═══════════════════════════════════════════════════════════════════════════*/

void em_set_image_base(Emitter *e, uint64_t base);
void em_set_subsystem(Emitter *e, uint16_t subsystem);
void em_set_alignment(Emitter *e, uint32_t section_align, uint32_t file_align);
void em_set_stack(Emitter *e, uint64_t reserve, uint64_t commit);
void em_set_heap(Emitter *e, uint64_t reserve, uint64_t commit);
void em_set_entry_label(Emitter *e, const char *label_name);
void em_set_dll_characteristics(Emitter *e, uint16_t flags);

/*═══════════════════════════════════════════════════════════════════════════
 * API — Raw Emission (into .text code buffer)
 *═══════════════════════════════════════════════════════════════════════════*/

void     em_byte(Emitter *e, uint8_t b);
void     em_bytes(Emitter *e, const uint8_t *buf, uint32_t len);
void     em_u16(Emitter *e, uint16_t v);
void     em_u32(Emitter *e, uint32_t v);
void     em_u64(Emitter *e, uint64_t v);
void     em_align(Emitter *e, uint32_t alignment);   /* NOP-pad to boundary */
uint32_t em_pos(const Emitter *e);                    /* Current code offset */

/*═══════════════════════════════════════════════════════════════════════════
 * API — Data Sections
 *═══════════════════════════════════════════════════════════════════════════*/

/* Append to .rdata, return offset within rdata buffer */
uint32_t em_rdata_bytes(Emitter *e, const void *buf, uint32_t len);
uint32_t em_rdata_string(Emitter *e, const char *str);
uint32_t em_rdata_wstring(Emitter *e, const uint16_t *wstr, uint32_t charcount);
uint32_t em_rdata_u32(Emitter *e, uint32_t v);
uint32_t em_rdata_u64(Emitter *e, uint64_t v);

/* Append to .data (read-write), return offset within rwdata buffer */
uint32_t em_rwdata_bytes(Emitter *e, const void *buf, uint32_t len);
uint32_t em_rwdata_zero(Emitter *e, uint32_t len);

/* Reserve BSS space, return offset within BSS */
uint32_t em_bss_reserve(Emitter *e, uint32_t len);

/*═══════════════════════════════════════════════════════════════════════════
 * API — Labels
 *═══════════════════════════════════════════════════════════════════════════*/

/* Define label at current code position */
void     em_label(Emitter *e, const char *name);

/* Define exported label (DLL exports) */
void     em_label_export(Emitter *e, const char *name);

/* Find label index, returns -1 if not found */
int      em_find_label(const Emitter *e, const char *name);

/* Get label offset */
uint32_t em_label_offset(const Emitter *e, int label_index);

/*═══════════════════════════════════════════════════════════════════════════
 * API — Imports
 *═══════════════════════════════════════════════════════════════════════════*/

/* Register an import. Returns a packed import ID (dll_idx << 16 | func_idx) */
uint32_t em_import(Emitter *e, const char *dll, const char *func);

/*═══════════════════════════════════════════════════════════════════════════
 * API — Fixups (register a patch to code buffer resolved at layout time)
 *═══════════════════════════════════════════════════════════════════════════*/

/* Emit CALL QWORD PTR [RIP+disp32] targeting an IAT slot */
void em_call_import(Emitter *e, uint32_t import_id);

/* Emit JMP QWORD PTR [RIP+disp32] targeting an IAT slot */
void em_jmp_import(Emitter *e, uint32_t import_id);

/* Emit LEA reg, [RIP+disp32] targeting a .rdata offset
   reg encoding: 0=rax,1=rcx,2=rdx,3=rbx,4=rsp,5=rbp,6=rsi,7=rdi
   For r8-r15 use em_lea_rN_rip_rdata */
void em_lea_reg_rip_rdata(Emitter *e, uint8_t reg, uint32_t rdata_offset);

/* LEA r8..r15, [RIP+disp32] targeting .rdata */
void em_lea_rN_rip_rdata(Emitter *e, uint8_t reg_n, uint32_t rdata_offset);

/* Emit CALL rel32 to a label (forward or backward) */
void em_call_label(Emitter *e, const char *label_name);

/* Emit JMP rel32 to a label */
void em_jmp_label(Emitter *e, const char *label_name);

/* Emit Jcc rel32 to a label (condition code: 0x84=JE, 0x85=JNE, etc.) */
void em_jcc_label(Emitter *e, uint8_t cc_opcode, const char *label_name);

/* Emit MOV reg, imm64 with absolute-address fixup to .rdata */
void em_mov_reg_abs_rdata(Emitter *e, uint8_t reg, uint32_t rdata_offset);

/*═══════════════════════════════════════════════════════════════════════════
 * API — x64 Instruction Emitters (complete set for backend use)
 *═══════════════════════════════════════════════════════════════════════════*/

/* ── Function Frame ──────────────────────────────────────────────────────── */

/* push rbp / mov rbp,rsp / sub rsp, N (N auto-aligned to 16+shadow) */
void em_prologue(Emitter *e, uint32_t local_bytes);

/* mov rsp,rbp / pop rbp / ret — Standard x64 stack frame teardown */
void em_epilogue(Emitter *e);

/* add rsp, N / ret — Epilogue without frame pointer */
void em_epilogue_noframe(Emitter *e, uint32_t local_bytes);

/* ── Register-Register ───────────────────────────────────────────────────── */
void em_mov_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_xor_r32_r32(Emitter *e, uint8_t dst, uint8_t src);  /* Also zeros upper 32 */
void em_add_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_sub_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_cmp_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_test_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_and_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_or_r64_r64(Emitter *e, uint8_t dst, uint8_t src);
void em_imul_r64_r64(Emitter *e, uint8_t dst, uint8_t src);

/* ── Register-Immediate ──────────────────────────────────────────────────── */
void em_mov_r64_imm64(Emitter *e, uint8_t reg, uint64_t imm);
void em_mov_r32_imm32(Emitter *e, uint8_t reg, uint32_t imm);
void em_add_r64_imm32(Emitter *e, uint8_t reg, int32_t imm);
void em_sub_r64_imm32(Emitter *e, uint8_t reg, int32_t imm);
void em_cmp_r64_imm32(Emitter *e, uint8_t reg, int32_t imm);
void em_and_r64_imm32(Emitter *e, uint8_t reg, int32_t imm);
void em_or_r64_imm32(Emitter *e, uint8_t reg, int32_t imm);
void em_shl_r64_imm8(Emitter *e, uint8_t reg, uint8_t count);
void em_shr_r64_imm8(Emitter *e, uint8_t reg, uint8_t count);
void em_sar_r64_imm8(Emitter *e, uint8_t reg, uint8_t count);

/* ── Memory Operations ───────────────────────────────────────────────────── */
void em_mov_r64_m64(Emitter *e, uint8_t reg, uint8_t base, int32_t disp);
void em_mov_m64_r64(Emitter *e, uint8_t base, int32_t disp, uint8_t reg);
void em_mov_r32_m32(Emitter *e, uint8_t reg, uint8_t base, int32_t disp);
void em_mov_m32_r32(Emitter *e, uint8_t base, int32_t disp, uint8_t reg);
void em_mov_r8_m8(Emitter *e, uint8_t reg, uint8_t base, int32_t disp);
void em_mov_m8_r8(Emitter *e, uint8_t base, int32_t disp, uint8_t reg);
void em_lea_r64_m(Emitter *e, uint8_t reg, uint8_t base, int32_t disp);

/* ── Stack Operations ────────────────────────────────────────────────────── */
void em_push_r64(Emitter *e, uint8_t reg);
void em_pop_r64(Emitter *e, uint8_t reg);
void em_push_imm32(Emitter *e, int32_t imm);

/* ── Control Flow ────────────────────────────────────────────────────────── */
void em_ret(Emitter *e);
void em_nop(Emitter *e);
void em_int3(Emitter *e);
void em_call_r64(Emitter *e, uint8_t reg);   /* CALL reg */
void em_jmp_r64(Emitter *e, uint8_t reg);    /* JMP reg */
void em_syscall(Emitter *e);
void em_nop_sled(Emitter *e, uint32_t count);

/* ── x64 Register Encoding ───────────────────────────────────────────────── */
#define REG_RAX  0
#define REG_RCX  1
#define REG_RDX  2
#define REG_RBX  3
#define REG_RSP  4
#define REG_RBP  5
#define REG_RSI  6
#define REG_RDI  7
#define REG_R8   8
#define REG_R9   9
#define REG_R10 10
#define REG_R11 11
#define REG_R12 12
#define REG_R13 13
#define REG_R14 14
#define REG_R15 15

/* ── Win64 ABI convenience ───────────────────────────────────────────────── */
#define REG_ARG1 REG_RCX
#define REG_ARG2 REG_RDX
#define REG_ARG3 REG_R8
#define REG_ARG4 REG_R9
#define REG_RET  REG_RAX

/*═══════════════════════════════════════════════════════════════════════════
 * API — PE Output
 *═══════════════════════════════════════════════════════════════════════════*/

/* Compute layout, resolve all fixups. Returns 0 on success, -1 on error.
   After this call, code[] is patched and ready to write. */
int pe_layout(Emitter *e);

/* Write complete PE32+ to file. Calls pe_layout() internally if needed.
   Returns 0 on success, -1 on error. */
int pe_write(Emitter *e, const char *filename);

/* Write PE32+ to memory buffer. Caller provides buffer and size.
   Returns bytes written, or 0 on error. */
uint32_t pe_write_mem(Emitter *e, uint8_t *buf, uint32_t buf_size);

/* Get last error message */
const char *em_error(const Emitter *e);

#ifdef __cplusplus
}
#endif

#endif /* RAWRXD_PE_EMITTER_H */
