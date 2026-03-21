// =============================================================================
// rawrxd/sovereign_target_manifest.hpp
// Neutral build-target description for cross-platform pipelines (CMake / clang).
// Does not encode syscalls, signing bypasses, or host "lock" scripts.
// =============================================================================

#pragma once

#include <cstdint>
#include <string>

namespace rawrxd::sovereign
{

enum class TargetOs : std::uint8_t
{
    Windows,
    Linux,
    MacOS,
    Android,
    IOS,
};

enum class TargetArch : std::uint8_t
{
    X86_64,
    Arm64,
};

enum class ObjectFormat : std::uint8_t
{
    Pe,
    Elf,
    MachO,
};

/// Which linker/toolchain is expected to finalize the image (informational).
enum class LinkerKind : std::uint8_t
{
    Msvc,
    Lld,
    Ld64,
    Ndk,
    Other,
};

enum class RuntimeModel : std::uint8_t
{
    /// Normal platform APIs (Win32, POSIX, Darwin).
    NativeApis,
    LibC,
    /// Cocoa/UIKit, Android Java/Kotlin surface, etc.
    FrameworkApis,
};

/// High-level manifest for "build this target the normal way."
struct TargetManifest
{
    TargetOs os = TargetOs::Windows;
    TargetArch arch = TargetArch::X86_64;
    ObjectFormat objectFormat = ObjectFormat::Pe;
    LinkerKind linker = LinkerKind::Msvc;
    RuntimeModel runtime = RuntimeModel::NativeApis;
    /// Optional triple string for CMake/tooling, e.g. "aarch64-linux-android".
    std::string llvmTriple{};
};

}  // namespace rawrxd::sovereign
