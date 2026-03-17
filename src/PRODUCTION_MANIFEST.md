# RawrXD Production Assembly Manifest

## BUILD STATUS: ✅ ALL FILES COMPILE SUCCESSFULLY

Tested: February 24, 2026

| File | Lines | OBJ Size | Status |
|------|-------|----------|--------|
| `RawrXD_Sovereign_Monolith.asm` | 4814 | 37KB | ✅ BUILDS |
| `pe_writer_production/RawrXD_Monolithic_PE_Emitter.asm` | 1911 | 8KB | ✅ BUILDS |
| `asm/RawrCodex.asm` | 9750 | 35KB | ✅ BUILDS |

**Final EXE: RawrXD_IDE.exe (17.9KB) - Zero Dependencies, No CRT, No Static Imports**

---

## PRODUCTION FILES (KEEP)

These are the only files needed for a complete build:

| File | Lines | PROCs | Purpose |
|------|-------|-------|---------|
| `RawrXD_Sovereign_Monolith.asm` | 4805 | 130 | Main IDE executable (entry: Start) |
| `pe_writer_production/RawrXD_Monolithic_PE_Emitter.asm` | 1912 | 42 | PE32+ writer library |
| `asm/RawrCodex.asm` | 9750 | 49 | Binary analysis engine |

**Total: 3 files, ~16,467 lines, 221 PROCs**

---

## BUILD CONFIGURATIONS

### Standalone IDE (No Dependencies)
```batch
ml64.exe /c /Fo RawrXD_Sovereign_Monolith.obj RawrXD_Sovereign_Monolith.asm
link.exe /ENTRY:Start /SUBSYSTEM:WINDOWS /NODEFAULTLIB RawrXD_Sovereign_Monolith.obj
```

### PE Writer Library
```batch
ml64.exe /c /Fo RawrXD_Monolithic_PE_Emitter.obj pe_writer_production\RawrXD_Monolithic_PE_Emitter.asm
```

### Binary Analyzer Library  
```batch
ml64.exe /c /Fo RawrCodex.obj asm\RawrCodex.asm
```

### Full System (All Components)
```batch
ml64.exe /c /Fo RawrXD_Sovereign_Monolith.obj RawrXD_Sovereign_Monolith.asm
ml64.exe /c /Fo RawrXD_Monolithic_PE_Emitter.obj pe_writer_production\RawrXD_Monolithic_PE_Emitter.asm  
ml64.exe /c /Fo RawrCodex.obj asm\RawrCodex.asm
link.exe /ENTRY:Start /SUBSYSTEM:WINDOWS /NODEFAULTLIB RawrXD_Sovereign_Monolith.obj RawrXD_Monolithic_PE_Emitter.obj RawrCodex.obj
```

---

## SCAFFOLDING FILES (SAFE TO DELETE)

The following 440+ .asm files are test artifacts, scaffolding, or superseded versions.
They can be safely archived or removed after verifying production builds work.

**DO NOT DELETE until production files compile successfully.**

---

## ARCHITECTURE

```
┌─────────────────────────────────────────────────────────────┐
│                   RawrXD_Sovereign_Monolith                 │
│                    (Entry Point: Start)                     │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────┐   │
│  │ PEB Walk    │ │ Gap Buffer   │ │ Render Engine      │   │
│  │ API Resolve │ │ Text Engine  │ │ GDI/D2D Bridge     │   │
│  └─────────────┘ └──────────────┘ └────────────────────┘   │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────┐   │
│  │ Lexer/Token │ │ Undo/Redo    │ │ Search/Replace     │   │
│  │ Highlight   │ │ State Mgmt   │ │ Regex SIMD         │   │
│  └─────────────┘ └──────────────┘ └────────────────────┘   │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────┐   │
│  │ Clipboard   │ │ File I/O     │ │ Process/Thread     │   │
│  │ Win32       │ │ Win32 Raw    │ │ Management         │   │
│  └─────────────┘ └──────────────┘ └────────────────────┘   │
│  ┌─────────────┐ ┌──────────────┐ ┌────────────────────┐   │
│  │ Heap Mgmt   │ │ Data Struct  │ │ Timing/Sync        │   │
│  │ Memory Pool │ │ LL/Stack/Q   │ │ Primitives         │   │
│  └─────────────┘ └──────────────┘ └────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
          │                                     │
          ▼                                     ▼
┌───────────────────────────┐   ┌──────────────────────────────┐
│ RawrXD_Monolithic_PE_     │   │        RawrCodex             │
│        Emitter            │   │   (Binary Analysis)          │
│ ┌───────────────────────┐ │   │ ┌──────────────────────────┐ │
│ │ DOS/NT/Section Header │ │   │ │ PE32/PE64 Parser         │ │
│ │ Writers               │ │   │ │ ELF32/ELF64 Parser       │ │
│ └───────────────────────┘ │   │ └──────────────────────────┘ │
│ ┌───────────────────────┐ │   │ ┌──────────────────────────┐ │
│ │ Import Table Builder  │ │   │ │ x64 Instruction Decoder  │ │
│ │ IAT/ILT Generator     │ │   │ │ REX/ModRM/SIB Parsing    │ │
│ └───────────────────────┘ │   │ └──────────────────────────┘ │
│ ┌───────────────────────┐ │   │ ┌──────────────────────────┐ │
│ │ Relocation Emitter    │ │   │ │ SSA Lifting              │ │
│ │ Fixup Generation      │ │   │ │ PHI Node Construction    │ │
│ └───────────────────────┘ │   │ └──────────────────────────┘ │
│ ┌───────────────────────┐ │   │ ┌──────────────────────────┐ │
│ │ x64 Machine Code      │ │   │ │ CFG Builder              │ │
│ │ Emitter (REX/ModRM)   │ │   │ │ Dominator Tree           │ │
│ └───────────────────────┘ │   │ └──────────────────────────┘ │
│ ┌───────────────────────┐ │   │ ┌──────────────────────────┐ │
│ │ PeBuilder_Build       │ │   │ │ Type Recovery            │ │
│ │ Complete PE Assembly  │ │   │ │ Pseudocode Emit          │ │
│ └───────────────────────┘ │   │ └──────────────────────────┘ │
└───────────────────────────┘   └──────────────────────────────┘
```

---

## PROC INVENTORY

### RawrXD_Sovereign_Monolith.asm (130 PROCs)
**PEB/API Resolution:** Start, FindDllBase, GetExportAddress, ResolvAllApis
**Text Engine:** GapBuffer_*, Line_*, Cursor_*
**Lexer:** Lexer_*, Token_*
**Undo:** UndoStack_*, Undo_*, Redo_*
**Search:** Search_*, Replace_*
**Clipboard:** Clipboard_*
**Viewport:** Viewport_*
**File I/O:** File_*
**Render:** Render_*, Draw_*
**Console:** Console_*
**Thread:** Thread_*
**Sync:** Mutex_*, Semaphore_*, Event_*, CriticalSection_*
**Process:** Process_*
**Timing:** Timer_*, Sleep_*, TickCount_*
**Heap:** Heap_*
**Data Structures:** LinkedList_*, Stack_*, Queue_*
**PE Emit:** Emit_DosHeader, Emit_NtHeaders, Emit_OptionalHeader64, Emit_SectionHeader

### RawrXD_Monolithic_PE_Emitter.asm (42 PROCs)
**PE Writer:** PeWriter_Init, PeWriter_WriteDosHeader, PeWriter_WriteNtHeaders, PeWriter_WriteSectionHeader, PeWriter_WriteSectionData, PeWriter_CreateMinimalExe
**Import Builder:** Import_Init, Import_AlignDword, Import_AlignQword, Import_WriteString, Import_WriteDword, Import_WriteQword, Import_WriteHintName, Import_BuildSimple
**Relocation:** Reloc_Init, Reloc_AddEntry, Reloc_Finalize
**Machine Code Emitter:** Emitter_Init, Emitter_EmitByte, Emitter_EmitDword, Emitter_EmitQword, Emitter_EmitMovRegImm64, Emitter_EmitRet, Emitter_EmitCallReg, Emitter_EmitPushReg, Emitter_EmitPopReg, Emitter_EmitSubRspImm8, Emitter_EmitAddRspImm8, Emitter_EmitXorRegReg, Emitter_EmitMovRegReg64, Emitter_EmitCallRipRel, Emitter_EmitJmpRipRel, Emitter_EmitLeaRipRel, Emitter_EmitNop, Emitter_EmitNopN, Emitter_EmitInt3
**Full Builder:** PeBuilder_Init, PeBuilder_AddCodeSection, PeBuilder_AddDataSection, PeBuilder_SetImportSection, PeBuilder_Build, PeBuilder_CreateHelloWorld

### RawrCodex.asm (49 PROCs)
**Core:** RawrCodex_Init, RawrCodex_Destroy
**PE Parsing:** ParsePE32, ParsePE64, ParseImportTable, ParseExportTable
**ELF Parsing:** ParseELF32, ParseELF64
**Disassembler:** Disasm_Init, Disasm_DecodeInstruction, Disasm_RecursiveDescent
**CFG:** CFG_Build, CFG_AddEdge, CFG_ComputeDominators
**SSA:** SSA_Lift, SSA_InsertPhi, SSA_Rename
**Type Recovery:** TypeRecover_Analyze
**Pseudocode:** Pseudocode_Emit
**Pattern Scanner:** Pattern_Init, Pattern_Search
**String Extraction:** Strings_Extract
**License:** RawrLicense_CheckFeature, RawrLicense_SetTier
