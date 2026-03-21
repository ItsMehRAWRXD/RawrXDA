# =============================================================================
# RawrXD Sovereign-Link v22.4.0 — PE64 manual IAT (NASM) + optional smoke EXE
# =============================================================================
# - Produces COFF .obj via NASM; MSVC link.exe still merges objects into a PE.
# - To emit a PE with zero link.exe, use a PE emitter (e.g. tools/pe_emitter.asm).
# =============================================================================

if(NOT WIN32)
    return()
endif()

find_program(RAWRXD_NASM_EXECUTABLE NAMES nasm PATHS ENV PATH)

if(NOT RAWRXD_NASM_EXECUTABLE)
    message(STATUS "Sovereign IAT: NASM not found — install NASM or add to PATH to build rawrxd_pe64_iat_v224 / rawrxd_sovereign_iat_smoke")
    return()
endif()

set(_RAWRXD_IAT_ASM "${CMAKE_SOURCE_DIR}/src/asm/RawrXD_PE64_IAT_Fabricator_v224.asm")
set(_RAWRXD_IAT_OBJ "${CMAKE_BINARY_DIR}/CMakeFiles/RawrXD_PE64_IAT_Fabricator_v224.obj")

set(_RAWRXD_SOVEREIGN_ENTRY_ASM "${CMAKE_SOURCE_DIR}/src/asm/RawrXD_Sovereign_MinimalEntry_v224.asm")
set(_RAWRXD_SOVEREIGN_ENTRY_OBJ "${CMAKE_BINARY_DIR}/CMakeFiles/RawrXD_Sovereign_MinimalEntry_v224.obj")

add_custom_command(
    OUTPUT ${_RAWRXD_IAT_OBJ}
    COMMAND "${RAWRXD_NASM_EXECUTABLE}" -f win64 -o "${_RAWRXD_IAT_OBJ}" "${_RAWRXD_IAT_ASM}"
    DEPENDS "${_RAWRXD_IAT_ASM}"
    COMMENT "NASM: RawrXD PE64 IAT fabricator v22.4.0"
    VERBATIM
)

add_custom_command(
    OUTPUT ${_RAWRXD_SOVEREIGN_ENTRY_OBJ}
    COMMAND "${RAWRXD_NASM_EXECUTABLE}" -f win64 -o "${_RAWRXD_SOVEREIGN_ENTRY_OBJ}" "${_RAWRXD_SOVEREIGN_ENTRY_ASM}"
    DEPENDS "${_RAWRXD_SOVEREIGN_ENTRY_ASM}"
    COMMENT "NASM: Sovereign minimal entry v22.4.0"
    VERBATIM
)

set_source_files_properties(${_RAWRXD_IAT_OBJ} ${_RAWRXD_SOVEREIGN_ENTRY_OBJ}
    PROPERTIES EXTERNAL_OBJECT TRUE GENERATED TRUE LANGUAGE CXX
)

# Static library: IAT object only (link into EXE/DLL targets that supply their own entry / other objects)
add_library(rawrxd_pe64_iat_v224 STATIC ${_RAWRXD_IAT_OBJ})
set_target_properties(rawrxd_pe64_iat_v224 PROPERTIES OUTPUT_NAME rawrxd_pe64_iat_v224)

# Optional smoke EXE: MessageBoxA + ExitProcess via manual IAT (no CRT / no import .lib).
# Invoked with link.exe directly so global CMake /LTCG + implicit kernel32.lib are not injected.
set(_RAWRXD_SOVEREIGN_SMOKE_EXE "${CMAKE_BINARY_DIR}/bin/rawrxd_sovereign_iat_smoke.exe")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC AND DEFINED CMAKE_LINKER)
    add_custom_command(
        OUTPUT ${_RAWRXD_SOVEREIGN_SMOKE_EXE}
        COMMAND "${CMAKE_LINKER}" /NOLOGO /MACHINE:X64 /SUBSYSTEM:WINDOWS /ENTRY:main
                /NODEFAULTLIB /IGNORE:4078
                "${_RAWRXD_SOVEREIGN_ENTRY_OBJ}" "${_RAWRXD_IAT_OBJ}"
                "/OUT:${_RAWRXD_SOVEREIGN_SMOKE_EXE}"
        DEPENDS ${_RAWRXD_SOVEREIGN_ENTRY_OBJ} ${_RAWRXD_IAT_OBJ}
        COMMENT "Link: rawrxd_sovereign_iat_smoke (manual IAT only, no import .lib)"
        VERBATIM
    )
    add_custom_target(rawrxd_sovereign_iat_smoke DEPENDS ${_RAWRXD_SOVEREIGN_SMOKE_EXE})
    set_target_properties(rawrxd_sovereign_iat_smoke PROPERTIES EXCLUDE_FROM_ALL TRUE)
else()
    message(STATUS "Sovereign IAT: rawrxd_sovereign_iat_smoke skipped (MSVC CMAKE_LINKER not defined)")
endif()

message(STATUS "Sovereign IAT: rawrxd_pe64_iat_v224 (+ optional rawrxd_sovereign_iat_smoke: cmake --build . --target rawrxd_sovereign_iat_smoke)")
