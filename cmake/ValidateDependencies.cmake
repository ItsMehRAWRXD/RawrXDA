# =============================================================================
# cmake/ValidateDependencies.cmake - Validate No Electron Dependencies
# =============================================================================
# This script is invoked as a CMake -P script after build to validate
# that no forbidden Electron/Node.js DLLs are linked.
#
# Usage:
#   cmake -DDEPS_FILE=<path> -DTARGET=<name> -P ValidateDependencies.cmake
# =============================================================================

# List of FORBIDDEN dependencies (Electron, Node.js, etc.)
set(FORBIDDEN_DLLS
    node.dll
    node.exe
    electron.dll
    electron.exe
    nodejs.dll
    v8.dll
    v8_libbase.dll
    v8_libplatform.dll
    libnode.dll
    libuv.dll
    icui18n.dll
    icuuc.dll
    icudt.dll
    zlib1.dll
    openssl.dll
    libcrypto.dll
    libssl.dll
    nghttp2.dll
    cares.dll
    libpthread.dll
    libgcc_s_seh.dll
    libstdc++6.dll
    libwinpthread.dll
    Qt5Core.dll
    Qt5Gui.dll
    Qt5Widgets.dll
    Qt6Core.dll
    Qt6Gui.dll
    Qt6Widgets.dll
    Qt5Network.dll
    Qt6Network.dll
    libqt5core.dll
    libqt6core.dll
)

# List of ALLOWED Windows system DLLs
set(ALLOWED_DLLS
    kernel32.dll
    ntdll.dll
    kernelbase.dll
    user32.dll
    gdi32.dll
    gdi32full.dll
    gdiplus.dll
    msimg32.dll
    opengl32.dll
    glu32.dll
    d3d11.dll
    d3d12.dll
    dxgi.dll
    d2d1.dll
    dwrite.dll
    dcomp.dll
    d3dcompiler47.dll
    advapi32.dll
    shell32.dll
    shlwapi.dll
    ole32.dll
    oleaut32.dll
    uuid.dll
    comctl32.dll
    comdlg32.dll
    winspool.dll
    winmm.dll
    imm32.dll
    version.dll
    imagehlp.dll
    psapi.dll
    msvcrt.dll
    msvcp140.dll
    vcruntime140.dll
    vcruntime140_1.dll
    concrt140.dll
    bcrypt.dll
    crypt32.dll
    cryptbase.dll
    wintrust.dll
    rsaenh.dll
    ncrypt.dll
    cryptsp.dll
    secur32.dll
    sspicli.dll
    netutils.dll
    srvcli.dll
    wkscli.dll
    netapi32.dll
    setupapi.dll
    cfgmgr32.dll
    devobj.dll
    winusb.dll
    hid.dll
    mscoree.dll
    mscoreei.dll
    clr.dll
    mscorwks.dll
    mscorlib.dll
    ucrtbase.dll
    ucrtbased.dll
    api-ms-win-core-*.dll
    api-ms-win-crt-*.dll
    vcomp140.dll
    vcomp140d.dll
    opencl.dll
    dxcore.dll
    directml.dll
    onnxruntime.dll
    onnxruntime_providers_shared.dll
    webview2loader.dll
    WebView2Loader.dll
)

function(validate_dll DLL_PATH)
    get_filename_component(DLL_NAME "${DLL_PATH}" NAME)
    string(TOLOWER "${DLL_NAME}" DLL_NAME_LOWER)

    # Check if forbidden
    foreach(FORBIDDEN ${FORBIDDEN_DLLS})
        string(TOLOWER "${FORBIDDEN}" FORBIDDEN_LOWER)
        if(DLL_NAME_LOWER STREQUAL FORBIDDEN_LOWER)
            message(FATAL_ERROR
                "FORBIDDEN DEPENDENCY DETECTED: ${DLL_NAME}\n"
                "This indicates Electron/Node.js/Qt contamination!\n"
                "RawrXD requires pure native C++ - no Electron/Qt allowed.\n"
                "Please remove any references to ${FORBIDDEN} from your build."
            )
        endif()
    endforeach()

    # Check if unknown (warn only, don't fail)
    set(IS_ALLOWED FALSE)
    foreach(ALLOWED ${ALLOWED_DLLS})
        string(TOLOWER "${ALLOWED}" ALLOWED_LOWER)
        if(DLL_NAME_LOWER STREQUAL ALLOWED_LOWER)
            set(IS_ALLOWED TRUE)
            break()
        endif()
    endforeach()

    # Check wildcard patterns for api-ms-win-*
    if(NOT IS_ALLOWED)
        string(FIND "${DLL_NAME_LOWER}" "api-ms-win-" API_POS)
        if(API_POS EQUAL 0)
            set(IS_ALLOWED TRUE)
        endif()
    endif()

    if(NOT IS_ALLOWED)
        message(WARNING
            "UNKNOWN DEPENDENCY: ${DLL_NAME}\n"
            "This DLL is not in the allowed list.\n"
            "If this is a legitimate dependency, add it to ALLOWED_DLLS."
        )
    endif()
endfunction()

# Main entry point when called with DEPS_FILE
if(DEFINED DEPS_FILE AND EXISTS "${DEPS_FILE}")
    file(READ "${DEPS_FILE}" DEPS_CONTENT)
    string(REGEX MATCHALL "[ \t]*[^ \t\r\n]+\\.dll" DLL_MATCHES "${DEPS_CONTENT}")

    set(FORBIDDEN_FOUND FALSE)
    foreach(DLL_MATCH ${DLL_MATCHES})
        string(STRIP "${DLL_MATCH}" DLL_NAME)
        validate_dll("${DLL_NAME}")
    endforeach()

    message(STATUS "Dependency validation passed for ${TARGET}")
endif()
