# =============================================================================
# cmake/FindDirectX.cmake - Find DirectX components
# =============================================================================

# Find DirectX
find_path(DIRECTX_INCLUDE_DIR
    NAMES d3d11.h
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
        "C:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
        "D:/Program Files (x86)/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um"
    DOC "DirectX Include Directory"
)

find_library(DIRECTX_D3D11
    NAMES d3d11
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "Direct3D 11 Library"
)

find_library(DIRECTX_D3D12
    NAMES d3d12
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "Direct3D 12 Library"
)

find_library(DIRECTX_DXGI
    NAMES dxgi
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "DXGI Library"
)

find_library(DIRECTX_D2D1
    NAMES d2d1
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "Direct2D Library"
)

find_library(DIRECTX_DWRITE
    NAMES dwrite
    PATHS
        "$ENV{ProgramFiles(x86)}/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "C:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
        "D:/Program Files (x86)/Windows Kits/10/Lib/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/x64"
    DOC "DirectWrite Library"
)

# Create imported target
if(DIRECTX_INCLUDE_DIR)
    add_library(DirectX::DirectX INTERFACE IMPORTED)
    
    target_include_directories(DirectX::DirectX INTERFACE
        "${DIRECTX_INCLUDE_DIR}"
    )
    
    target_link_libraries(DirectX::DirectX INTERFACE
        "${DIRECTX_D3D11}"
        "${DIRECTX_D3D12}"
        "${DIRECTX_DXGI}"
        "${DIRECTX_D2D1}"
        "${DIRECTX_DWRITE}"
    )
    
    message(STATUS "DirectX found: ${DIRECTX_INCLUDE_DIR}")
else()
    message(STATUS "DirectX not found - some features may be unavailable")
endif()
