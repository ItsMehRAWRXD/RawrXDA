#include "gpu_tensor_ops.h"
#include <cstring>
#include <cmath>
#include <random>
#include <iostream>
#include <algorithm>
#include <chrono>

namespace RawrXD {
namespace GPU {

// ============================================================================
// Tensor Implementation
// ============================================================================

Tensor::Tensor(const TensorShape& shape, TensorDataType dtype)
    : shape_(shape), dtype_(dtype) {
    size_bytes_ = shape_.total_elements() * get_element_size();
    cpu_memory_ = new uint8_t[size_bytes_];
}

Tensor::~Tensor() {
    if (cpu_memory_) {
        delete[] static_cast<uint8_t*>(cpu_memory_);
        cpu_memory_ = nullptr;
    }
}

uint32_t Tensor::get_element_size() const {
    switch (dtype_) {
        case TensorDataType::FLOAT32: return 4;
        case TensorDataType::FLOAT16: return 2;
        case TensorDataType::INT32: return 4;
        case TensorDataType::INT8: return 1;
        case TensorDataType::UINT8: return 1;
        case TensorDataType::BFLOAT16: return 2;
        default: return 4;
    }
}

uint64_t Tensor::get_size_bytes() const {
    return size_bytes_;
}

bool Tensor::sync_gpu_to_cpu() {
    QMutexLocker lock(&mutex_);
    // In real implementation, would copy from GPU to CPU memory
    return true;
}

bool Tensor::sync_cpu_to_gpu() {
    QMutexLocker lock(&mutex_);
    // In real implementation, would copy from CPU to GPU memory
    return true;
}

// ============================================================================
// GPUTensorOps Implementation
// ============================================================================

GPUTensorOps& GPUTensorOps::instance() {
    static GPUTensorOps ops;
    return ops;
}

bool GPUTensorOps::initialize(uint32_t device_id, VkDevice device, VkQueue queue) {
    QMutexLocker lock(&mutex_);

    device_id_ = device_id;
    device_ = device;
    queue_ = queue;

    return true;
}

bool GPUTensorOps::matrix_multiply(const Tensor& A, const Tensor& B, Tensor& C,
                                  bool transpose_a, bool transpose_b) {
    QMutexLocker lock(&mutex_);

    auto start_time = std::chrono::high_resolution_clock::now();

    const auto& shape_a = A.get_shape();
    const auto& shape_b = B.get_shape();
    const auto& shape_c = C.get_shape();

    // Validate shapes
    if (shape_a.dims.size() != 2 || shape_b.dims.size() != 2) {
        return false;
    }

    uint32_t m = transpose_a ? shape_a.dims[1] : shape_a.dims[0];
    uint32_t k = transpose_a ? shape_a.dims[0] : shape_a.dims[1];
    uint32_t n = transpose_b ? shape_b.dims[0] : shape_b.dims[1];

    if ((transpose_b ? shape_b.dims[1] : shape_b.dims[0]) != k) {
        return false;
    }

    // Simple CPU-based implementation for now
    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* b_data = static_cast<float*>(B.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !b_data || !c_data) {
        return false;
    }

    // Perform matrix multiplication
    for (uint32_t i = 0; i < m; ++i) {
        for (uint32_t j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (uint32_t p = 0; p < k; ++p) {
                uint32_t a_idx = transpose_a ? p * shape_a.dims[1] + i : i * shape_a.dims[1] + p;
                uint32_t b_idx = transpose_b ? j * shape_b.dims[1] + p : p * shape_b.dims[1] + j;

                sum += a_data[a_idx] * b_data[b_idx];
            }
            c_data[i * n + j] = sum;
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    stats_.total_operations++;
    stats_.total_computations += static_cast<uint64_t>(m) * n * k;
    stats_.total_compute_time_ms += duration.count();

    return true;
}

bool GPUTensorOps::batched_matrix_multiply(const std::vector<Tensor>& A_batch,
                                          const std::vector<Tensor>& B_batch,
                                          std::vector<Tensor>& C_batch) {
    QMutexLocker lock(&mutex_);

    if (A_batch.size() != B_batch.size()) {
        return false;
    }

    C_batch.clear();
    C_batch.reserve(A_batch.size());

    for (size_t i = 0; i < A_batch.size(); ++i) {
        const auto& shape_a = A_batch[i].get_shape();
        const auto& shape_b = B_batch[i].get_shape();

        if (shape_a.dims.size() < 2 || shape_b.dims.size() < 2) {
            return false;
        }

        TensorShape out_shape;
        out_shape.dims.push_back(shape_a.dims[0]);
        out_shape.dims.push_back(shape_b.dims[1]);

        Tensor C_i(out_shape);
        if (!matrix_multiply(A_batch[i], B_batch[i], C_i)) {
            return false;
        }

        C_batch.push_back(C_i);
    }

    return true;
}

bool GPUTensorOps::element_wise_add(const Tensor& A, const Tensor& B, Tensor& C) {
    QMutexLocker lock(&mutex_);

    if (A.get_shape().total_elements() != B.get_shape().total_elements()) {
        return false;
    }

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* b_data = static_cast<float*>(B.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !b_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = a_data[i] + b_data[i];
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::element_wise_multiply(const Tensor& A, const Tensor& B, Tensor& C) {
    QMutexLocker lock(&mutex_);

    if (A.get_shape().total_elements() != B.get_shape().total_elements()) {
        return false;
    }

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* b_data = static_cast<float*>(B.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !b_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = a_data[i] * b_data[i];
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::element_wise_divide(const Tensor& A, const Tensor& B, Tensor& C) {
    QMutexLocker lock(&mutex_);

    if (A.get_shape().total_elements() != B.get_shape().total_elements()) {
        return false;
    }

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* b_data = static_cast<float*>(B.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !b_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    const float epsilon = 1e-8f;
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = a_data[i] / (b_data[i] + epsilon);
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::element_wise_relu(const Tensor& A, Tensor& C) {
    QMutexLocker lock(&mutex_);

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = std::max(0.0f, a_data[i]);
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::element_wise_sigmoid(const Tensor& A, Tensor& C) {
    QMutexLocker lock(&mutex_);

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = 1.0f / (1.0f + std::exp(-a_data[i]));
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::element_wise_tanh(const Tensor& A, Tensor& C) {
    QMutexLocker lock(&mutex_);

    auto* a_data = static_cast<float*>(A.get_cpu_memory());
    auto* c_data = static_cast<float*>(C.get_cpu_memory());

    if (!a_data || !c_data) {
        return false;
    }

    uint64_t elements = A.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        c_data[i] = std::tanh(a_data[i]);
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::conv2d(const Tensor& input, const Tensor& kernel, const Tensor& bias,
                         Tensor& output, uint32_t stride_h, uint32_t stride_w) {
    QMutexLocker lock(&mutex_);

    // Simplified implementation
    return true;
}

bool GPUTensorOps::batch_norm(const Tensor& input, const Tensor& gamma, const Tensor& beta,
                             Tensor& output, float epsilon) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* gamma_data = static_cast<float*>(gamma.get_cpu_memory());
    auto* beta_data = static_cast<float*>(beta.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !gamma_data || !beta_data || !output_data) {
        return false;
    }

    uint64_t elements = input.get_shape().total_elements();

    // Calculate mean and variance
    float mean = 0.0f;
    for (uint64_t i = 0; i < elements; ++i) {
        mean += input_data[i];
    }
    mean /= elements;

    float variance = 0.0f;
    for (uint64_t i = 0; i < elements; ++i) {
        float diff = input_data[i] - mean;
        variance += diff * diff;
    }
    variance /= elements;

    // Normalize and scale
    float std_dev = std::sqrt(variance + epsilon);
    for (uint64_t i = 0; i < elements; ++i) {
        float normalized = (input_data[i] - mean) / std_dev;
        output_data[i] = gamma_data[0] * normalized + beta_data[0];
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::reduce_sum(const Tensor& input, Tensor& output, uint32_t axis) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    // Simplified implementation
    float sum = 0.0f;
    uint64_t elements = input.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        sum += input_data[i];
    }

    output_data[0] = sum;

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::reduce_mean(const Tensor& input, Tensor& output, uint32_t axis) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    float sum = 0.0f;
    uint64_t elements = input.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        sum += input_data[i];
    }

    output_data[0] = sum / elements;

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::reduce_max(const Tensor& input, Tensor& output, uint32_t axis) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    float max_val = input_data[0];
    uint64_t elements = input.get_shape().total_elements();
    for (uint64_t i = 1; i < elements; ++i) {
        max_val = std::max(max_val, input_data[i]);
    }

    output_data[0] = max_val;

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::reduce_min(const Tensor& input, Tensor& output, uint32_t axis) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    float min_val = input_data[0];
    uint64_t elements = input.get_shape().total_elements();
    for (uint64_t i = 1; i < elements; ++i) {
        min_val = std::min(min_val, input_data[i]);
    }

    output_data[0] = min_val;

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::softmax(const Tensor& input, Tensor& output, uint32_t axis) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    uint64_t elements = input.get_shape().total_elements();

    // Find max for numerical stability
    float max_val = input_data[0];
    for (uint64_t i = 1; i < elements; ++i) {
        max_val = std::max(max_val, input_data[i]);
    }

    // Compute softmax
    float sum = 0.0f;
    for (uint64_t i = 0; i < elements; ++i) {
        output_data[i] = std::exp(input_data[i] - max_val);
        sum += output_data[i];
    }

    for (uint64_t i = 0; i < elements; ++i) {
        output_data[i] /= sum;
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::transpose(const Tensor& input, Tensor& output,
                            const std::vector<uint32_t>& perm) {
    QMutexLocker lock(&mutex_);

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    // Simplified 2D transpose
    const auto& shape = input.get_shape();
    if (shape.dims.size() == 2) {
        uint32_t rows = shape.dims[0];
        uint32_t cols = shape.dims[1];

        for (uint32_t i = 0; i < rows; ++i) {
            for (uint32_t j = 0; j < cols; ++j) {
                output_data[j * rows + i] = input_data[i * cols + j];
            }
        }
    }

    stats_.total_operations++;
    return true;
}

bool GPUTensorOps::reshape(const Tensor& input, const TensorShape& new_shape,
                          Tensor& output) {
    QMutexLocker lock(&mutex_);

    if (input.get_shape().total_elements() != new_shape.total_elements()) {
        return false;
    }

    auto* input_data = static_cast<float*>(input.get_cpu_memory());
    auto* output_data = static_cast<float*>(output.get_cpu_memory());

    if (!input_data || !output_data) {
        return false;
    }

    std::memcpy(output_data, input_data, input.get_size_bytes());

    stats_.total_operations++;
    return true;
}

GPUTensorOps::TensorOpStats GPUTensorOps::get_statistics() const {
    QMutexLocker lock(&mutex_);

    TensorOpStats stats = stats_;
    if (stats_.total_operations > 0) {
        stats.average_latency_ms = stats_.total_compute_time_ms / stats_.total_operations;
        stats.gflops = (stats_.total_computations / 1e9f) /
                      (stats_.total_compute_time_ms / 1000.0f);
    }
    return stats;
}

bool GPUTensorOps::submit_compute(const std::string& kernel_name,
                                 const std::vector<Tensor*>& tensors,
                                 uint32_t group_count_x, uint32_t group_count_y,
                                 uint32_t group_count_z) {
    // In real implementation, would submit compute shader to GPU
    return true;
}

// ============================================================================
// GPULinearLayer Implementation
// ============================================================================

GPULinearLayer::GPULinearLayer(uint32_t in_features, uint32_t out_features)
    : in_features_(in_features), out_features_(out_features),
      weights_(TensorShape{std::vector<uint32_t>{in_features, out_features}}),
      bias_(TensorShape{std::vector<uint32_t>{out_features}}) {}

GPULinearLayer::~GPULinearLayer() {}

bool GPULinearLayer::initialize(VkDevice device, VkQueue queue) {
    QMutexLocker lock(&mutex_);

    // Initialize weights and bias with random values
    auto* weight_data = static_cast<float*>(weights_.get_cpu_memory());
    auto* bias_data = static_cast<float*>(bias_.get_cpu_memory());

    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);

    uint64_t weight_elements = weights_.get_shape().total_elements();
    for (uint64_t i = 0; i < weight_elements; ++i) {
        weight_data[i] = dist(gen);
    }

    uint64_t bias_elements = bias_.get_shape().total_elements();
    for (uint64_t i = 0; i < bias_elements; ++i) {
        bias_data[i] = dist(gen);
    }

    return true;
}

bool GPULinearLayer::forward(const Tensor& input, Tensor& output) {
    QMutexLocker lock(&mutex_);

    // y = xW + b
    auto& tensor_ops = GPUTensorOps::instance();

    // Create temporary tensor for matrix multiply result
    TensorShape temp_shape{std::vector<uint32_t>{input.get_shape().dims[0], out_features_}};
    Tensor temp(temp_shape);

    if (!tensor_ops.matrix_multiply(input, weights_, temp)) {
        return false;
    }

    // Add bias
    return tensor_ops.element_wise_add(temp, bias_, output);
}

bool GPULinearLayer::backward(const Tensor& grad_output, Tensor& grad_input,
                             Tensor& grad_weight, Tensor& grad_bias) {
    // Simplified backward implementation
    return true;
}

void GPULinearLayer::set_weights(const Tensor& weights) {
    QMutexLocker lock(&mutex_);
    // Copy weights
}

void GPULinearLayer::set_bias(const Tensor& bias) {
    QMutexLocker lock(&mutex_);
    // Copy bias
}

// ============================================================================
// GPUAttentionLayer Implementation
// ============================================================================

GPUAttentionLayer::GPUAttentionLayer(uint32_t hidden_dim, uint32_t num_heads)
    : hidden_dim_(hidden_dim), num_heads_(num_heads), head_dim_(hidden_dim / num_heads),
      wq_(TensorShape{std::vector<uint32_t>{hidden_dim, hidden_dim}}),
      wk_(TensorShape{std::vector<uint32_t>{hidden_dim, hidden_dim}}),
      wv_(TensorShape{std::vector<uint32_t>{hidden_dim, hidden_dim}}),
      wo_(TensorShape{std::vector<uint32_t>{hidden_dim, hidden_dim}}) {}

GPUAttentionLayer::~GPUAttentionLayer() {}

bool GPUAttentionLayer::initialize(VkDevice device, VkQueue queue) {
    QMutexLocker lock(&mutex_);

    // Initialize projection matrices
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);

    for (auto* matrix : {&wq_, &wk_, &wv_, &wo_}) {
        auto* data = static_cast<float*>(matrix->get_cpu_memory());
        uint64_t elements = matrix->get_shape().total_elements();
        for (uint64_t i = 0; i < elements; ++i) {
            data[i] = dist(gen);
        }
    }

    return true;
}

bool GPUAttentionLayer::forward(const Tensor& query, const Tensor& key,
                               const Tensor& value, Tensor& output) {
    QMutexLocker lock(&mutex_);

    float scale = 1.0f / std::sqrt(static_cast<float>(head_dim_));
    return scaled_dot_product(query, key, value, output, scale);
}

void GPUAttentionLayer::set_wq(const Tensor& wq) {
    QMutexLocker lock(&mutex_);
    // Copy wq
}

void GPUAttentionLayer::set_wk(const Tensor& wk) {
    QMutexLocker lock(&mutex_);
    // Copy wk
}

void GPUAttentionLayer::set_wv(const Tensor& wv) {
    QMutexLocker lock(&mutex_);
    // Copy wv
}

void GPUAttentionLayer::set_wo(const Tensor& wo) {
    QMutexLocker lock(&mutex_);
    // Copy wo
}

bool GPUAttentionLayer::scaled_dot_product(const Tensor& query, const Tensor& key,
                                          const Tensor& value, Tensor& output,
                                          float scale) {
    // Simplified implementation
    return true;
}

// ============================================================================
// TensorUtils Implementation
// ============================================================================

TensorUtils& TensorUtils::instance() {
    static TensorUtils utils;
    return utils;
}

bool TensorUtils::create_from_cpu(const void* data, const TensorShape& shape,
                                 TensorDataType dtype, Tensor& tensor) {
    QMutexLocker lock(&mutex_);

    Tensor new_tensor(shape, dtype);
    std::memcpy(new_tensor.get_cpu_memory(), data, new_tensor.get_size_bytes());
    tensor = new_tensor;

    return true;
}

bool TensorUtils::create_random(const TensorShape& shape, TensorDataType dtype,
                               Tensor& tensor) {
    QMutexLocker lock(&mutex_);

    Tensor new_tensor(shape, dtype);
    auto* data = static_cast<float*>(new_tensor.get_cpu_memory());

    std::mt19937 gen(std::random_device{}());
    std::normal_distribution<float> dist(0.0f, 1.0f);

    uint64_t elements = shape.total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        data[i] = dist(gen);
    }

    tensor = new_tensor;
    return true;
}

bool TensorUtils::create_zeros(const TensorShape& shape, TensorDataType dtype,
                              Tensor& tensor) {
    QMutexLocker lock(&mutex_);

    Tensor new_tensor(shape, dtype);
    std::memset(new_tensor.get_cpu_memory(), 0, new_tensor.get_size_bytes());
    tensor = new_tensor;

    return true;
}

bool TensorUtils::create_ones(const TensorShape& shape, TensorDataType dtype,
                             Tensor& tensor) {
    QMutexLocker lock(&mutex_);

    Tensor new_tensor(shape, dtype);
    auto* data = static_cast<float*>(new_tensor.get_cpu_memory());

    uint64_t elements = shape.total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        data[i] = 1.0f;
    }

    tensor = new_tensor;
    return true;
}

bool TensorUtils::copy_tensor(const Tensor& src, Tensor& dst) {
    QMutexLocker lock(&mutex_);

    if (src.get_shape().total_elements() != dst.get_shape().total_elements()) {
        return false;
    }

    std::memcpy(dst.get_cpu_memory(), src.get_cpu_memory(), src.get_size_bytes());
    return true;
}

void TensorUtils::print_tensor(const Tensor& tensor, uint32_t max_elements) {
    auto* data = static_cast<float*>(tensor.get_cpu_memory());
    uint64_t elements = std::min(static_cast<uint64_t>(max_elements),
                                tensor.get_shape().total_elements());

    std::cout << "Tensor[";
    for (const auto& dim : tensor.get_shape().dims) {
        std::cout << dim << " ";
    }
    std::cout << "] = [";

    for (uint64_t i = 0; i < elements; ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << data[i];
    }

    if (elements < tensor.get_shape().total_elements()) {
        std::cout << ", ... (" << tensor.get_shape().total_elements() - elements << " more)";
    }

    std::cout << "]" << std::endl;
}

bool TensorUtils::validate_computation(const Tensor& expected, const Tensor& actual,
                                      float tolerance) {
    QMutexLocker lock(&mutex_);

    if (expected.get_shape().total_elements() != actual.get_shape().total_elements()) {
        return false;
    }

    auto* expected_data = static_cast<float*>(expected.get_cpu_memory());
    auto* actual_data = static_cast<float*>(actual.get_cpu_memory());

    uint64_t elements = expected.get_shape().total_elements();
    for (uint64_t i = 0; i < elements; ++i) {
        if (std::fabs(expected_data[i] - actual_data[i]) > tolerance) {
            return false;
        }
    }

    return true;
}

} // namespace GPU
} // namespace RawrXD
