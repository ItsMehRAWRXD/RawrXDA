# =============================================================================
# RawrXD Reverse Engineering Suite — Master Build Configuration
# =============================================================================
#
# Sub-modules:
#   codex/           — RawrCodex advanced binary analysis (C++ headers + MASM64)
#   omega_suite/     — OMEGA-POLYGLOT PE analyzer family (MASM32/64 ASM)
#   disassembler/    — x86/x64 instruction decoder (MASM64)
#   deobfuscator/    — Anti-obfuscation engine + MetaReverse (MASM64)
#   reverser_compiler/ — Self-hosting "Reverser" language compiler (NASM/MASM64)
#   pe_tools/        — PE analysis utilities (C++)
#   model_reverse/   — LLM model reverse-loading pipeline (MASM64)
#   security_toolkit/ — Offensive security toolkit (MASM64)
#   re_modules/      — Additional RE modules (GPU DMA, etc.)
#
# Build targets:
#   RawrXD-RE-Library  — Static library of C++ RE code (headers + pe_tools)
#   re_omega_suite     — MASM32 omega PE analyzer executables
#   re_deobfuscator    — MASM64 deobfuscator objects
#   re_disassembler    — MASM64 disassembler objects (via src/asm/RawrCodex.asm)
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)

# --- C++ RE Library (Headers + PE Tools) ------------------------------------
# This library bundles the C++ reverse engineering headers and utilities.
# It is linked into both RawrEngine and RawrXD-Win32IDE.

set(RE_CPP_SOURCES
    pe_tools/re_tools.cpp
)

set(RE_CPP_HEADERS
    RawrCodex.hpp
    RawrCompiler.hpp
    RawrDumpBin.hpp
    RawrReverseEngine.hpp
    pe_tools/re_tools.h
)

add_library(RawrXD-RE-Library STATIC ${RE_CPP_SOURCES})

target_include_directories(RawrXD-RE-Library PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pe_tools
)

target_include_directories(RawrXD-RE-Library PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(WIN32)
    target_link_libraries(RawrXD-RE-Library PRIVATE shlwapi dbghelp)
endif()

# --- MASM64 Deobfuscator Objects (MSVC only) --------------------------------
# These are assembled into .obj files for linking into the main binary.

if(RAWR_HAS_MASM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/deobfuscator/RawrXD_OmegaDeobfuscator.asm")
    set(RE_DEOBF_ASM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/deobfuscator/RawrXD_OmegaDeobfuscator.asm")
    set(RE_DEOBF_ASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_OmegaDeobfuscator.obj")
    add_custom_command(
        OUTPUT ${RE_DEOBF_ASM_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${RE_DEOBF_ASM_OBJ} ${RE_DEOBF_ASM_SRC}
        DEPENDS ${RE_DEOBF_ASM_SRC}
        COMMENT "[MASM64] Assembling RawrXD_OmegaDeobfuscator.asm (AVX-512 deobfuscation)"
        VERBATIM
    )

    set(RE_META_ASM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/deobfuscator/RawrXD_MetaReverse.asm")
    set(RE_META_ASM_OBJ "${CMAKE_CURRENT_BINARY_DIR}/RawrXD_MetaReverse.obj")
    add_custom_command(
        OUTPUT ${RE_META_ASM_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${RE_META_ASM_OBJ} ${RE_META_ASM_SRC}
        DEPENDS ${RE_META_ASM_SRC}
        COMMENT "[MASM64] Assembling RawrXD_MetaReverse.asm (authenticity detection)"
        VERBATIM
    )

    set(RE_DEOBF_OBJECTS ${RE_DEOBF_ASM_OBJ} ${RE_META_ASM_OBJ} PARENT_SCOPE)
    
    add_custom_target(re_deobfuscator DEPENDS ${RE_DEOBF_ASM_OBJ} ${RE_META_ASM_OBJ})
    message(STATUS "[RE] Deobfuscator MASM64 targets registered")
endif()

# --- MASM64 Model Reverse Pipeline Objects ----------------------------------
if(RAWR_HAS_MASM AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model_reverse/model-llm-harvester.asm")
    set(RE_HARVESTER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/model_reverse/model-llm-harvester.asm")
    set(RE_HARVESTER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/model_llm_harvester.obj")
    add_custom_command(
        OUTPUT ${RE_HARVESTER_OBJ}
        COMMAND "${CMAKE_ASM_MASM_COMPILER}" /c /Zi /Zd /Fo${RE_HARVESTER_OBJ} ${RE_HARVESTER_SRC}
        DEPENDS ${RE_HARVESTER_SRC}
        COMMENT "[MASM64] Assembling model-llm-harvester.asm (LLM weight generator)"
        VERBATIM
    )

    set(RE_MODEL_OBJECTS ${RE_HARVESTER_OBJ} PARENT_SCOPE)
    add_custom_target(re_model_reverse DEPENDS ${RE_HARVESTER_OBJ})
    message(STATUS "[RE] Model reverse pipeline MASM64 target registered")
endif()

# --- Omega Suite Build Script (MASM32 — standalone executables) -------------
# The omega_suite/ contains MASM32 sources that build with C:\masm32\
# These are standalone PE analysis tools, built via batch scripts.
# We register a custom target that invokes the appropriate build command.

if(EXISTS "C:/masm32/bin/ml.exe")
    set(OMEGA_V3_SRC "${CMAKE_CURRENT_SOURCE_DIR}/omega_suite/v3/omega_pro_v3.asm")
    set(OMEGA_V3_EXE "${CMAKE_CURRENT_BINARY_DIR}/omega_pro_v3.exe")

    add_custom_command(
        OUTPUT ${OMEGA_V3_EXE}
        COMMAND cmd /c "C:\\masm32\\bin\\ml.exe /c /coff /Fo${CMAKE_CURRENT_BINARY_DIR}/omega_pro_v3.obj ${OMEGA_V3_SRC}"
        COMMAND cmd /c "C:\\masm32\\bin\\link.exe /SUBSYSTEM:CONSOLE /OUT:${OMEGA_V3_EXE} ${CMAKE_CURRENT_BINARY_DIR}/omega_pro_v3.obj C:\\masm32\\lib\\kernel32.lib C:\\masm32\\lib\\user32.lib C:\\masm32\\lib\\masm32.lib"
        DEPENDS ${OMEGA_V3_SRC}
        COMMENT "[MASM32] Building omega_pro_v3.exe (PE Analyzer)"
        VERBATIM
    )
    add_custom_target(re_omega_v3 DEPENDS ${OMEGA_V3_EXE})
    message(STATUS "[RE] Omega v3 MASM32 target registered")
endif()

# --- Reverser Compiler (NASM — self-hosting compiler suite) -----------------
# The reverser_compiler/ is a complete compiler infrastructure in assembly.
# It uses NASM syntax. We provide a build target for the core compiler.

find_program(NASM_EXECUTABLE nasm PATHS "C:/nasm" "C:/nasm/nasm-2.16.03" ENV PATH)
if(NASM_EXECUTABLE)
    set(REVERSER_LEXER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/reverser_compiler/reverser_lexer.asm")
    set(REVERSER_LEXER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/reverser_lexer.obj")
    add_custom_command(
        OUTPUT ${REVERSER_LEXER_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f win64 -o ${REVERSER_LEXER_OBJ} ${REVERSER_LEXER_SRC}
        DEPENDS ${REVERSER_LEXER_SRC}
        COMMENT "[NASM] Assembling reverser_lexer.asm"
        VERBATIM
    )

    set(REVERSER_PARSER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/reverser_compiler/reverser_parser.asm")
    set(REVERSER_PARSER_OBJ "${CMAKE_CURRENT_BINARY_DIR}/reverser_parser.obj")
    add_custom_command(
        OUTPUT ${REVERSER_PARSER_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f win64 -o ${REVERSER_PARSER_OBJ} ${REVERSER_PARSER_SRC}
        DEPENDS ${REVERSER_PARSER_SRC}
        COMMENT "[NASM] Assembling reverser_parser.asm"
        VERBATIM
    )

    set(REVERSER_AST_SRC "${CMAKE_CURRENT_SOURCE_DIR}/reverser_compiler/reverser_ast.asm")
    set(REVERSER_AST_OBJ "${CMAKE_CURRENT_BINARY_DIR}/reverser_ast.obj")
    add_custom_command(
        OUTPUT ${REVERSER_AST_OBJ}
        COMMAND ${NASM_EXECUTABLE} -f win64 -o ${REVERSER_AST_OBJ} ${REVERSER_AST_SRC}
        DEPENDS ${REVERSER_AST_SRC}
        COMMENT "[NASM] Assembling reverser_ast.asm"
        VERBATIM
    )

    set(REVERSER_OBJECTS ${REVERSER_LEXER_OBJ} ${REVERSER_PARSER_OBJ} ${REVERSER_AST_OBJ} PARENT_SCOPE)
    add_custom_target(re_reverser_compiler DEPENDS ${REVERSER_LEXER_OBJ} ${REVERSER_PARSER_OBJ} ${REVERSER_AST_OBJ})
    message(STATUS "[RE] Reverser compiler NASM targets registered")
else()
    message(STATUS "[RE] NASM not found — Reverser compiler targets skipped")
endif()

# --- Summary ----------------------------------------------------------------
message(STATUS "[RE] Reverse Engineering Suite configured:")
message(STATUS "  C++ Library:       RawrXD-RE-Library (${RE_CPP_SOURCES})")
message(STATUS "  Headers:           4 HPP files (Codex, Compiler, DumpBin, ReverseEngine)")
message(STATUS "  Omega Suite:       19 ASM files across v3/v4/v5/v7 + src_variants")
message(STATUS "  Deobfuscator:      3 ASM files + 1 INC + build/test")
message(STATUS "  Reverser Compiler: 11 ASM + 6 tests + 1 .rev source")
message(STATUS "  PE Tools:          re_tools.cpp/h (PE analyzer + inline compiler)")
message(STATUS "  Model Reverse:     model-llm-harvester.asm")
message(STATUS "  Security Toolkit:  RawrXD_IDE_unified.asm")
message(STATUS "  RE Modules:        gpu_dma_complete_reverse_engineered.asm")
