#include <concepts>
#include <coroutine>

template<typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template<Numeric T>
class Tensor {
    T* data_;
    size_t size_;
    
public:
    explicit Tensor(size_t n) : size_(n) {
        data_ = new T[n];
    }
    
    ~Tensor() { delete[] data_; }
    
    // Cursor position for completion test: v
    T& operator[](size_t i) { return data_[i]; }
    
    template<typename U>
    requires Numeric<U>
    auto dot(const Tensor<U>& other) -> decltype(T{} * U{}) {
        // Cursor: test type-aware completion here
        return {};
    }
    
private:
    void validateIndex(size_t i) const;
};

// Test scope-aware completion
class ModelLoader {
    std::string modelPath_;
    
protected:
    virtual bool validateFormat() = 0;
    
public:
    explicit ModelLoader(const std::string& path) : modelPath_(path) {}
    
    // Cursor: test access modifier awareness
    virtual ~ModelLoader() = default;
};
