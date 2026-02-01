#pragma once
#include <cstdint>
#include <string>
#include <vector>

enum class TensorState {
    UNLOADED,
    LOADING,
    LOADED,
    EVICTED,
    ERROR_STATE
};

struct TensorMetadata {
    std::string name;
    std::vector<int64_t> shape;
    int type;
    uint64_t offset;
    uint64_t size;
    TensorState state;
    void* data;
};

namespace Phase2 {
    class ModelLoader {
    public:
        ModelLoader();
        ~ModelLoader();
        
        bool LoadModel(const char* path);
        uint64_t GetTensorCount() const;
        TensorMetadata* GetTensorByIndex(uint64_t index);
        TensorMetadata* GetTensor(const char* name);
        
        uint64_t GetBytesLoaded() const;
        uint64_t GetTotalSize() const;
        
        bool IsTensorLoaded(const char* name);
        bool PrefetchTensor(const char* name);
        void EvictTensor(const char* name);
        
    private:
        void* m_context;
    };
}
