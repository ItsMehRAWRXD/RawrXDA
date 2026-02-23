# Phase 2 linker smoke test.
# Prereqs: MSVC (cl) and rawrxd_link built. Run from Developer Command Prompt or
# use build_test_obj.bat then link manually.
cmake_minimum_required(VERSION 3.10)

set(SMOKE_C ${CMAKE_CURRENT_SOURCE_DIR}/linker_smoke.c)
set(SMOKE_OBJ ${CMAKE_CURRENT_BINARY_DIR}/linker_smoke.obj)
set(SMOKE_EXE ${CMAKE_CURRENT_BINARY_DIR}/linker_smoke.exe)
# When phase2_linker is the top-level project, linker lives in build/; when tests is add_subdirectory, in parent build/
set(LINKER_EXE "$<TARGET_FILE:rawrxd_link>")

if(MSVC AND TARGET rawrxd_link)
  add_custom_target(linker_smoke_test
    COMMAND ${CMAKE_C_COMPILER} /nologo /c ${SMOKE_C} /Fo${SMOKE_OBJ}
    COMMAND ${LINKER_EXE} ${SMOKE_OBJ} -o ${SMOKE_EXE}
    COMMAND ${SMOKE_EXE}
    COMMENT "Phase 2 linker smoke: compile -> link -> run (expect exit 42)"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS rawrxd_link
  )
  message(STATUS "Phase 2: target 'linker_smoke_test' (run with: cmake --build . --target linker_smoke_test)")
elseif(MSVC)
  set(LINKER_EXE "${CMAKE_CURRENT_SOURCE_DIR}/../build/rawrxd_link.exe")
  add_custom_target(linker_smoke_test
    COMMAND ${CMAKE_C_COMPILER} /nologo /c ${SMOKE_C} /Fo${SMOKE_OBJ}
    COMMAND ${LINKER_EXE} ${SMOKE_OBJ} -o ${SMOKE_EXE}
    COMMAND ${SMOKE_EXE}
    COMMENT "Phase 2 linker smoke (standalone): compile -> link -> run"
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  message(STATUS "Phase 2: target 'linker_smoke_test' (linker at ../build/rawrxd_link.exe)")
else()
  message(STATUS "Phase 2 linker_smoke_test: use MSVC to build .obj, then run rawrxd_link manually")
endif()
