// RawrXD_SelfPatch.hpp - Autonomous Code Generation and Patching
// Pure C++20 - No Qt Dependencies
// Ported from: self_patch.cpp (adapted for cl.exe/Win32)

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <windows.h>

namespace fs = std::filesystem;

namespace RawrXD {

class SelfPatch {
public:
    static bool AddKernel(const std::string& name, const std::string& templateName) {
        std::string tplPath = "kernels/" + templateName + ".comp";
        std::string outPath = "kernels/" + name + ".comp";

        try {
            if (fs::exists(outPath)) return true;
            if (!fs::exists("kernels")) fs::create_directories("kernels");
            fs::copy_file(tplPath, outPath);

            // Update build script (cl.exe doesn't compile shaders, but we might track it)
            updateBuildManifest(name + ".comp");
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool AddCpp(const std::string& name) {
        std::string hppPath = "src/gpu/" + name + ".hpp";
        std::string cppPath = "src/gpu/" + name + ".cpp";

        try {
            if (fs::exists(cppPath)) return true;
            if (!fs::exists("src/gpu")) fs::create_directories("src/gpu");

            std::ofstream hpp(hppPath);
            hpp << "#pragma once\n"
                << "#include <vector>\n"
                << "#include <cstddef>\n\n"
                << "class " << name << " {\n"
                << "public:\n"
                << "    static std::vector<float> wrap(const float* src, size_t n);\n"
                << "    static void initialize();\n"
                << "    static void cleanup();\n"
                << "};\n";
            hpp.close();

            std::ofstream cpp(cppPath);
            cpp << "#include \"" << name << ".hpp\"\n"
                << "#include <windows.h>\n"
                << "#include <iostream>\n"
                << "#include <immintrin.h>\n\n"
                << "void " << name << "::initialize() {\n"
                << "    std::cout << \"[" << name << "] Initializing GPU kernel wrapper\\n\";\n"
                << "    // Verify SIMD support for optimized tensor operations\n"
                << "    int cpuInfo[4] = {};\n"
                << "    __cpuid(cpuInfo, 1);\n"
                << "    bool hasAVX2 = (cpuInfo[2] & (1 << 28)) != 0;\n"
                << "    std::cout << \"[" << name << "] AVX2 support: \" << (hasAVX2 ? \"yes\" : \"no\") << \"\\n\";\n"
                << "}\n\n"
                << "void " << name << "::cleanup() {\n"
                << "    std::cout << \"[" << name << "] Cleaning up GPU kernel resources\\n\";\n"
                << "}\n\n"
                << "std::vector<float> " << name << "::wrap(const float* src, size_t n) {\n"
                << "    std::vector<float> res(n);\n"
                << "    if (!src || n == 0) return res;\n"
                << "    // SIMD-accelerated copy using AVX2 (256-bit = 8 floats per op)\n"
                << "    size_t simdEnd = (n / 8) * 8;\n"
                << "    for (size_t i = 0; i < simdEnd; i += 8) {\n"
                << "        __m256 v = _mm256_loadu_ps(src + i);\n"
                << "        _mm256_storeu_ps(res.data() + i, v);\n"
                << "    }\n"
                << "    // Scalar remainder\n"
                << "    for (size_t i = simdEnd; i < n; ++i) res[i] = src[i];\n"
                << "    return res;\n"
                << "}\n";
            cpp.close();

            updateBuildManifest(name + ".cpp");
            return true;
        } catch (...) {
            return false;
        }
    }

    static bool HotReload() {
        // Trigger the build_all.bat and restart
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;
        if (CreateProcessA(NULL, (char*)"cmd.exe /c build_all.bat", NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            // In a real app, we'd wait for exit and then relaunch ourself
            return true;
        }
        return false;
    }

private:
    static void updateBuildManifest(const std::string& filename) {
        std::ofstream manifest("build_manifest.txt", std::ios::app);
        manifest << filename << "\n";
    }
};

} // namespace RawrXD
