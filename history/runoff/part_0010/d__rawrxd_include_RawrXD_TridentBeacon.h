// ============================================================================
// RawrXD_TridentBeacon.h — COM Trident Host & Beacon Protocol C++ Bridge
// ============================================================================
//
// MASM64 IE/Trident WebBrowser COM host with:
//   - window.external.BeaconInvoke() JS→native bridge
//   - Async Ollama streaming integration
//   - Runtime JS injection (window.RawrXD namespace)
//   - GPU acceleration registry setup (via GPU companion module)
//
// Binary: RawrXD_TridentBeacon.asm + RawrXD_TridentBeacon_GPU.asm
// Dependencies: ole32.lib oleaut32.lib uuid.lib shlwapi.lib wininet.lib
//               opengl32.lib gdi32.lib advapi32.lib (GPU module)
//
// Pattern:  RAX=0 success, non-zero failure
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>

// ============================================================================
// Beacon Opcodes (match ASM constants in RawrXD_TridentBeacon.asm)
// ============================================================================
enum class BeaconOpcode : uint32_t {
    InjectJS        = 0x0001,   // Execute JavaScript in Trident context
    OllamaRequest   = 0x0002,   // Send async request to Ollama
    SetHTML         = 0x0003,   // Load HTML content
    CSSInject       = 0x0004,   // Inject CSS stylesheet
    GetState        = 0x0005,   // Return JSON state snapshot
};

// GPU Creation flags (match ASM TRIDENT_ENABLE_*)
constexpr uint32_t TRIDENT_ENABLE_GPU    = 0x0001;
constexpr uint32_t TRIDENT_ENABLE_WEBGL  = 0x0002;
constexpr uint32_t TRIDENT_ENABLE_ALL    = 0x0003;

// ============================================================================
// Stream callback type
// Called from background thread when Ollama sends a chunk
// ============================================================================
typedef void (*RawrXD_StreamCallback)(const char* chunk, uint32_t len);

// ============================================================================
// C-Linkage Declarations — TridentBeacon core (RawrXD_TridentBeacon.asm)
// ============================================================================
#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// TridentBeacon_Create
//   Instantiate COM Trident host, embed IWebBrowser2 in hWnd.
//   hWnd: parent window handle
//   x, y, w, h: placement rectangle
//   Returns: 0 on success, -1 on failure
// ---------------------------------------------------------------------------
int64_t TridentBeacon_Create(HWND hWnd, int32_t x, int32_t y,
                              int32_t w, int32_t h);

// ---------------------------------------------------------------------------
// TridentBeacon_Destroy
//   Release COM objects, close browser, free host struct.
//   Returns: 0
// ---------------------------------------------------------------------------
int64_t TridentBeacon_Destroy(void);

// ---------------------------------------------------------------------------
// TridentBeacon_Navigate
//   Navigate the embedded browser to a URL.
//   url: ANSI URL string
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t TridentBeacon_Navigate(const char* url);

// ---------------------------------------------------------------------------
// TridentBeacon_InjectJS
//   Execute JavaScript code in the current document context.
//   js: ANSI JavaScript code string
//   Returns: 0 on success, -1 if document not ready
// ---------------------------------------------------------------------------
int64_t TridentBeacon_InjectJS(const char* js);

// ---------------------------------------------------------------------------
// TridentBeacon_LoadHTML
//   Load raw HTML content (about:blank + document.write pattern).
//   html: ANSI HTML string
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t TridentBeacon_LoadHTML(const char* html);

// ---------------------------------------------------------------------------
// TridentBeacon_IsOllamaRunning
//   Check if the watchdog thread detects Ollama at 127.0.0.1:11434.
//   Returns: 1 if running, 0 if not
// ---------------------------------------------------------------------------
int64_t TridentBeacon_IsOllamaRunning(void);

// ---------------------------------------------------------------------------
// TridentBeacon_SetStreamCallback
//   Register a C++ function to receive Ollama streaming chunks.
//   fn: callback function pointer (called from background thread)
//   Returns: 0
// ---------------------------------------------------------------------------
int64_t TridentBeacon_SetStreamCallback(RawrXD_StreamCallback fn);

// ---------------------------------------------------------------------------
// TridentBeacon_GetHitCount
//   Return total number of Beacon protocol invocations.
//   Returns: hit count
// ---------------------------------------------------------------------------
int64_t TridentBeacon_GetHitCount(void);

// ---------------------------------------------------------------------------
// TridentBeacon_InjectBeaconRuntime
//   Inject the window.RawrXD namespace JS into the current document.
//   Must be called after document is loaded (e.g., on DocumentComplete).
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t TridentBeacon_InjectBeaconRuntime(void);

// ============================================================================
// C-Linkage Declarations — GPU Module (RawrXD_TridentBeacon_GPU.asm)
// ============================================================================

// ---------------------------------------------------------------------------
// Trident_EnableGPU
//   Write IE Feature Control registry keys for current process:
//     FEATURE_BROWSER_EMULATION        = 11001 (IE11 Edge)
//     FEATURE_GPU_RENDERING            = 1
//     FEATURE_WEBOC_ENABLE_GPU_RENDERING = 1
//     FEATURE_IVIEWOBJECTDRAW_DMLT9_WITH_GDI = 0
//     FEATURE_ENABLE_CORS              = 1
//     FEATURE_DISABLE_SOFTWARE_RENDERING_LIST = 1
//     FEATURE_BROWSER_EMULATION_EXTENDED = 11001
//     FEATURE_MAXCONNECTIONSPERSERVER   = 10
//     FEATURE_MAXCONNECTIONSPER1_0SERVER = 10
//   Returns: 0 on success, N = number of failed keys
// ---------------------------------------------------------------------------
int64_t Trident_EnableGPU(void);

// ---------------------------------------------------------------------------
// Trident_DisableGPU
//   Remove all Feature Control registry values for current process.
//   Returns: 0
// ---------------------------------------------------------------------------
int64_t Trident_DisableGPU(void);

// ---------------------------------------------------------------------------
// Trident_GetEmulationMode
//   Returns: current IE emulation mode (e.g. 11001, or 0 if not set)
// ---------------------------------------------------------------------------
int64_t Trident_GetEmulationMode(void);

// ---------------------------------------------------------------------------
// WebGL_CreateContext
//   Create an offscreen OpenGL rendering context with FBO.
//   width, height: render target dimensions
//   Returns: context ID (>0) on success, 0 on failure
// ---------------------------------------------------------------------------
uint64_t WebGL_CreateContext(uint32_t width, uint32_t height);

// ---------------------------------------------------------------------------
// WebGL_DestroyContext
//   Release GL context, FBO, renderbuffers, and hidden window.
//   ctxId: context ID returned by WebGL_CreateContext
//   Returns: 0 on success, -1 if context not found
// ---------------------------------------------------------------------------
int64_t WebGL_DestroyContext(uint64_t ctxId);

// ---------------------------------------------------------------------------
// WebGL_ExecuteCommand
//   Process a WebGL opcode on the given context.
//   ctxId:   context ID
//   opcode:  WGLCMD_* constant (see ASM header)
//   args:    pointer to packed argument buffer (layout varies by opcode)
//   argLen:  byte length of args
//   Returns: GL result value or 0 (opcode-dependent)
// ---------------------------------------------------------------------------
int64_t WebGL_ExecuteCommand(uint64_t ctxId, uint32_t opcode,
                              const void* args, uint32_t argLen);

// ---------------------------------------------------------------------------
// WebGL_CompositeToTrident
//   Flush and swap the GL context, signaling Trident to re-composite.
//   Called automatically after draw calls; can be called manually.
//   Returns: 0
// ---------------------------------------------------------------------------
int64_t WebGL_CompositeToTrident(void* ctxPtr);

// ---------------------------------------------------------------------------
// WebGL_GetContextCount
//   Returns: number of active WebGL contexts
// ---------------------------------------------------------------------------
int64_t WebGL_GetContextCount(void);

// ---------------------------------------------------------------------------
// Trident_CreateGPU
//   Combined initialization: set GPU registry keys + prepare for WebGL.
//   Must be called BEFORE TridentBeacon_Create (registry reads at CoCreate).
//   hWnd:  parent window
//   flags: TRIDENT_ENABLE_GPU | TRIDENT_ENABLE_WEBGL
//   Returns: 0 on success
// ---------------------------------------------------------------------------
int64_t Trident_CreateGPU(HWND hWnd, uint32_t flags);

#ifdef __cplusplus
}
#endif

// ============================================================================
// WebGL Command Opcodes (C++ constants matching ASM)
// ============================================================================
#ifdef __cplusplus

namespace RawrXD::WebGL {
    constexpr uint32_t CMD_VIEWPORT         = 0x0102;
    constexpr uint32_t CMD_CLEAR            = 0x0103;
    constexpr uint32_t CMD_CLEAR_COLOR      = 0x0104;
    constexpr uint32_t CMD_GEN_BUFFERS      = 0x0110;
    constexpr uint32_t CMD_BIND_BUFFER      = 0x0111;
    constexpr uint32_t CMD_BUFFER_DATA      = 0x0112;
    constexpr uint32_t CMD_GEN_TEXTURES     = 0x0120;
    constexpr uint32_t CMD_BIND_TEXTURE     = 0x0121;
    constexpr uint32_t CMD_TEX_IMAGE_2D     = 0x0122;
    constexpr uint32_t CMD_TEX_PARAMETER    = 0x0123;
    constexpr uint32_t CMD_CREATE_SHADER    = 0x0200;
    constexpr uint32_t CMD_SHADER_SOURCE    = 0x0201;
    constexpr uint32_t CMD_COMPILE_SHADER   = 0x0202;
    constexpr uint32_t CMD_CREATE_PROGRAM   = 0x0210;
    constexpr uint32_t CMD_ATTACH_SHADER    = 0x0211;
    constexpr uint32_t CMD_LINK_PROGRAM     = 0x0212;
    constexpr uint32_t CMD_USE_PROGRAM      = 0x0213;
    constexpr uint32_t CMD_GET_ATTRIB_LOC   = 0x0220;
    constexpr uint32_t CMD_GET_UNIFORM_LOC  = 0x0221;
    constexpr uint32_t CMD_DRAW_ARRAYS      = 0x0300;
    constexpr uint32_t CMD_DRAW_ELEMENTS    = 0x0301;
    constexpr uint32_t CMD_FLUSH            = 0x0302;
    constexpr uint32_t CMD_ENABLE           = 0x0310;
    constexpr uint32_t CMD_DISABLE          = 0x0311;
    constexpr uint32_t CMD_BLEND_FUNC       = 0x0312;

    // Argument packing helpers
    struct ClearColorArgs { float r, g, b, a; };
    struct ViewportArgs   { int32_t x, y, w, h; };
    struct DrawArrayArgs  { uint32_t mode; int32_t first, count; };
    struct BindBufferArgs { uint32_t target, buffer; };
}

// ============================================================================
// C++ RAII Wrapper for TridentBeacon
// ============================================================================
class RawrXDTridentHost {
public:
    explicit RawrXDTridentHost(HWND parent, int32_t x, int32_t y,
                                int32_t w, int32_t h,
                                uint32_t flags = TRIDENT_ENABLE_ALL)
        : m_created(false)
    {
        // Must set registry BEFORE CoCreateInstance
        Trident_CreateGPU(parent, flags);
        if (TridentBeacon_Create(parent, x, y, w, h) == 0) {
            m_created = true;
        }
    }

    ~RawrXDTridentHost() {
        if (m_created) {
            TridentBeacon_Destroy();
            Trident_DisableGPU();
        }
    }

    RawrXDTridentHost(const RawrXDTridentHost&) = delete;
    RawrXDTridentHost& operator=(const RawrXDTridentHost&) = delete;

    bool isCreated() const { return m_created; }

    void navigate(const char* url) { TridentBeacon_Navigate(url); }
    void injectJS(const char* js)  { TridentBeacon_InjectJS(js); }
    void loadHTML(const char* html) { TridentBeacon_LoadHTML(html); }
    void injectBeaconRuntime()     { TridentBeacon_InjectBeaconRuntime(); }

    bool isOllamaRunning() const { return TridentBeacon_IsOllamaRunning() != 0; }
    int64_t hitCount() const     { return TridentBeacon_GetHitCount(); }

    void setStreamCallback(RawrXD_StreamCallback fn) {
        TridentBeacon_SetStreamCallback(fn);
    }

private:
    bool m_created;
};

#endif // __cplusplus
