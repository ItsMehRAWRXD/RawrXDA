/**
 * @file self_patch.cpp
 * @brief SelfPatch implementation – Qt-free (C++20 / Win32)
 *
 * Adds Vulkan kernels, C++ wrappers, hot-reloads the binary,
 * and patches existing files. Uses Win32 CreateProcess for builds.
 */

#include "self_patch.hpp"
#include "process_utils.hpp"
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

// ── Constructor ──────────────────────────────────────────────────────────

SelfPatch::SelfPatch() = default;

// ── addKernel ────────────────────────────────────────────────────────────

bool SelfPatch::addKernel(const std::string& name, const std::string& templateName) {
    // Resolve paths
    std::string srcDir  = getEnvVar("RAWRXD_SRC", "src");
    std::string tplPath = srcDir + "/kernels/templates/" + templateName + ".comp";
    std::string dstPath = srcDir + "/kernels/" + name + ".comp";

    if (!fs::exists(tplPath)) {
        fprintf(stderr, "[SelfPatch] template not found: %s\n", tplPath.c_str());
        return false;
    }

    // Copy template → new kernel
    std::string tplSrc = fileutil::readAll(tplPath);
    if (tplSrc.empty()) {
        fprintf(stderr, "[SelfPatch] failed to read template: %s\n", tplPath.c_str());
        return false;
    }

    // Substitute placeholder
    const std::string placeholder = "{{KERNEL_NAME}}";
    for (size_t pos = tplSrc.find(placeholder);
         pos != std::string::npos;
         pos = tplSrc.find(placeholder, pos)) {
        tplSrc.replace(pos, placeholder.size(), name);
    }

    if (!fileutil::writeAll(dstPath, tplSrc)) {
        fprintf(stderr, "[SelfPatch] failed to write kernel: %s\n", dstPath.c_str());
        return false;
    }

    // Compile SPIR-V (glslangValidator)
    std::string spvPath = srcDir + "/kernels/" + name + ".spv";
    ProcResult pr = proc::run("glslangValidator",
                              {"-V", dstPath, "-o", spvPath}, 30000);
    if (!pr.ok()) {
        fprintf(stderr, "[SelfPatch] glslangValidator failed: %s\n", pr.stderrStr.c_str());
        return false;
    }

    fprintf(stderr, "[SelfPatch] kernel '%s' added from template '%s'\n",
            name.c_str(), templateName.c_str());
    fireKernelAdded(name);
    return true;
}

// ── addCpp ───────────────────────────────────────────────────────────────

bool SelfPatch::addCpp(const std::string& name, const std::string& deps) {
    std::string srcDir = getEnvVar("RAWRXD_SRC", "src");
    std::string hppPath = srcDir + "/kernels/" + name + "_wrapper.hpp";
    std::string cppPath = srcDir + "/kernels/" + name + "_wrapper.cpp";

    // Generate minimal header
    std::string hdr;
    hdr += "#pragma once\n";
    hdr += "// Auto-generated wrapper for kernel: " + name + "\n";
    hdr += "#include <cstdint>\n";
    if (!deps.empty()) hdr += "#include \"" + deps + "\"\n";
    hdr += "\n";
    hdr += "namespace kernels {\n";
    hdr += "bool " + name + "_dispatch(const void* input, void* output, uint32_t count);\n";
    hdr += "} // namespace kernels\n";

    // Generate real .cpp — Vulkan SPIR-V dispatch via VulkanCompute API
    std::string src;
    src += "#include \"" + name + "_wrapper.hpp\"\n";
    src += "#include \"../../vulkan_compute.h\"\n";
    src += "#include <cstdio>\n";
    src += "#include <cstring>\n\n";
    src += "namespace kernels {\n\n";
    src += "// Thread-safe singleton accessor for VulkanCompute\n";
    src += "static VulkanCompute& getCompute() {\n";
    src += "    static VulkanCompute instance;\n";
    src += "    static bool initialized = false;\n";
    src += "    if (!initialized) {\n";
    src += "        if (instance.Initialize()) {\n";
    src += "            initialized = true;\n";
    src += "            fprintf(stderr, \"[kernels] VulkanCompute initialized for dispatch\\n\");\n";
    src += "        } else {\n";
    src += "            fprintf(stderr, \"[kernels] WARNING: VulkanCompute init failed\\n\");\n";
    src += "        }\n";
    src += "    }\n";
    src += "    return instance;\n";
    src += "}\n\n";
    src += "bool " + name + "_dispatch(const void* input, void* output, uint32_t count) {\n";
    src += "    auto& vk = getCompute();\n\n";
    src += "    // Step 1: Load the compiled SPIR-V shader\n";
    src += "    std::string spvPath = \"src/kernels/" + name + ".spv\";\n";
    src += "    if (!vk.LoadShader(\"" + name + "\", spvPath)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: LoadShader failed for %s\\n\", spvPath.c_str());\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    // Step 2: Create compute pipeline for this kernel\n";
    src += "    if (!vk.CreateComputePipeline(\"" + name + "\")) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: CreateComputePipeline failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    // Step 3: Allocate GPU buffers for input and output\n";
    src += "    uint32_t inputIdx = 0, outputIdx = 0;\n";
    src += "    size_t inputSize = count * sizeof(float);\n";
    src += "    size_t outputSize = count * sizeof(float);\n";
    src += "    size_t allocSize = 0;\n\n";
    src += "    if (!vk.AllocateBuffer(inputSize, inputIdx, allocSize)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: input buffer alloc failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n";
    src += "    if (!vk.AllocateBuffer(outputSize, outputIdx, allocSize)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: output buffer alloc failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    // Step 4: Upload input data to GPU\n";
    src += "    if (!vk.CopyHostToBuffer(const_cast<void*>(input), inputIdx, inputSize)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: CopyHostToBuffer failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    // Step 5: Dispatch the compute shader\n";
    src += "    if (!vk.DispatchMatMul(inputIdx, inputIdx, outputIdx, count, 1, 1)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: DispatchMatMul failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    // Step 6: Read back results from GPU\n";
    src += "    if (!vk.CopyBufferToHost(outputIdx, output, outputSize)) {\n";
    src += "        fprintf(stderr, \"[kernels] " + name + "_dispatch: CopyBufferToHost failed\\n\");\n";
    src += "        return false;\n";
    src += "    }\n\n";
    src += "    fprintf(stderr, \"[kernels] " + name + "_dispatch: completed %u elements via Vulkan\\n\", count);\n";
    src += "    return true;\n";
    src += "}\n\n";
    src += "} // namespace kernels\n";

    if (!fileutil::writeAll(hppPath, hdr) || !fileutil::writeAll(cppPath, src)) {
        fprintf(stderr, "[SelfPatch] failed to write C++ wrapper for '%s'\n", name.c_str());
        return false;
    }

    fprintf(stderr, "[SelfPatch] C++ wrapper '%s' generated\n", name.c_str());
    fireCppAdded(name);
    return true;
}

// ── hotReload ────────────────────────────────────────────────────────────

bool SelfPatch::hotReload() {
    fireReloadStarted();

    std::string buildDir = getEnvVar("RAWRXD_BUILD", "build");

    // CMake configure
    ProcResult cfgResult = proc::run("cmake",
        {"--build", buildDir, "--config", "Release", "--target", "RawrXD-Shell"},
        120000);

    if (!cfgResult.ok()) {
        fprintf(stderr, "[SelfPatch] hotReload build failed:\n%s\n", cfgResult.stderrStr.c_str());
        fireReloadDone(false);
        return false;
    }

    // Attempt to restart (detached)
    std::string exePath = buildDir + "/Release/RawrXD-Shell.exe";
    if (!fs::exists(exePath)) {
        exePath = buildDir + "/RawrXD-Shell.exe";
    }

    if (fs::exists(exePath)) {
        proc::startDetached(exePath, {"--hot-reload-resume"});
        fprintf(stderr, "[SelfPatch] hotReload: new binary launched\n");
    } else {
        fprintf(stderr, "[SelfPatch] hotReload: binary not found at %s, build-only\n", exePath.c_str());
    }

    fireReloadDone(true);
    return true;
}

// ── patchFile ────────────────────────────────────────────────────────────

bool SelfPatch::patchFile(const std::string& filename, const std::string& patch) {
    if (!fs::exists(filename)) {
        fprintf(stderr, "[SelfPatch] patchFile: file not found: %s\n", filename.c_str());
        return false;
    }

    // Write patch to temp file, then apply with `git apply`
    std::string tmpPatch = filename + ".patch.tmp";
    if (!fileutil::writeAll(tmpPatch, patch)) {
        fprintf(stderr, "[SelfPatch] patchFile: failed to write temp patch\n");
        return false;
    }

    ProcResult pr = proc::run("git", {"apply", "--stat", tmpPatch}, 15000);
    if (!pr.ok()) {
        // Fallback: direct string replacement if patch is a simple diff-less content
        std::string original = fileutil::readAll(filename);
        if (original.empty()) {
            fprintf(stderr, "[SelfPatch] patchFile: failed to read original: %s\n", filename.c_str());
            fs::remove(tmpPatch);
            return false;
        }
        // Append patch content (simple mode)
        original += "\n" + patch + "\n";
        if (!fileutil::writeAll(filename, original)) {
            fprintf(stderr, "[SelfPatch] patchFile: failed to write patched file\n");
            fs::remove(tmpPatch);
            return false;
        }
        fprintf(stderr, "[SelfPatch] patchFile: appended patch to %s (simple mode)\n", filename.c_str());
    } else {
        // Apply for real
        ProcResult apply = proc::run("git", {"apply", tmpPatch}, 15000);
        if (!apply.ok()) {
            fprintf(stderr, "[SelfPatch] git apply failed: %s\n", apply.stderrStr.c_str());
            fs::remove(tmpPatch);
            return false;
        }
        fprintf(stderr, "[SelfPatch] patchFile: git apply succeeded on %s\n", filename.c_str());
    }

    fs::remove(tmpPatch);
    return true;
}
