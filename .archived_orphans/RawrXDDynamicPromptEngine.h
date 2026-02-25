// ============================================================================
// RawrXDDynamicPromptEngine.h — Unreal Engine 5 Integration Header
// ============================================================================
// UObject-based wrapper exposing the RawrXD Dynamic Prompt Engine to
// Unreal Engine Blueprints, C++ gameplay code, and Editor scripts.
//
// Setup:
//   1. Place RawrXD_DynamicPromptEngine.dll in:
//        Binaries/Win64/  (or ThirdParty/RawrXD/Binaries/Win64/)
//   2. Add this .h + .cpp to your Source/YourModule/ directory
//   3. In YourModule.Build.cs:
//        PublicDependencyModuleNames.Add("Core");
//        PublicDependencyModuleNames.Add("CoreUObject");
//        PublicDependencyModuleNames.Add("Engine");
//        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "ThirdParty/RawrXD/include"));
//        // Runtime load of the DLL (see .cpp)
//
// Usage (C++):
//   URawrXDPromptEngine* Engine = URawrXDPromptEngine::Get();
//   FString Critic = Engine->BuildCritic(TEXT("analyze this code snippet"));
//   EPromptMode Mode = Engine->Classify(TEXT("sudo rm -rf /")).Mode;
//
// Usage (Blueprint):
//   Drag "RawrXD Prompt Engine" nodes from the palette.
//   All functions are BlueprintCallable.
//
// Compatibility: UE 5.0+, UE 5.4+, Windows 64-bit
// ============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RawrXDDynamicPromptEngine.generated.h"

// ============================================================================
// Enums — Must match ASM constants
// ============================================================================

UENUM(BlueprintType, Category = "RawrXD|PromptEngine")
enum class EPromptMode : uint8
{
    Generic     = 0     UMETA(DisplayName = "Generic"),
    Casual      = 1     UMETA(DisplayName = "Casual"),
    Code        = 2     UMETA(DisplayName = "Code Review"),
    Security    = 3     UMETA(DisplayName = "Security Audit"),
    Shell       = 4     UMETA(DisplayName = "Shell Analysis"),
    Enterprise  = 5     UMETA(DisplayName = "Enterprise"),
    Auto        = 255   UMETA(DisplayName = "Auto Detect")
};

UENUM(BlueprintType, Category = "RawrXD|PromptEngine")
enum class EPromptTemplateType : uint8
{
    Critic  = 0     UMETA(DisplayName = "Critic"),
    Auditor = 1     UMETA(DisplayName = "Auditor")
};

// ============================================================================
// Classification Result Struct (Blueprint exposed)
// ============================================================================

USTRUCT(BlueprintType, Category = "RawrXD|PromptEngine")
struct FPromptClassifyResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "RawrXD|PromptEngine")
    EPromptMode Mode = EPromptMode::Generic;

    UPROPERTY(BlueprintReadOnly, Category = "RawrXD|PromptEngine")
    int32 Score = 0;

    UPROPERTY(BlueprintReadOnly, Category = "RawrXD|PromptEngine")
    FString ModeName;
};

// ============================================================================
// Blueprint Function Library — Static, globally accessible
// ============================================================================

UCLASS(meta = (ScriptName = "RawrXDPromptEngine"))
class URawrXDPromptEngineLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ========================================================================
    // Core API — BlueprintCallable
    // ========================================================================

    /**
     * Classify input text and return the detected context mode + confidence score.
     * @param Text  Input text to classify.
     * @return Classification result.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Classify Context"))
    static FPromptClassifyResult Classify(const FString& Text);

    /**
     * Generate a Critic prompt for the given context.
     * Auto-classifies the context and selects the appropriate template.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Build Critic Prompt"))
    static FString BuildCritic(const FString& Context);

    /**
     * Generate an Auditor prompt for the given context.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Build Auditor Prompt"))
    static FString BuildAuditor(const FString& Context);

    /**
     * Interpolate {{CONTEXT}} markers in a template with the given context.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Interpolate Template"))
    static FString Interpolate(const FString& Template, const FString& Context);

    /**
     * Get raw template string for a mode and type.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Get Template"))
    static FString GetTemplate(EPromptMode Mode, EPromptTemplateType Type);

    /**
     * Force classification to a specific mode.
     * Use EPromptMode::Auto to return to auto-detection.
     * @return Previous force mode.
     */
    UFUNCTION(BlueprintCallable, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Force Classification Mode"))
    static EPromptMode ForceMode(EPromptMode Mode);

    /**
     * Get the human-readable name for a mode.
     */
    UFUNCTION(BlueprintPure, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Get Mode Name"))
    static FString GetModeName(EPromptMode Mode);

    /**
     * Get the DLL version string.
     */
    UFUNCTION(BlueprintPure, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Get Engine Version"))
    static FString GetVersionString();

    /**
     * Quick check: Is the native DLL loaded and functional?
     */
    UFUNCTION(BlueprintPure, Category = "RawrXD|PromptEngine",
              meta = (DisplayName = "Is Engine Available"))
    static bool IsAvailable();
};

// ============================================================================
// Subsystem-style Singleton (for C++ callers who prefer UOBJECT lifecycle)
// ============================================================================

UCLASS(BlueprintType, Category = "RawrXD|PromptEngine")
class URawrXDPromptEngine : public UObject
{
    GENERATED_BODY()

public:

    /** Get the singleton instance. Creates on first call. */
    static URawrXDPromptEngine* Get();

    /** Check if the native DLL is loaded. */
    UFUNCTION(BlueprintPure, Category = "RawrXD")
    bool IsNativeLoaded() const { return bNativeLoaded; }

    /** Initialize — loads the DLL. Called automatically by Get(). */
    void Initialize();

    /** Shutdown — releases the DLL handle. */
    void Shutdown();

private:
    void* DllHandle = nullptr;
    bool  bNativeLoaded = false;

    static URawrXDPromptEngine* Singleton;
};
