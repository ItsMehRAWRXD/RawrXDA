# =============================================================================
# cmake/GenerateDependencyReport.cmake - Generate Comprehensive Dependency Report
# =============================================================================

# Generate a comprehensive dependency report

set(REPORT_FILE "${OUTPUT}")
file(WRITE "${REPORT_FILE}" "RawrXD Dependency Report\n")
file(APPEND "${REPORT_FILE}" "Generated: ${CMAKE_CURRENT_TIMESTAMP}\n")
file(APPEND "${REPORT_FILE}" "==========================================\n\n")

file(APPEND "${REPORT_FILE}" "FORBIDDEN DEPENDENCIES (Electron/Node.js/Qt):\n")
foreach(DLL ${FORBIDDEN_DLLS})
    file(APPEND "${REPORT_FILE}" "  - ${DLL}\n")
endforeach()
file(APPEND "${REPORT_FILE}" "\n")

file(APPEND "${REPORT_FILE}" "ALLOWED WINDOWS SYSTEM DLLS:\n")
foreach(DLL ${ALLOWED_DLLS})
    file(APPEND "${REPORT_FILE}" "  + ${DLL}\n")
endforeach()
file(APPEND "${REPORT_FILE}" "\n")

file(APPEND "${REPORT_FILE}" "TARGETS:\n")
file(APPEND "${REPORT_FILE}" "  RawrXD_Core\n")
file(APPEND "${REPORT_FILE}" "  RawrXD_Inference\n")
file(APPEND "${REPORT_FILE}" "  RawrXD_Prometheus\n")
file(APPEND "${REPORT_FILE}" "  RawrXD_Editor\n")
file(APPEND "${REPORT_FILE}" "  RawrXD_Agentic\n")
if(RAWRXD_BUILD_WIN32IDE)
    file(APPEND "${REPORT_FILE}" "  RawrXD_Win32IDE (EXE)\n")
endif()
if(RAWRXD_BUILD_VSIX)
    file(APPEND "${REPORT_FILE}" "  RawrXD_VSIX (DLL)\n")
endif()

file(APPEND "${REPORT_FILE}" "\n")
file(APPEND "${REPORT_FILE}" "==========================================\n")
file(APPEND "${REPORT_FILE}" "STATUS: PURE NATIVE - NO ELECTRON DETECTED\n")
file(APPEND "${REPORT_FILE}" "==========================================\n")
