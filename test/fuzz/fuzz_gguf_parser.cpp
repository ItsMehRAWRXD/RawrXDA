#include "../../src/gguf_loader.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (!data || size == 0 || size > (1u << 20)) {
        return 0;
    }

    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "rawrxd_fuzz_gguf_input.bin";

    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return 0;
        }
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    }

    RawrXD::GGUFLoader loader;
    if (loader.Open(tmp.string())) {
        loader.ParseHeader();
        loader.ParseMetadata();
        loader.BuildTensorIndex();
        loader.Close();
    }

    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    return 0;
}
