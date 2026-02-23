# Phase 2 linker — COFF -> PE32+. Build with system C compiler only.
cmake_minimum_required(VERSION 3.10)
project(phase2_linker C)

set(CMAKE_C_STANDARD 99)

add_executable(rawrxd_link
  main.c
  coff_reader.c
  pe_writer.c
  reloc_resolver.c
  entry_stub.c
  section_merge.c
)
target_include_directories(rawrxd_link PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# PE validator — standalone diagnostic for "invalid application" (e.g. rawrxd_check.exe test.exe)
add_executable(rawrxd_check rawrxd_check.c)
target_include_directories(rawrxd_check PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Optional smoke test (MSVC: compile linker_smoke.c -> link -> run, expect exit 42)
add_subdirectory(tests)

