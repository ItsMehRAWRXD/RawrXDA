#include "gguf_loader.h"

namespace CPUInference {

GGUFLoader::GGUFLoader() {
    // Stub
}

GGUFLoader::~GGUFLoader() {
    // Stub
}

bool GGUFLoader::Open(const std::string& filepath) {
    // Stub
    return false;
}

bool GGUFLoader::Close() {
    // Stub
    return false;
}

bool GGUFLoader::ParseHeader() {
    // Stub
    return false;
}

GGUFHeader GGUFLoader::GetHeader() const {
    // Stub
    return {};
}

bool GGUFLoader::ParseMetadata() {
    // Stub
    return false;
}

GGUFMetadata GGUFLoader::GetMetadata() const {
    // Stub
    return {};
}

std::vector<TensorInfo> GGUFLoader::GetTensorInfo() const {
    // Stub
    return {};
}

bool GGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // Stub
    return false;
}

bool GGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    // Stub
    return false;
}

size_t GGUFLoader::GetTensorByteSize(const TensorInfo& tensor) const {
    // Stub
    return 0;
}

std::string GGUFLoader::GetTypeString(GGMLType type) const {
    // Stub
    return "";
}

uint64_t GGUFLoader::GetFileSize() const {
    // Stub
    return 0;
}

bool GGUFLoader::BuildTensorIndex() {
    // Stub
    return false;
}

bool GGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
    // Stub
    return false;
}

bool GGUFLoader::UnloadZone(const std::string& zone_name) {
    // Stub
    return false;
}

std::vector<std::string> GGUFLoader::GetLoadedZones() const {
    // Stub
    return {};
}

std::vector<std::string> GGUFLoader::GetAllZones() const {
    // Stub
    return {};
}

std::vector<TensorInfo> GGUFLoader::GetAllTensorInfo() const {
    // Stub
    return {};
}

uint64_t GGUFLoader::GetCurrentMemoryUsage() const {
    // Stub
    return 0;
}

} // namespace CPUInference