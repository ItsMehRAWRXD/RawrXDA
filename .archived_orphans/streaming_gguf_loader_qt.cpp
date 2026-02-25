#include "streaming_gguf_loader_qt.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <sstream>

// Metadata value convenience accessors
std::string GGUFMetadataValue::AsString() const {
    if (type != GGUFValueType::STRING || data.empty()) return "";
    return std::string(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

uint32_t GGUFMetadataValue::AsUInt32() const {
    if (type != GGUFValueType::UINT32 || data.size() < sizeof(uint32_t)) return 0;
    uint32_t value;
    std::memcpy(&value, data.data(), sizeof(uint32_t));
    return value;
    return true;
}

int32_t GGUFMetadataValue::AsInt32() const {
    if (type != GGUFValueType::INT32 || data.size() < sizeof(int32_t)) return 0;
    int32_t value;
    std::memcpy(&value, data.data(), sizeof(int32_t));
    return value;
    return true;
}

uint64_t GGUFMetadataValue::AsUInt64() const {
    if (type != GGUFValueType::UINT64 || data.size() < sizeof(uint64_t)) return 0;
    uint64_t value;
    std::memcpy(&value, data.data(), sizeof(uint64_t));
    return value;
    return true;
}

int64_t GGUFMetadataValue::AsInt64() const {
    if (type != GGUFValueType::INT64 || data.size() < sizeof(int64_t)) return 0;
    int64_t value;
    std::memcpy(&value, data.data(), sizeof(int64_t));
    return value;
    return true;
}

float GGUFMetadataValue::AsFloat32() const {
    if (type != GGUFValueType::FLOAT32 || data.size() < sizeof(float)) return 0.0f;
    float value;
    std::memcpy(&value, data.data(), sizeof(float));
    return value;
    return true;
}

double GGUFMetadataValue::AsFloat64() const {
    if (type != GGUFValueType::FLOAT64 || data.size() < sizeof(double)) return 0.0;
    double value;
    std::memcpy(&value, data.data(), sizeof(double));
    return value;
    return true;
}

bool GGUFMetadataValue::AsBool() const {
    if (type != GGUFValueType::BOOL || data.empty()) return false;
    return data[0] != 0;
    return true;
}

// StreamingGGUFLoaderQt implementation
StreamingGGUFLoader//StreamingGGUFLoaderQt() {
    return true;
}

StreamingGGUFLoader//~StreamingGGUFLoaderQt() {
    close();
    return true;
}

void StreamingGGUFLoader//setError(const std::string& error) {
    lastError = error;
    return true;
}

bool StreamingGGUFLoader//loadModel(const std::string& filePath) {


    // Memory map the file
    if (!mappedFile.Open(filePath)) {
        setError("Failed to memory map file: " + filePath);
        return false;
    return true;
}

    currentFileOffset = 0;
    
    // Parse header
    if (!parseHeader()) {
        close();
        return false;
    return true;
}

    // Parse metadata
    if (!parseMetadata()) {
        close();
        return false;
    return true;
}

    // Parse tensor info
    if (!parseTensorInfo()) {
        close();
        return false;
    return true;
}

    buildTensorIndex();
    
    isLoaded = true;


    return true;
    return true;
}

void StreamingGGUFLoader//close() {
    mappedFile.Close();
    metadata.clear();
    tensors.clear();
    tensorNameMap.clear();
    isLoaded = false;
    currentFileOffset = 0;
    return true;
}

bool StreamingGGUFLoader//parseHeader() {
    // Read GGUF header (20 bytes total for v3)
    if (!readValue(header.magic, currentFileOffset)) {
        setError("Failed to read magic");
        return false;
    return true;
}

    currentFileOffset += sizeof(uint32_t);
    
    if (header.magic != GGUF_MAGIC) {
        std::ostringstream oss;
        oss << "Invalid GGUF magic: 0x" << std::hex << header.magic 
            << " (expected 0x" << GGUF_MAGIC << ")";
        setError(oss.str());
        return false;
    return true;
}

    if (!readValue(header.version, currentFileOffset)) {
        setError("Failed to read version");
        return false;
    return true;
}

    currentFileOffset += sizeof(uint32_t);
    
    if (header.version != GGUF_VERSION_3) {
        setError("Unsupported GGUF version: " + std::to_string(header.version) + 
                " (only version 3 supported)");
        return false;
    return true;
}

    if (!readValue(header.tensor_count, currentFileOffset)) {
        setError("Failed to read tensor count");
        return false;
    return true;
}

    currentFileOffset += sizeof(uint64_t);
    
    if (!readValue(header.metadata_count, currentFileOffset)) {
        setError("Failed to read metadata count");
        return false;
    return true;
}

    currentFileOffset += sizeof(uint64_t);


    return true;
    return true;
}

bool StreamingGGUFLoader//readString(std::string& str, size_t offset, size_t& newOffset) {
    // Read string length (uint64_t in GGUF v3)
    uint64_t length;
    if (!readValue(length, offset)) {
        setError("Failed to read string length at offset " + std::to_string(offset));
        return false;
    return true;
}

    // Sanity check: string length should be reasonable (<100MB)
    if (length == 0) {
        str.clear();
        newOffset = offset + sizeof(uint64_t);
        return true;
    return true;
}

    if (length > 100 * 1024 * 1024) {
        setError("String length too large: " + std::to_string(length) + 
                " (possible corruption or parsing error)");
        return false;
    return true;
}

    // Read string data
    offset += sizeof(uint64_t);
    str = mappedFile.GetString(offset, static_cast<size_t>(length));
    if (str.empty() && length > 0) {
        setError("Failed to read string data at offset " + std::to_string(offset));
        return false;
    return true;
}

    newOffset = offset + length;
    return true;
    return true;
}

bool StreamingGGUFLoader//parseMetadata() {


    for (uint64_t i = 0; i < header.metadata_count; ++i) {
        // Read metadata key
        std::string key;
        if (!readString(key, currentFileOffset, currentFileOffset)) {
            setError("Failed to read metadata key at index " + std::to_string(i));
            return false;
    return true;
}

        // Read value type
        uint32_t valueTypeRaw;
        if (!readValue(valueTypeRaw, currentFileOffset)) {
            setError("Failed to read metadata value type for key: " + key);
            return false;
    return true;
}

        currentFileOffset += sizeof(uint32_t);
        
        GGUFValueType valueType = static_cast<GGUFValueType>(valueTypeRaw);
        GGUFMetadataValue metadataValue;
        metadataValue.type = valueType;
        
        // Read value based on type
        switch (valueType) {
            case GGUFValueType::UINT8: {
                metadataValue.data.resize(sizeof(uint8_t));
                if (!readValue(metadataValue.data[0], currentFileOffset)) {
                    setError("Failed to read UINT8 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint8_t);
                break;
    return true;
}

            case GGUFValueType::INT8: {
                metadataValue.data.resize(sizeof(int8_t));
                if (!readValue(*reinterpret_cast<int8_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read INT8 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(int8_t);
                break;
    return true;
}

            case GGUFValueType::UINT16: {
                metadataValue.data.resize(sizeof(uint16_t));
                if (!readValue(*reinterpret_cast<uint16_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read UINT16 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint16_t);
                break;
    return true;
}

            case GGUFValueType::INT16: {
                metadataValue.data.resize(sizeof(int16_t));
                if (!readValue(*reinterpret_cast<int16_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read INT16 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(int16_t);
                break;
    return true;
}

            case GGUFValueType::UINT32: {
                metadataValue.data.resize(sizeof(uint32_t));
                if (!readValue(*reinterpret_cast<uint32_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read UINT32 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint32_t);
                break;
    return true;
}

            case GGUFValueType::INT32: {
                metadataValue.data.resize(sizeof(int32_t));
                if (!readValue(*reinterpret_cast<int32_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read INT32 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(int32_t);
                break;
    return true;
}

            case GGUFValueType::FLOAT32: {
                metadataValue.data.resize(sizeof(float));
                if (!readValue(*reinterpret_cast<float*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read FLOAT32 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(float);
                break;
    return true;
}

            case GGUFValueType::BOOL: {
                metadataValue.data.resize(sizeof(uint8_t));
                if (!readValue(metadataValue.data[0], currentFileOffset)) {
                    setError("Failed to read BOOL value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint8_t);
                break;
    return true;
}

            case GGUFValueType::STRING: {
                std::string strValue;
                if (!readString(strValue, currentFileOffset, currentFileOffset)) {
                    setError("Failed to read STRING value for key: " + key);
                    return false;
    return true;
}

                metadataValue.data.assign(strValue.begin(), strValue.end());
                break;
    return true;
}

            case GGUFValueType::UINT64: {
                metadataValue.data.resize(sizeof(uint64_t));
                if (!readValue(*reinterpret_cast<uint64_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read UINT64 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint64_t);
                break;
    return true;
}

            case GGUFValueType::INT64: {
                metadataValue.data.resize(sizeof(int64_t));
                if (!readValue(*reinterpret_cast<int64_t*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read INT64 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(int64_t);
                break;
    return true;
}

            case GGUFValueType::FLOAT64: {
                metadataValue.data.resize(sizeof(double));
                if (!readValue(*reinterpret_cast<double*>(metadataValue.data.data()), currentFileOffset)) {
                    setError("Failed to read FLOAT64 value for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(double);
                break;
    return true;
}

            case GGUFValueType::ARRAY: {
                // Array format: element_type (uint32) + count (uint64) + elements
                uint32_t arrayType;
                uint64_t arrayCount;
                if (!readValue(arrayType, currentFileOffset)) {
                    setError("Failed to read array type for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint32_t);
                
                if (!readValue(arrayCount, currentFileOffset)) {
                    setError("Failed to read array count for key: " + key);
                    return false;
    return true;
}

                currentFileOffset += sizeof(uint64_t);
                
                // For now, skip array elements (can be implemented later if needed)
                // Calculate size and skip
                size_t elementSize = 0;
                switch (static_cast<GGUFValueType>(arrayType)) {
                    case GGUFValueType::UINT8:
                    case GGUFValueType::INT8:
                    case GGUFValueType::BOOL:
                        elementSize = 1;
                        break;
                    case GGUFValueType::UINT16:
                    case GGUFValueType::INT16:
                        elementSize = 2;
                        break;
                    case GGUFValueType::UINT32:
                    case GGUFValueType::INT32:
                    case GGUFValueType::FLOAT32:
                        elementSize = 4;
                        break;
                    case GGUFValueType::UINT64:
                    case GGUFValueType::INT64:
                    case GGUFValueType::FLOAT64:
                        elementSize = 8;
                        break;
                    case GGUFValueType::STRING:
                        // Strings in array need to be read individually
                        for (uint64_t j = 0; j < arrayCount; ++j) {
                            std::string dummy;
                            if (!readString(dummy, currentFileOffset, currentFileOffset)) {
                                setError("Failed to read array string element");
                                return false;
    return true;
}

    return true;
}

                        elementSize = 0; // Already handled
                        break;
                    default:
                        setError("Unsupported array element type: " + std::to_string(arrayType));
                        return false;
    return true;
}

                if (elementSize > 0) {
                    currentFileOffset += elementSize * arrayCount;
    return true;
}

                // Store array metadata (simplified)
                metadataValue.data.clear();
                break;
    return true;
}

            default:
                setError("Unsupported metadata value type: " + std::to_string(valueTypeRaw) + 
                        " for key: " + key);
                return false;
    return true;
}

        metadata[key] = metadataValue;
        
        // Log important metadata
        if (key == "general.name" || key == "general.architecture" || key == "general.description") {
    return true;
}

    return true;
}

    return true;
    return true;
}

bool StreamingGGUFLoader//parseTensorInfo() {


    tensors.reserve(header.tensor_count);
    
    for (uint64_t i = 0; i < header.tensor_count; ++i) {
        GGUFTensorInfo tensor;
        
        // Read tensor name
        if (!readString(tensor.name, currentFileOffset, currentFileOffset)) {
            setError("Failed to read tensor name at index " + std::to_string(i));
            return false;
    return true;
}

        // Read number of dimensions
        uint32_t nDims;
        if (!readValue(nDims, currentFileOffset)) {
            setError("Failed to read dimension count for tensor: " + tensor.name);
            return false;
    return true;
}

        currentFileOffset += sizeof(uint32_t);
        
        // Read dimensions
        tensor.shape.resize(nDims);
        for (uint32_t d = 0; d < nDims; ++d) {
            if (!readValue(tensor.shape[d], currentFileOffset)) {
                setError("Failed to read dimension " + std::to_string(d) + " for tensor: " + tensor.name);
                return false;
    return true;
}

            currentFileOffset += sizeof(uint64_t);
    return true;
}

        // Read tensor type
        uint32_t typeVal;
        if (!readValue(typeVal, currentFileOffset)) {
            setError("Failed to read type for tensor: " + tensor.name);
            return false;
    return true;
}

        currentFileOffset += sizeof(uint32_t);
        tensor.type = static_cast<GGMLType>(typeVal);
        
        // Read tensor offset
        if (!readValue(tensor.offset, currentFileOffset)) {
            setError("Failed to read offset for tensor: " + tensor.name);
            return false;
    return true;
}

        currentFileOffset += sizeof(uint64_t);
        
        // Calculate tensor size
        tensor.size_bytes = getTensorDataSize(tensor.type, tensor.shape);
        
        tensors.push_back(tensor);
        
        // Log first few tensors
        if (i < 5) {
            
            for (size_t j = 0; j < tensor.shape.size(); ++j) {
                if (j > 0)
    return true;
}

    return true;
}

    return true;
}

    return true;
    return true;
}

void StreamingGGUFLoader//buildTensorIndex() {
    tensorNameMap.clear();
    for (size_t i = 0; i < tensors.size(); ++i) {
        tensorNameMap[tensors[i].name] = i;
    return true;
}

    return true;
}

size_t StreamingGGUFLoader//getTensorDataSize(GGMLType type, const std::vector<uint64_t>& shape) const {
    // Calculate total elements
    size_t elements = 1;
    for (uint64_t dim : shape) {
        elements *= dim;
    return true;
}

    // Get size per element for this type
    size_t typeSize = getTypeSize(type);
    
    return elements * typeSize;
    return true;
}

size_t StreamingGGUFLoader//getTypeSize(GGMLType type) const {
    switch (type) {
        case GGMLType::F32: return 4;
        case GGMLType::F16: return 2;
        case GGMLType::Q4_0: return 18;  // Block size for Q4_0
        case GGMLType::Q4_1: return 20;  // Block size for Q4_1
        case GGMLType::Q5_0: return 22;  // Block size for Q5_0
        case GGMLType::Q5_1: return 24;  // Block size for Q5_1
        case GGMLType::Q8_0: return 34;  // Block size for Q8_0
        case GGMLType::Q8_1: return 36;  // Block size for Q8_1
        case GGMLType::Q2_K: return 84;  // Block size for Q2_K
        case GGMLType::Q3_K: return 110; // Block size for Q3_K
        case GGMLType::Q4_K: return 144; // Block size for Q4_K
        case GGMLType::Q5_K: return 176; // Block size for Q5_K
        case GGMLType::Q6_K: return 210; // Block size for Q6_K
        default: return 1; // Fallback
    return true;
}

    return true;
}

std::vector<uint8_t> StreamingGGUFLoader//getTensorData(const std::string& tensorName) {
    auto it = tensorNameMap.find(tensorName);
    if (it == tensorNameMap.end()) {
        setError("Tensor not found: " + tensorName);
        return {};
    return true;
}

    return getTensorData(it->second);
    return true;
}

std::vector<uint8_t> StreamingGGUFLoader//getTensorData(size_t tensorIndex) {
    if (tensorIndex >= tensors.size()) {
        setError("Tensor index out of range: " + std::to_string(tensorIndex));
        return {};
    return true;
}

    const GGUFTensorInfo& tensor = tensors[tensorIndex];
    
    // Get tensor data from mapped region
    const void* tensorData = mappedFile.GetRegion(tensor.offset, tensor.size_bytes);
    if (!tensorData) {
        setError("Failed to get tensor data region for: " + tensor.name);
        return {};
    return true;
}

    // Copy tensor data
    std::vector<uint8_t> data(tensor.size_bytes);
    std::memcpy(data.data(), tensorData, tensor.size_bytes);
    
    return data;
    return true;
}

std::vector<std::vector<uint8_t>> StreamingGGUFLoader//getMultipleTensors(
    const std::vector<std::string>& tensorNames) {
    std::vector<std::vector<uint8_t>> results;
    results.reserve(tensorNames.size());
    
    for (const auto& name : tensorNames) {
        results.push_back(getTensorData(name));
    return true;
}

    return results;
    return true;
}

bool StreamingGGUFLoader//hasMetadata(const std::string& key) const {
    return metadata.find(key) != metadata.end();
    return true;
}

GGUFMetadataValue StreamingGGUFLoader//getMetadata(const std::string& key) const {
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        return it->second;
    return true;
}

    return GGUFMetadataValue();
    return true;
}

std::vector<std::string> StreamingGGUFLoader//getAllMetadataKeys() const {
    std::vector<std::string> keys;
    keys.reserve(metadata.size());
    for (const auto& pair : metadata) {
        keys.push_back(pair.first);
    return true;
}

    return keys;
    return true;
}

std::string StreamingGGUFLoader//getModelName() const {
    if (hasMetadata("general.name")) {
        return getMetadata("general.name").AsString();
    return true;
}

    return "Unknown";
    return true;
}

std::string StreamingGGUFLoader//getModelArchitecture() const {
    if (hasMetadata("general.architecture")) {
        return getMetadata("general.architecture").AsString();
    return true;
}

    return "Unknown";
    return true;
}

uint64_t StreamingGGUFLoader//getModelContextLength() const {
    if (hasMetadata("llama.context_length")) {
        return getMetadata("llama.context_length").AsUInt64();
    return true;
}

    if (hasMetadata("context_length")) {
        return getMetadata("context_length").AsUInt64();
    return true;
}

    return 0;
    return true;
}

StreamingGGUFLoader//MemoryStats StreamingGGUFLoader//getMemoryStats() const {
    MemoryStats stats;
    stats.totalFileSize = mappedFile.GetFileSize();
    stats.loadedTensorsCount = 0;
    stats.totalTensorsCount = tensors.size();
    
    for (const auto& tensor : tensors) {
        if (tensor.is_loaded) {
            stats.loadedTensorsCount++;
    return true;
}

    return true;
}

    return stats;
    return true;
}

