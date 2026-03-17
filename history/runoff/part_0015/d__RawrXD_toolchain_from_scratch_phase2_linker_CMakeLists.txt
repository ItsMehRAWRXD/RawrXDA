# ============================================================================
# Phase 2 Linker (rawrxd_link) - Fully Reverse Engineered PE/COFF Linker
# ============================================================================
#
# Architecture (5 phases):
#   Phase 1: coff_reader.c    - Parse COFF .obj (headers, sections, symbols, relocs)
#   Phase 2: section_merge.c  - Merge sections from multiple objects, assign RVAs
#   Phase 3: reloc_resolver.c - Apply x64 relocations (REL32, ADDR64, ADDR32NB, etc.)
#   Phase 4: pe_writer.c      - Generate complete PE32+ executable
#   Phase 5: entry_stub.c     - CRT entry point injection
#   Driver:  main.c           - CLI orchestrator wiring all phases
#
# Build: cmake -B build -G Ninja && cmake --build build
# ============================================================================

cmake_minimum_required(VERSION 3.16)
project(phase2_linker C)

set(CMAKE_C_STANDARD 99)

# ---- rawrxd_link: the linker executable ----
add_executable(rawrxd_link
  main.c              # Phase 5 driver / CLI
  coff_reader.c       # Phase 1: COFF parser
  section_merge.c     # Phase 2: section merger
  reloc_resolver.c    # Phase 3: relocation resolver
  pe_writer.c         # Phase 4: PE writer
  entry_stub.c        # Phase 5: entry stub code
)
target_include_directories(rawrxd_link PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  target_link_libraries(rawrxd_link PRIVATE kernel32)
endif()

# ---- rawrxd_check: diagnostic utility ----
add_executable(rawrxd_check
  rawrxd_check.c
  coff_reader.c       # Shares Phase 1 reader
)
target_include_directories(rawrxd_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  target_link_libraries(rawrxd_check PRIVATE kernel32)
endif()

# ---- Tests ----
add_subdirectory(tests)
