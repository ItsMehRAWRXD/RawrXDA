// ============================================================================
// RawrXDDynamicPromptEngine.cs — Unity P/Invoke Wrapper
// ============================================================================
// Drop-in Unity integration for the RawrXD Dynamic Prompt Engine.
//
// Setup:
//   1. Place RawrXD_DynamicPromptEngine.dll in:
//        Assets/Plugins/x86_64/  (Windows 64-bit)
//   2. Add this .cs file anywhere under Assets/Scripts/
//   3. In Unity Editor, select the DLL → Inspector:
//        ✅ CPU: x86_64
//        ✅ OS: Windows
//        ✅ Plugin platform: Editor + Standalone
//
// Usage:
//   string prompt = RawrXDPromptEngine.BuildCritic("review this code");
//   var result    = RawrXDPromptEngine.Classify("yo what's up bro");
//   Debug.Log($"Mode: {result.Mode}, Score: {result.Score}");
//
// Thread Safety:
//   ForceMode is global state — do NOT call from multiple threads.
//   All other functions are thread-safe (read-only templates + spinlocked scratch).
//
// Compatibility: Unity 2020.3 LTS+, Unity 6+, IL2CPP + Mono
// ============================================================================

using System;
using System.Runtime.InteropServices;
using System.Text;
using UnityEngine;

/// <summary>
/// Native DLL binding for the RawrXD Dynamic Prompt Engine.
/// All P/Invoke declarations use CallingConvention.Cdecl to match the ASM kernel.
/// </summary>
public static class RawrXDPromptEngine
{
#if UNITY_EDITOR || UNITY_STANDALONE_WIN
    private const string DLL_NAME = "RawrXD_DynamicPromptEngine";
#elif UNITY_STANDALONE_LINUX
    private const string DLL_NAME = "libRawrXD_DynamicPromptEngine";
#elif UNITY_STANDALONE_OSX
    private const string DLL_NAME = "libRawrXD_DynamicPromptEngine";
#else
    private const string DLL_NAME = "RawrXD_DynamicPromptEngine";
#endif

    // ========================================================================
    // Constants — Must match ASM/C++ header
    // ========================================================================

    public const int CTX_MODE_GENERIC    = 0;
    public const int CTX_MODE_CASUAL     = 1;
    public const int CTX_MODE_CODE       = 2;
    public const int CTX_MODE_SECURITY   = 3;
    public const int CTX_MODE_SHELL      = 4;
    public const int CTX_MODE_ENTERPRISE = 5;
    public const int CTX_MODE_COUNT      = 6;
    public const int CTX_MODE_AUTO       = -1;

    public const int TEMPLATE_CRITIC     = 0;
    public const int TEMPLATE_AUDITOR    = 1;

    /// <summary>Maximum output buffer size for generated prompts.</summary>
    public const int MAX_PROMPT_SIZE     = 8192;

    // ========================================================================
    // Classification Mode Enum
    // ========================================================================

    /// <summary>Context classification mode returned by the prompt engine.</summary>
    public enum PromptMode
    {
        Generic    = CTX_MODE_GENERIC,
        Casual     = CTX_MODE_CASUAL,
        Code       = CTX_MODE_CODE,
        Security   = CTX_MODE_SECURITY,
        Shell      = CTX_MODE_SHELL,
        Enterprise = CTX_MODE_ENTERPRISE,
        Auto       = CTX_MODE_AUTO
    }

    /// <summary>Template type selector.</summary>
    public enum TemplateType
    {
        Critic  = TEMPLATE_CRITIC,
        Auditor = TEMPLATE_AUDITOR
    }

    // ========================================================================
    // Classification Result (mirrors RawrXD_ClassifyResult)
    // ========================================================================

    /// <summary>
    /// Result of context classification. Contains the detected mode and
    /// the confidence score (weighted keyword hit count).
    /// </summary>
    [StructLayout(LayoutKind.Sequential, Pack = 8)]
    public struct ClassifyResult
    {
        public int Mode;
        public int Score;

        /// <summary>Get the mode as the PromptMode enum.</summary>
        public PromptMode PromptMode =>
            (Mode >= 0 && Mode < CTX_MODE_COUNT)
                ? (PromptMode)Mode
                : PromptMode.Generic;

        public override string ToString() =>
            $"ClassifyResult(Mode={PromptMode}, Score={Score})";
    }

    // ========================================================================
    // Native P/Invoke Declarations
    // ========================================================================

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_ClassifyToStruct")]
    private static extern ClassifyResult Native_ClassifyToStruct(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string textPtr,
        UIntPtr textLen
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_BuildCritic")]
    private static extern UIntPtr Native_BuildCritic(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string contextPtr,
        UIntPtr contextLen,
        byte[] outBuf,
        UIntPtr outSize
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_BuildAuditor")]
    private static extern UIntPtr Native_BuildAuditor(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string contextPtr,
        UIntPtr contextLen,
        byte[] outBuf,
        UIntPtr outSize
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_Interpolate")]
    private static extern UIntPtr Native_Interpolate(
        [MarshalAs(UnmanagedType.LPUTF8Str)] string templatePtr,
        [MarshalAs(UnmanagedType.LPUTF8Str)] string contextPtr,
        byte[] outBuf,
        UIntPtr outSize
    );

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_GetTemplate")]
    private static extern IntPtr Native_GetTemplate(int mode, int type);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_ForceMode")]
    private static extern int Native_ForceMode(int mode);

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_GetVersion")]
    private static extern uint Native_GetVersion();

    [DllImport(DLL_NAME, CallingConvention = CallingConvention.Cdecl,
               EntryPoint = "PromptGen_GetModeName")]
    private static extern IntPtr Native_GetModeName(int mode);

    // ========================================================================
    // Public API — Managed Wrappers
    // ========================================================================

    /// <summary>
    /// Classify input text and return the detected context mode + score.
    /// </summary>
    /// <param name="text">The input text to classify.</param>
    /// <returns>Classification result with mode and confidence score.</returns>
    public static ClassifyResult Classify(string text)
    {
        if (string.IsNullOrEmpty(text))
            return new ClassifyResult { Mode = CTX_MODE_GENERIC, Score = 0 };

        return Native_ClassifyToStruct(text, (UIntPtr)Encoding.UTF8.GetByteCount(text));
    }

    /// <summary>
    /// Generate a Critic prompt for the given context text.
    /// Automatically classifies the context and selects the appropriate template.
    /// </summary>
    public static string BuildCritic(string context)
    {
        if (string.IsNullOrEmpty(context)) return string.Empty;

        byte[] buf = new byte[MAX_PROMPT_SIZE];
        UIntPtr written = Native_BuildCritic(
            context,
            (UIntPtr)Encoding.UTF8.GetByteCount(context),
            buf,
            (UIntPtr)buf.Length
        );

        int len = (int)(ulong)written;
        return (len > 0) ? Encoding.UTF8.GetString(buf, 0, len) : string.Empty;
    }

    /// <summary>
    /// Generate an Auditor prompt for the given context text.
    /// </summary>
    public static string BuildAuditor(string context)
    {
        if (string.IsNullOrEmpty(context)) return string.Empty;

        byte[] buf = new byte[MAX_PROMPT_SIZE];
        UIntPtr written = Native_BuildAuditor(
            context,
            (UIntPtr)Encoding.UTF8.GetByteCount(context),
            buf,
            (UIntPtr)buf.Length
        );

        int len = (int)(ulong)written;
        return (len > 0) ? Encoding.UTF8.GetString(buf, 0, len) : string.Empty;
    }

    /// <summary>
    /// Interpolate {{CONTEXT}} markers in an arbitrary template string.
    /// </summary>
    /// <param name="template">Template string containing {{CONTEXT}} markers.</param>
    /// <param name="context">Replacement text.</param>
    /// <returns>Interpolated string, or empty on overflow.</returns>
    public static string Interpolate(string template_, string context)
    {
        if (string.IsNullOrEmpty(template_)) return string.Empty;
        if (context == null) context = string.Empty;

        byte[] buf = new byte[MAX_PROMPT_SIZE];
        UIntPtr written = Native_Interpolate(
            template_,
            context,
            buf,
            (UIntPtr)buf.Length
        );

        int len = (int)(ulong)written;
        return (len > 0) ? Encoding.UTF8.GetString(buf, 0, len) : string.Empty;
    }

    /// <summary>
    /// Get the raw template string for a given mode and type.
    /// The returned string is from static memory — do not modify.
    /// </summary>
    public static string GetTemplate(PromptMode mode, TemplateType type)
    {
        IntPtr ptr = Native_GetTemplate((int)mode, (int)type);
        return (ptr != IntPtr.Zero) ? Marshal.PtrToStringAnsi(ptr) : null;
    }

    /// <summary>
    /// Force classification to a specific mode. Pass PromptMode.Auto (-1)
    /// to return to auto-detection.
    /// </summary>
    /// <returns>The previously set ForceMode value.</returns>
    public static PromptMode ForceMode(PromptMode mode)
    {
        int prev = Native_ForceMode((int)mode);
        return (PromptMode)prev;
    }

    /// <summary>
    /// Get the human-readable name for a mode (e.g., "CODE", "SHELL").
    /// </summary>
    public static string GetModeName(PromptMode mode)
    {
        IntPtr ptr = Native_GetModeName((int)mode);
        return (ptr != IntPtr.Zero) ? Marshal.PtrToStringAnsi(ptr) : "UNKNOWN";
    }

    /// <summary>
    /// Get the DLL version as a System.Version object.
    /// </summary>
    public static Version GetVersion()
    {
        uint packed = Native_GetVersion();
        int major = (int)((packed >> 16) & 0xFF);
        int minor = (int)((packed >> 8)  & 0xFF);
        int patch = (int)(packed & 0xFF);
        return new Version(major, minor, patch);
    }

    /// <summary>
    /// Convenience: Classify and return just the PromptMode enum.
    /// </summary>
    public static PromptMode DetectMode(string text)
    {
        return Classify(text).PromptMode;
    }

    // ========================================================================
    // Unity-Specific Helpers
    // ========================================================================

    /// <summary>
    /// Log classification result to Unity console with color-coded output.
    /// </summary>
    [System.Diagnostics.Conditional("UNITY_EDITOR")]
    public static void DebugClassify(string text)
    {
        var result = Classify(text);
        string modeName = GetModeName(result.PromptMode);
        Debug.Log($"[RawrXD-Prompt] <color=cyan>{modeName}</color> " +
                  $"(score: {result.Score}) — \"{TruncateForLog(text, 80)}\"");
    }

    private static string TruncateForLog(string s, int maxLen)
    {
        if (s == null) return "";
        return (s.Length <= maxLen) ? s : s.Substring(0, maxLen) + "...";
    }
}
