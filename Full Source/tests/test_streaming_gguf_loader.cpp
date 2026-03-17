#include "streaming_gguf_loader.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "logging/logger.h"
static Logger s_logger("test_streaming_gguf_loader");

namespace {
struct TensorDef {
    std::string name;
    std::vector<uint64_t> shape;
    GGMLType type;
    uint64_t size_bytes;
    uint64_t offset;
};

uint64_t typeSize(GGMLType type) {
    switch (type) {
    case GGMLType::F32: return sizeof(float);
    case GGMLType::F16: return 2;
    case GGMLType::BF16: return 2;
    default: return sizeof(float);
    }
}

uint64_t tensorSize(const std::vector<uint64_t>& shape, GGMLType type) {
    uint64_t elements = 1;
    for (uint64_t d : shape) {
        elements *= d;
    }
    return elements * typeSize(type);
}

uint64_t tensorInfoBytes(const TensorDef& def) {
    return sizeof(uint64_t) + def.name.size() + sizeof(uint32_t) +
           def.shape.size() * sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint64_t);
}

uint64_t align32(uint64_t value) {
    return (value + 31u) & ~31u;
}

void writeTensorInfo(std::ofstream& out, const TensorDef& def) {
    uint64_t name_len = def.name.size();
    out.write(reinterpret_cast<const char*>(&name_len), sizeof(uint64_t));
    out.write(def.name.data(), static_cast<std::streamsize>(def.name.size()));

    uint32_t n_dims = static_cast<uint32_t>(def.shape.size());
    out.write(reinterpret_cast<const char*>(&n_dims), sizeof(uint32_t));
    for (uint64_t dim : def.shape) {
        out.write(reinterpret_cast<const char*>(&dim), sizeof(uint64_t));
    }

    uint32_t type_val = static_cast<uint32_t>(def.type);
    out.write(reinterpret_cast<const char*>(&type_val), sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(&def.offset), sizeof(uint64_t));
}

TensorDef makeTensor(const std::string& name, const std::vector<uint64_t>& shape, GGMLType type, uint64_t offset) {
    TensorDef def{name, shape, type, 0, offset};
    def.size_bytes = tensorSize(shape, type);
    return def;
}

std::vector<TensorDef> buildTestModel(const std::filesystem::path& path) {
    std::vector<TensorDef> defs;
    defs.reserve(4);
    defs.push_back(makeTensor("token_embd.weight", {262144}, GGMLType::F32, 0));
    defs.push_back(makeTensor("blk.0.ffn", {360000}, GGMLType::F16, 0));
    defs.push_back(makeTensor("blk.1.ffn", {360000}, GGMLType::F16, 0));
    defs.push_back(makeTensor("output.weight", {800000}, GGMLType::F16, 0));

    const uint64_t header_bytes = sizeof(uint32_t) * 2 + sizeof(uint64_t) * 2;
    uint64_t info_bytes = 0;
    for (const auto& d : defs) {
        info_bytes += tensorInfoBytes(d);
    }
    uint64_t data_start = align32(header_bytes + info_bytes);

    uint64_t cursor = data_start;
    for (auto& d : defs) {
        d.offset = cursor;
        cursor = align32(cursor + d.size_bytes);
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("failed to create test gguf file");
    }

    const uint32_t magic = 0x46554747;
    const uint32_t version = 3;
    const uint64_t tensor_count = defs.size();
    const uint64_t metadata_count = 0;

    out.write(reinterpret_cast<const char*>(&magic), sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(&version), sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(&tensor_count), sizeof(uint64_t));
    out.write(reinterpret_cast<const char*>(&metadata_count), sizeof(uint64_t));

    for (const auto& d : defs) {
        writeTensorInfo(out, d);
    }

    if (data_start > static_cast<uint64_t>(out.tellp())) {
        const uint64_t pad = data_start - static_cast<uint64_t>(out.tellp());
        std::vector<char> zeros(pad, 0);
        out.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
    }

    const uint64_t final_size = defs.back().offset + defs.back().size_bytes;
    out.seekp(static_cast<std::streamoff>(final_size - 1));
    out.put(0);
    out.close();

    return defs;
}

void setBudgetEnv() {
#ifdef _WIN32
    _putenv_s("RAWRXD_STREAMING_ZONE_MB", "1");
#else
    setenv("RAWRXD_STREAMING_ZONE_MB", "1", 1);
#endif
}
} // namespace

int main() {
    try {
        setBudgetEnv();
        const auto temp_dir = std::filesystem::temp_directory_path();
        const auto temp_path = temp_dir / "streaming_loader_120b.gguf";
        
        s_logger.error( "Creating test GGUF at: " << temp_path << std::endl;
        auto defs = buildTestModel(temp_path);
        
        if (!std::filesystem::exists(temp_path)) {
            s_logger.error( "Test GGUF file was not created" << std::endl;
            return 1;
        }
        s_logger.error( "Test GGUF file size: " << std::filesystem::file_size(temp_path) << " bytes" << std::endl;

        StreamingGGUFLoader loader;
        if (!loader.Open(temp_path.string())) {
            s_logger.error( "failed to open streaming gguf loader" << std::endl;
            std::filesystem::remove(temp_path);
            return 1;
        }
        s_logger.error( "✓ Loader opened successfully" << std::endl;

        const uint64_t budget_bytes = 1024ull * 1024ull;
        auto zones = loader.GetAllZones();
        if (zones.empty()) {
            s_logger.error( "no zones were assigned" << std::endl;
            std::filesystem::remove(temp_path);
            return 2;
        }
        s_logger.error( "✓ Zones assigned: " << zones.size() << std::endl;

        for (const auto& zone_name : zones) {
            auto info = loader.GetZoneInfo(zone_name);
            if (zone_name.rfind("layers_", 0) == 0 && zone_name.find("oversize") == std::string::npos) {
                if (info.total_bytes > budget_bytes) {
                    s_logger.error( "layer zone exceeds budget" << std::endl;
                    std::filesystem::remove(temp_path);
                    return 3;
                }
            }
        }
        s_logger.error( "✓ Zone budgets validated" << std::endl;

        std::vector<uint8_t> data;
        if (!loader.GetTensorData("output.weight", data)) {
            s_logger.error( "failed to stream oversize tensor" << std::endl;
            std::filesystem::remove(temp_path);
            return 4;
        }
        if (data.size() != defs.back().size_bytes) {
            s_logger.error( "unexpected tensor size: " << data.size() << " vs " << defs.back().size_bytes << std::endl;
            std::filesystem::remove(temp_path);
            return 5;
        }
        s_logger.error( "✓ Tensor streaming works" << std::endl;

        loader.Close();
        std::filesystem::remove(temp_path);
        s_logger.info("PASS: 120B streaming loader test");
        return 0;
    } catch (const std::exception& ex) {
        s_logger.error( "Exception: " << ex.what() << std::endl;
        return 10;
    }
}
