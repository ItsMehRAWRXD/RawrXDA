# =============================================================================
# cmake/WindowsSDK.cmake - Find Windows SDK Components
# =============================================================================

# Find Windows SDK
find_path(WINDOWS_SDK_INCLUDE_DIR
    NAMES windows.h
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
        "$ENV{ProgramFiles}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
        "D:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
    DOC "Windows SDK Include Directory"
)

find_path(WINDOWS_SDK_SHARED_DIR
    NAMES windows.foundation.h
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/shared"
        "$ENV{ProgramFiles}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/shared"
        "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/shared"
        "D:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/shared"
    DOC "Windows SDK Shared Directory"
)

find_path(WINDOWS_SDK_UCRT_DIR
    NAMES corecrt.h
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt"
        "$ENV{ProgramFiles}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt"
        "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt"
        "D:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/ucrt"
    DOC "Windows SDK UCRT Directory"
)

# Find Windows SDK libraries
find_library(WINDOWS_SDK_KERNEL32
    NAMES kernel32
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "Windows SDK kernel32 Library"
)

find_library(WINDOWS_SDK_USER32
    NAMES user32
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "$ENV{ProgramFiles}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "Windows SDK user32 Library"
)

# Create imported target for Windows SDK
if(WINDOWS_SDK_INCLUDE_DIR AND WINDOWS_SDK_UCRT_DIR)
    add_library(WindowsSDK::SDK INTERFACE IMPORTED)
    
    target_include_directories(WindowsSDK::SDK INTERFACE
        "${WINDOWS_SDK_INCLUDE_DIR}"
        "${WINDOWS_SDK_SHARED_DIR}"
        "${WINDOWS_SDK_UCRT_DIR}"
    )
    
    target_compile_definitions(WindowsSDK::SDK INTERFACE
        USE_WINDOWS_SDK
        WINDOWS_FOUND
    )
    
    message(STATUS "Windows SDK found: ${WINDOWS_SDK_INCLUDE_DIR}")
else()
    message(STATUS "Windows SDK not found via find_path - relying on environment paths")
endif()
