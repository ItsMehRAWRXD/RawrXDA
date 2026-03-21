# Build host vs target (Windows, Linux, macOS, Android, iOS)

This document separates **where you compile** (host) from **what binary format you emit** (target). RawrXD’s **Win32 IDE** is primarily developed on **Windows**; **CMake** presets and portable headers support building **parts of the tree** on other hosts for CI, tools, and lab code.

---

## Terms

| Term | Meaning |
|------|---------|
| **Host** | OS/arch running CMake + compiler (e.g. Windows x64, Linux arm64, macOS). |
| **Target** | OS/arch of the artifact (PE32+, ELF64, Mach-O, Android `.so`, iOS app). |

---

## Host OS: building this repository

| Host | Typical use | Notes |
|------|-------------|--------|
| **Windows** | Primary IDE, MSVC, MASM, Win32 app | Root `CMakeLists.txt` includes Windows SDK / MSVC path fixes when not in a VS Dev Prompt. |
| **Linux** | CI, Ninja, GCC/Clang for portable tests | Use **`cmake --preset linux-ninja-release`** (or plain CMake with your compiler). |
| **macOS** | Clang, Ninja, Xcode optional | Use **`cmake --preset darwin-ninja-release`** for a dedicated build dir name; or plain CMake. |
| **Android** | NDK cross-build | Set **`ANDROID_NDK`**, then configure with **`CMAKE_TOOLCHAIN_FILE`** = `$ANDROID_NDK/build/cmake/android.toolchain.cmake` (see preset **`android-ndk-release`**). |
| **iOS** | Xcode + CMake | Use **Xcode** or **Ninja** with **`CMAKE_SYSTEM_NAME=iOS`**; set **`CMAKE_OSX_SYSROOT`** to an iPhone **SDK** or **simulator** SDK as needed. Preset **`ios-xcode-release`** is a starting point — many targets remain Win32-only; expect subset builds. |

**Honesty:** The full **RawrXD Win32 IDE** target is **not** expected to link on Linux/macOS without substantial Win32-specific code gated out. Portable pieces include **headers**, **tests** that avoid Win32, and **toolchain/lab** C code.

---

## CMake presets (host-oriented)

See root **`CMakePresets.json`**:

| Preset | Intent |
|--------|--------|
| `ninja-release` | Default **host** build (folder `build/cmake-preset-ninja-release`). |
| `linux-ninja-release` | Linux/WSL explicit. |
| `darwin-ninja-release` | macOS explicit. |
| `android-ndk-release` | Cross-compile for Android when **`ANDROID_NDK`** is set. |

Add more presets locally if your SDK paths differ; presets are **convenience**, not a guarantee every target builds in CI.

---

## Target OS: what the “micro-linker” / SOM lab emits

| Target | In-repo lab / notes |
|--------|---------------------|
| **Windows PE** | `toolchain/from_scratch/phase2_linker/`, `include/rawrxd/sovereign_emit_formats.hpp` (`composePe64MinimalImage`), MASM PE experiments. |
| **ELF64 / Mach-O** | Tri-format **`composeElf64MinimalImage`**, **`composeMacho64MinimalImage`** — lab synthesis, not a full linker. |
| **Android / iOS** | Use platform toolchains; **SOM minimal** structs in **`include/rawrxd/sovereign_som_minimal.h`** are format-agnostic — the **emitter** chooses PE vs ELF vs Mach-O. |

---

## Related

- **`docs/SOVEREIGN_PRODUCTION_SCOPE_AND_ROADMAP.md`** — production Windows builder = **MSVC pipeline** (**§7**).
- **`docs/SOVEREIGN_TOOLCHAIN_LAB_ARCHITECTURE.md`** — CIR/SOM lab; atomic “opcodes + fixups” floor.
- **`docs/UNIVERSAL_PLATFORM_GAP_MATRIX.md`** — platform × feature honesty.
