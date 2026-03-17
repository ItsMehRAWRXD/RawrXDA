if(NOT DEFINED INTERCONNECT_DIR)
    message(FATAL_ERROR "[Interconnect Gate] INTERCONNECT_DIR not set")
endif()

set(_primary "${INTERCONNECT_DIR}/RawrXD_Interconnect.dll")
set(_secondary "")
if(DEFINED INTERCONNECT_OPTIONAL_DIR)
    set(_secondary "${INTERCONNECT_OPTIONAL_DIR}/RawrXD_Interconnect.dll")
endif()

if(EXISTS "${_primary}")
    message(STATUS "[Interconnect Gate] Found ${_primary}")
    return()
endif()

if(_secondary AND EXISTS "${_secondary}")
    message(STATUS "[Interconnect Gate] Found ${_secondary}")
    return()
endif()

message(FATAL_ERROR
    "[Interconnect Gate] RawrXD_Interconnect.dll missing.\n"
    "Expected at:\n"
    "  ${_primary}\n"
    "or:\n"
    "  ${_secondary}\n"
    "Disable with -DRAWRXD_EXPECT_INTERCONNECT=OFF if CPU fallback is acceptable."
)
