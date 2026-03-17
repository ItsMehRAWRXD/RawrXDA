#include "streaming_gguf_loader.h"

StreamingGGUFLoader::StreamingGGUFLoader() {
    // Stub
}

StreamingGGUFLoader::~StreamingGGUFLoader() {
    // Stub
}

bool StreamingGGUFLoader::Open(const std::string& filepath) {
    // Stub
    return false;
}

bool StreamingGGUFLoader::Close() {
    // Stub
    return false;
}

bool StreamingGGUFLoader::ParseHeader() {
    // Stub
    return false;
}

GGUFHeader StreamingGGUFLoader::GetHeader() const {
    // Stub
    return {};
}

bool StreamingGGUFLoader::ParseMetadata() {
    // Stub
    return false;
}

GGUFMetadata StreamingGGUFLoader::GetMetadata() const {
    // Stub
    return {};
}

std::vector<TensorInfo> StreamingGGUFLoader::GetTensorInfo() const {
    // Stub
    return {};
}

bool StreamingGGUFLoader::LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) {
    // Stub
    return false;
}

bool StreamingGGUFLoader::LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) {
    // Stub
    return false;
}

size_t StreamingGGUFLoader::GetTensorByteSize(const TensorInfo& tensor) const {
    // Stub
    return 0;
}

std::string StreamingGGUFLoader::GetTypeString(GGMLType type) const {
    // Stub
    return "";
}

uint64_t StreamingGGUFLoader::GetFileSize() const {
    // Stub
    return 0;
}

bool StreamingGGUFLoader::BuildTensorIndex() {
    // Stub
    return false;
}

bool StreamingGGUFLoader::LoadZone(const std::string& zone_name, uint64_t max_memory_mb) {
    // Stub
    return false;
}

bool StreamingGGUFLoader::UnloadZone(const std::string& zone_name) {
    // Stub
    return false;
}

std::vector<std::string> StreamingGGUFLoader::GetLoadedZones() const {
    // Stub
    return {};
}

std::vector<std::string> StreamingGGUFLoader::GetAllZones() const {
    // Stub
    return {};
}

std::vector<TensorInfo> StreamingGGUFLoader::GetAllTensorInfo() const {
    // Stub
    return {};
}

uint64_t StreamingGGUFLoader::GetCurrentMemoryUsage() const {
    // Stub
    return 0;
}