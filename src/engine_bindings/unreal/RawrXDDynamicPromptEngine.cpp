// ============================================================================
// RawrXDDynamicPromptEngine.cpp — Unreal Engine 5 Implementation
// ============================================================================
// Implements runtime DLL loading and Blueprint-callable wrappers for the
// RawrXD Dynamic Prompt Engine MASM64 kernel.
//
// DLL Loading Strategy:
//   Uses FPlatformProcess::GetDllHandle() for safe runtime loading.
//   Falls back gracefully — all functions return safe defaults when DLL
//   is missing, so the game/editor won't crash.
//
// Thread Safety:
//   All functions except ForceMode are thread-safe.
//   ForceMode modifies global state — call from game thread only.
// ============================================================================

#include "RawrXDDynamicPromptEngine.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

DEFINE_LOG_CATEGORY_STATIC(LogRawrXDPrompt, Log, All);

// ============================================================================
// Native Function Pointer Types
// ============================================================================

// Must match dynamic_prompt_engine.hpp signatures exactly
struct RawrXD_ClassifyResult_Native
{
    int32 Mode;
    int32 Score;
};

typedef int64  (*fn_AnalyzeContext)(const char* textPtr, size_t textLen);
typedef size_t (*fn_BuildCritic)(const char* ctx, size_t len, char* out, size_t outSize);
typedef size_t (*fn_BuildAuditor)(const char* ctx, size_t len, char* out, size_t outSize);
typedef size_t (*fn_Interpolate)(const char* tmpl, const char* ctx, char* out, size_t outSize);
typedef const char* (*fn_GetTemplate)(int32 mode, int32 type);
typedef int32  (*fn_ForceMode)(int32 mode);
typedef RawrXD_ClassifyResult_Native (*fn_ClassifyToStruct)(const char* textPtr, size_t textLen);
typedef uint32 (*fn_GetVersion)(void);
typedef const char* (*fn_GetModeName)(int32 mode);

// ============================================================================
// Function Pointer Storage
// ============================================================================

static fn_AnalyzeContext     g_pfnAnalyzeContext   = nullptr;
static fn_BuildCritic        g_pfnBuildCritic      = nullptr;
static fn_BuildAuditor       g_pfnBuildAuditor     = nullptr;
static fn_Interpolate        g_pfnInterpolate      = nullptr;
static fn_GetTemplate        g_pfnGetTemplate      = nullptr;
static fn_ForceMode          g_pfnForceMode        = nullptr;
static fn_ClassifyToStruct   g_pfnClassifyToStruct = nullptr;
static fn_GetVersion         g_pfnGetVersion       = nullptr;
static fn_GetModeName        g_pfnGetModeName      = nullptr;

static bool g_bDllLoaded = false;

// ============================================================================
// DLL Name
// ============================================================================

static const TCHAR* DLL_NAME = TEXT("RawrXD_DynamicPromptEngine");

// ============================================================================
// DLL Search Paths (in priority order)
// ============================================================================

static TArray<FString> GetDllSearchPaths()
{
    TArray<FString> Paths;

    // 1. Game binaries directory
    Paths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries/Win64")));

    // 2. ThirdParty directory
    Paths.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("ThirdParty/RawrXD/Binaries/Win64")));

    // 3. Plugin binaries (if distributed as a plugin)
    Paths.Add(FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("RawrXD/Binaries/Win64")));

    // 4. Engine ThirdParty (for engine-level integration)
    Paths.Add(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/RawrXD/Win64")));

    // 5. Executable directory (fallback)
    Paths.Add(FPlatformProcess::GetModulesDirectory());

    return Paths;
}

// ============================================================================
// Helper: Convert FString to UTF-8 for ASM kernel (which expects ANSI/UTF-8)
// ============================================================================

static TArray<char> ToUTF8(const FString& InString)
{
    // Unreal uses UTF-16 internally; convert to UTF-8 for the ASM kernel
    FTCHARToUTF8 Converter(*InString);
    int32 Len = Converter.Length();
    TArray<char> Result;
    Result.SetNumUninitialized(Len + 1);
    FMemory::Memcpy(Result.GetData(), Converter.Get(), Len);
    Result[Len] = '\0';
    return Result;
}

// ============================================================================
// Helper: Load a function from the DLL
// ============================================================================

template<typename T>
static bool LoadFunction(void* DllHandle, const char* FuncName, T& OutFunc)
{
    OutFunc = (T)FPlatformProcess::GetDllExport(DllHandle, ANSI_TO_TCHAR(FuncName));
    if (!OutFunc)
    {
        UE_LOG(LogRawrXDPrompt, Warning,
            TEXT("[RawrXD] Failed to load function: %s"), ANSI_TO_TCHAR(FuncName));
        return false;
    }
    return true;
}

// ============================================================================
// URawrXDPromptEngine — Singleton Implementation
// ============================================================================

URawrXDPromptEngine* URawrXDPromptEngine::Singleton = nullptr;

URawrXDPromptEngine* URawrXDPromptEngine::Get()
{
    if (!Singleton)
    {
        Singleton = NewObject<URawrXDPromptEngine>();
        Singleton->AddToRoot(); // Prevent GC
        Singleton->Initialize();
    }
    return Singleton;
}

void URawrXDPromptEngine::Initialize()
{
    if (bNativeLoaded) return;

    // Search for the DLL in standard locations
    TArray<FString> SearchPaths = GetDllSearchPaths();

    for (const FString& SearchDir : SearchPaths)
    {
        FString FullPath = FPaths::Combine(SearchDir, FString(DLL_NAME) + TEXT(".dll"));

        if (FPaths::FileExists(FullPath))
        {
            // Push DLL directory for dependent DLL resolution
            FPlatformProcess::PushDllDirectory(*SearchDir);
            DllHandle = FPlatformProcess::GetDllHandle(*FullPath);
            FPlatformProcess::PopDllDirectory(*SearchDir);

            if (DllHandle)
            {
                UE_LOG(LogRawrXDPrompt, Log,
                    TEXT("[RawrXD] Loaded DynamicPromptEngine from: %s"), *FullPath);
                break;
            }
        }
    }

    if (!DllHandle)
    {
        // Try default search (system PATH)
        DllHandle = FPlatformProcess::GetDllHandle(DLL_NAME);
    }

    if (!DllHandle)
    {
        UE_LOG(LogRawrXDPrompt, Error,
            TEXT("[RawrXD] Failed to load %s.dll — prompt engine unavailable."), DLL_NAME);
        return;
    }

    // Load all function pointers
    bool bAllLoaded = true;
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_AnalyzeContext",   g_pfnAnalyzeContext);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_BuildCritic",      g_pfnBuildCritic);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_BuildAuditor",     g_pfnBuildAuditor);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_Interpolate",      g_pfnInterpolate);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_GetTemplate",      g_pfnGetTemplate);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_ForceMode",        g_pfnForceMode);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_ClassifyToStruct", g_pfnClassifyToStruct);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_GetVersion",       g_pfnGetVersion);
    bAllLoaded &= LoadFunction(DllHandle, "PromptGen_GetModeName",      g_pfnGetModeName);

    if (!bAllLoaded)
    {
        UE_LOG(LogRawrXDPrompt, Warning,
            TEXT("[RawrXD] Some functions failed to load — partial functionality available."));
    }

    bNativeLoaded = true;
    g_bDllLoaded = true;

    // Log version
    if (g_pfnGetVersion)
    {
        uint32 Ver = g_pfnGetVersion();
        UE_LOG(LogRawrXDPrompt, Log,
            TEXT("[RawrXD] DynamicPromptEngine v%d.%d.%d loaded."),
            (Ver >> 16) & 0xFF, (Ver >> 8) & 0xFF, Ver & 0xFF);
    }
}

void URawrXDPromptEngine::Shutdown()
{
    if (DllHandle)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }

    g_pfnAnalyzeContext   = nullptr;
    g_pfnBuildCritic      = nullptr;
    g_pfnBuildAuditor     = nullptr;
    g_pfnInterpolate      = nullptr;
    g_pfnGetTemplate      = nullptr;
    g_pfnForceMode        = nullptr;
    g_pfnClassifyToStruct = nullptr;
    g_pfnGetVersion       = nullptr;
    g_pfnGetModeName      = nullptr;

    bNativeLoaded = false;
    g_bDllLoaded = false;

    UE_LOG(LogRawrXDPrompt, Log, TEXT("[RawrXD] DynamicPromptEngine unloaded."));
}

// ============================================================================
// URawrXDPromptEngineLibrary — Blueprint Function Library Implementation
// ============================================================================

// Buffer size for generated prompts
static constexpr int32 PROMPT_BUFFER_SIZE = 8192;

FPromptClassifyResult URawrXDPromptEngineLibrary::Classify(const FString& Text)
{
    FPromptClassifyResult Result;
    Result.Mode = EPromptMode::Generic;
    Result.Score = 0;
    Result.ModeName = TEXT("GENERIC");

    // Ensure DLL is loaded
    URawrXDPromptEngine::Get();

    if (!g_pfnClassifyToStruct || Text.IsEmpty()) return Result;

    TArray<char> Utf8 = ToUTF8(Text);
    RawrXD_ClassifyResult_Native Native = g_pfnClassifyToStruct(Utf8.GetData(), Utf8.Num() - 1);

    // Map int32 to enum (clamping for safety)
    int32 ModeVal = FMath::Clamp(Native.Mode, 0, 5);
    Result.Mode = static_cast<EPromptMode>(ModeVal);
    Result.Score = Native.Score;

    // Get mode name
    if (g_pfnGetModeName)
    {
        const char* Name = g_pfnGetModeName(ModeVal);
        Result.ModeName = Name ? FString(ANSI_TO_TCHAR(Name)) : TEXT("UNKNOWN");
    }

    return Result;
}

FString URawrXDPromptEngineLibrary::BuildCritic(const FString& Context)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnBuildCritic || Context.IsEmpty()) return FString();

    TArray<char> Utf8 = ToUTF8(Context);
    TArray<char> OutBuf;
    OutBuf.SetNumZeroed(PROMPT_BUFFER_SIZE);

    size_t Written = g_pfnBuildCritic(
        Utf8.GetData(), Utf8.Num() - 1,
        OutBuf.GetData(), PROMPT_BUFFER_SIZE
    );

    return (Written > 0)
        ? FString(ANSI_TO_TCHAR(OutBuf.GetData()))
        : FString();
}

FString URawrXDPromptEngineLibrary::BuildAuditor(const FString& Context)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnBuildAuditor || Context.IsEmpty()) return FString();

    TArray<char> Utf8 = ToUTF8(Context);
    TArray<char> OutBuf;
    OutBuf.SetNumZeroed(PROMPT_BUFFER_SIZE);

    size_t Written = g_pfnBuildAuditor(
        Utf8.GetData(), Utf8.Num() - 1,
        OutBuf.GetData(), PROMPT_BUFFER_SIZE
    );

    return (Written > 0)
        ? FString(ANSI_TO_TCHAR(OutBuf.GetData()))
        : FString();
}

FString URawrXDPromptEngineLibrary::Interpolate(const FString& Template, const FString& Context)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnInterpolate || Template.IsEmpty()) return FString();

    TArray<char> TmplUtf8 = ToUTF8(Template);
    TArray<char> CtxUtf8  = ToUTF8(Context);
    TArray<char> OutBuf;
    OutBuf.SetNumZeroed(PROMPT_BUFFER_SIZE);

    size_t Written = g_pfnInterpolate(
        TmplUtf8.GetData(), CtxUtf8.GetData(),
        OutBuf.GetData(), PROMPT_BUFFER_SIZE
    );

    return (Written > 0)
        ? FString(ANSI_TO_TCHAR(OutBuf.GetData()))
        : FString();
}

FString URawrXDPromptEngineLibrary::GetTemplate(EPromptMode Mode, EPromptTemplateType Type)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnGetTemplate) return FString();

    int32 ModeInt = (Mode == EPromptMode::Auto) ? -1 : static_cast<int32>(Mode);
    int32 TypeInt = static_cast<int32>(Type);

    const char* Ptr = g_pfnGetTemplate(ModeInt, TypeInt);
    return Ptr ? FString(ANSI_TO_TCHAR(Ptr)) : FString();
}

EPromptMode URawrXDPromptEngineLibrary::ForceMode(EPromptMode Mode)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnForceMode) return EPromptMode::Auto;

    int32 ModeInt = (Mode == EPromptMode::Auto) ? -1 : static_cast<int32>(Mode);
    int32 Prev = g_pfnForceMode(ModeInt);

    if (Prev < 0 || Prev > 5) return EPromptMode::Auto;
    return static_cast<EPromptMode>(Prev);
}

FString URawrXDPromptEngineLibrary::GetModeName(EPromptMode Mode)
{
    URawrXDPromptEngine::Get();
    if (!g_pfnGetModeName) return TEXT("UNKNOWN");

    int32 ModeInt = (Mode == EPromptMode::Auto) ? -1 : static_cast<int32>(Mode);
    const char* Name = g_pfnGetModeName(ModeInt);
    return Name ? FString(ANSI_TO_TCHAR(Name)) : TEXT("UNKNOWN");
}

FString URawrXDPromptEngineLibrary::GetVersionString()
{
    URawrXDPromptEngine::Get();
    if (!g_pfnGetVersion) return TEXT("0.0.0");

    uint32 Ver = g_pfnGetVersion();
    return FString::Printf(TEXT("%d.%d.%d"),
        (Ver >> 16) & 0xFF, (Ver >> 8) & 0xFF, Ver & 0xFF);
}

bool URawrXDPromptEngineLibrary::IsAvailable()
{
    URawrXDPromptEngine::Get();
    return g_bDllLoaded && g_pfnAnalyzeContext != nullptr;
}
