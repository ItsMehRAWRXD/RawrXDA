if(NOT DEFINED VERIFY_DIR)
    message(FATAL_ERROR "VERIFY_DIR not set")
endif()

if(NOT EXISTS "${VERIFY_DIR}")
    message(FATAL_ERROR "Verify directory does not exist: ${VERIFY_DIR}")
endif()

if(NOT DEFINED REQUIRED_DLLS)
    message(FATAL_ERROR "REQUIRED_DLLS not set")
endif()

set(missing "")
foreach(dll IN LISTS REQUIRED_DLLS)
    if(NOT EXISTS "${VERIFY_DIR}/${dll}")
        list(APPEND missing "${dll}")
    endif()
endforeach()

if(missing)
    string(REPLACE ";" ", " missing_list "${missing}")
    message(FATAL_ERROR "Missing Qt runtime DLLs in ${VERIFY_DIR}: ${missing_list}")
endif()

message(STATUS "Qt runtime verification passed in ${VERIFY_DIR}")
