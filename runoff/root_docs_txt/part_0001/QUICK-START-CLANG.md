# ⚡ Super Quick Start (You Have Everything!)

You have VS2022 and Clang. You're ready to build right now.

## Just Do This:

```powershell
cd "c:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader"
.\build.ps1
```

**That's it.** Build will take 3-5 minutes.

## After Build:

```powershell
.\build\bin\Release\RawrXD-ModelLoader.exe
```

## Verify It Works:

```powershell
# In another PowerShell window
curl http://localhost:11434/api/tags
```

---

## 🎯 Options (If Needed)

```powershell
# Use MSVC instead
.\build.ps1 -UseClang $false

# Debug build
.\build.ps1 -Configuration Debug

# Clean rebuild
.\build.ps1 -CleanBuild

# Skip shaders
.\build.ps1 -SkipShaderCompile
```

---

## ✅ Checklist Before Running

- [ ] Vulkan SDK installed (set `$env:VULKAN_SDK`)
- [ ] Clang installed (`clang-cl --version` should work)
- [ ] CMake 3.20+ installed
- [ ] Ninja installed (for Clang builds)

---

## 🚀 Run Build

```
.\build.ps1
```

Done!
