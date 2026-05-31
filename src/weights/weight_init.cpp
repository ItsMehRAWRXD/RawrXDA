#include "weight_init.hpp"

#include "safetensors.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <vector>

namespace weights
{

WeightInitializer::WeightInitializer()
{
    std::random_device rd;
    m_rng.seed(rd());
}

bool WeightInitializer::initialize(float* data, const TensorShape& shape, const InitConfig& cfg)
{
    m_last_error.clear();
    if (!data)
    {
        m_last_error = "null data";
        return false;
    }
    const int64_t sz = shape.size();
    if (sz <= 0)
    {
        m_last_error = "empty tensor";
        return false;
    }

    switch (cfg.method)
    {
        case InitMethod::Zeros:
            init_zeros_(data, sz);
            return true;
        case InitMethod::Ones:
            init_ones_(data, sz);
            return true;
        case InitMethod::Constant:
            init_constant_(data, sz, cfg.constant_value);
            return true;
        case InitMethod::RandomNormal:
            init_random_normal_(data, sz, cfg.mean, cfg.std, cfg.seed);
            return true;
        case InitMethod::RandomUniform:
            init_random_uniform_(data, sz, cfg.min_val, cfg.max_val, cfg.seed);
            return true;
        case InitMethod::XavierNormal:
            init_xavier_normal_(data, shape, cfg.gain, cfg.seed);
            return true;
        case InitMethod::XavierUniform:
            init_xavier_uniform_(data, shape, cfg.gain, cfg.seed);
            return true;
        case InitMethod::KaimingNormal:
            init_kaiming_normal_(data, shape, cfg.gain, cfg.fan_in, cfg.seed);
            return true;
        case InitMethod::KaimingUniform:
            init_kaiming_uniform_(data, shape, cfg.gain, cfg.fan_in, cfg.seed);
            return true;
        case InitMethod::Orthogonal:
            init_orthogonal_(data, shape, cfg.orthogonal_gain, cfg.seed);
            return true;
        case InitMethod::FromSafetensors:
            return load_from_safetensors(data, shape, cfg.file_path, cfg.tensor_name);
        case InitMethod::FromFile:
            return load_from_file(data, shape, cfg.file_path);
        default:
            m_last_error = "unknown init method";
            return false;
    }
}

bool WeightInitializer::load_from_file(float* data, const TensorShape& shape, const std::string& path)
{
    m_last_error.clear();
    const int64_t sz = shape.size();
    if (!data || sz <= 0)
        return false;

    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
    {
        m_last_error = "failed to open: " + path;
        return false;
    }

    // Expect raw float32 payload matching tensor size.
    f.seekg(0, std::ios::end);
    const std::streamoff end = f.tellg();
    f.seekg(0, std::ios::beg);
    const size_t bytes = (size_t)end;
    const size_t expect = (size_t)sz * sizeof(float);
    if (bytes != expect)
    {
        m_last_error = "file size mismatch (expected raw f32 tensor)";
        return false;
    }
    f.read(reinterpret_cast<char*>(data), (std::streamsize)expect);
    if (!f)
    {
        m_last_error = "failed to read tensor bytes";
        return false;
    }
    return true;
}

static inline float f16_to_f32_(uint16_t h)
{
    const uint32_t sign = (uint32_t)(h >> 15) << 31;
    uint32_t exp = (h >> 10) & 0x1Fu;
    uint32_t mant = h & 0x3FFu;
    uint32_t v;
    if (exp == 0)
    {
        if (mant == 0)
        {
            v = sign;
        }
        else
        {
            // denorm
            while ((mant & 0x400u) == 0)
            {
                mant <<= 1;
                --exp;
            }
            mant &= 0x3FFu;
            exp = (exp + 127 - 15);
            v = sign | (exp << 23) | (mant << 13);
        }
    }
    else if (exp == 31)
    {
        v = sign | 0x7F800000u | (mant << 13);
    }
    else
    {
        exp = exp + 127 - 15;
        v = sign | (exp << 23) | (mant << 13);
    }
    float f;
    std::memcpy(&f, &v, sizeof(f));
    return f;
}

bool WeightInitializer::load_from_safetensors(float* data, const TensorShape& shape, const std::string& path,
                                              const std::string& tensor_name)
{
    m_last_error.clear();
    SafetensorsReader r;
    if (!r.load(path))
    {
        m_last_error = r.last_error();
        return false;
    }
    const TensorMeta* meta = r.get_tensor_meta(tensor_name);
    if (!meta)
    {
        m_last_error = "tensor not found: " + tensor_name;
        return false;
    }
    if (meta->shape.size() != shape.dims.size())
    {
        m_last_error = "shape rank mismatch";
        return false;
    }
    for (size_t i = 0; i < meta->shape.size(); ++i)
    {
        if (meta->shape[i] != shape.dims[i])
        {
            m_last_error = "shape mismatch";
            return false;
        }
    }
    const uint8_t* ptr = r.get_tensor_data_ptr(tensor_name);
    if (!ptr)
    {
        m_last_error = "tensor data missing";
        return false;
    }
    const int64_t elems = shape.size();
    if (meta->dtype == "F32")
    {
        const size_t need = (size_t)elems * sizeof(float);
        if (meta->size < need)
        {
            m_last_error = "tensor data too small";
            return false;
        }
        std::memcpy(data, ptr, need);
        return true;
    }
    if (meta->dtype == "F16" || meta->dtype == "BF16")
    {
        const size_t need = (size_t)elems * sizeof(uint16_t);
        if (meta->size < need)
        {
            m_last_error = "tensor data too small";
            return false;
        }
        const uint16_t* h = reinterpret_cast<const uint16_t*>(ptr);
        for (int64_t i = 0; i < elems; ++i)
        {
            data[i] = f16_to_f32_(h[(size_t)i]);
        }
        return true;
    }
    m_last_error = "unsupported dtype: " + meta->dtype;
    return false;
}

void WeightInitializer::init_zeros_(float* data, int64_t size)
{
    std::memset(data, 0, (size_t)size * sizeof(float));
}

void WeightInitializer::init_ones_(float* data, int64_t size)
{
    for (int64_t i = 0; i < size; ++i)
        data[i] = 1.0f;
}

void WeightInitializer::init_constant_(float* data, int64_t size, float v)
{
    for (int64_t i = 0; i < size; ++i)
        data[i] = v;
}

void WeightInitializer::init_random_normal_(float* data, int64_t size, float mean, float stddev, unsigned int seed)
{
    std::mt19937 gen(seed ? seed : (unsigned int)std::random_device{}());
    std::normal_distribution<float> dist(mean, stddev);
    for (int64_t i = 0; i < size; ++i)
        data[i] = dist(gen);
}

void WeightInitializer::init_random_uniform_(float* data, int64_t size, float a, float b, unsigned int seed)
{
    std::mt19937 gen(seed ? seed : (unsigned int)std::random_device{}());
    std::uniform_real_distribution<float> dist(a, b);
    for (int64_t i = 0; i < size; ++i)
        data[i] = dist(gen);
}

void WeightInitializer::init_xavier_normal_(float* data, const TensorShape& shape, float gain, unsigned int seed)
{
    auto fan = shape.calculate_fan();
    const float stddev = gain * std::sqrt(2.0f / (float)(fan.first + fan.second));
    init_random_normal_(data, shape.size(), 0.0f, stddev, seed);
}

void WeightInitializer::init_xavier_uniform_(float* data, const TensorShape& shape, float gain, unsigned int seed)
{
    auto fan = shape.calculate_fan();
    const float bound = gain * std::sqrt(6.0f / (float)(fan.first + fan.second));
    init_random_uniform_(data, shape.size(), -bound, bound, seed);
}

void WeightInitializer::init_kaiming_normal_(float* data, const TensorShape& shape, float gain, bool fan_in,
                                             unsigned int seed)
{
    auto fan = shape.calculate_fan();
    const int64_t f = fan_in ? fan.first : fan.second;
    const float stddev = gain / std::sqrt((float)f);
    init_random_normal_(data, shape.size(), 0.0f, stddev, seed);
}

void WeightInitializer::init_kaiming_uniform_(float* data, const TensorShape& shape, float gain, bool fan_in,
                                              unsigned int seed)
{
    auto fan = shape.calculate_fan();
    const int64_t f = fan_in ? fan.first : fan.second;
    const float bound = gain * std::sqrt(3.0f / (float)f);
    init_random_uniform_(data, shape.size(), -bound, bound, seed);
}

void WeightInitializer::init_orthogonal_(float* data, const TensorShape& shape, float gain, unsigned int seed)
{
    // Deterministic, dependency-free Gram-Schmidt over rows (works for 2D).
    if (shape.ndim() < 2)
    {
        init_random_normal_(data, shape.size(), 0.0f, gain, seed);
        return;
    }
    const int64_t rows = shape.dim(0);
    const int64_t cols = shape.dim(1);
    if (rows <= 0 || cols <= 0)
    {
        m_last_error = "invalid orthogonal shape";
        return;
    }

    std::vector<float> m((size_t)rows * (size_t)cols);
    init_random_normal_(m.data(), (int64_t)m.size(), 0.0f, 1.0f, seed);

    // Orthonormalize rows.
    for (int64_t r = 0; r < rows; ++r)
    {
        float* vr = m.data() + (size_t)r * (size_t)cols;
        // subtract projections
        for (int64_t k = 0; k < r; ++k)
        {
            const float* vk = m.data() + (size_t)k * (size_t)cols;
            float dot = 0.0f;
            for (int64_t c = 0; c < cols; ++c)
                dot += vr[c] * vk[c];
            for (int64_t c = 0; c < cols; ++c)
                vr[c] -= dot * vk[c];
        }
        float norm = 0.0f;
        for (int64_t c = 0; c < cols; ++c)
            norm += vr[c] * vr[c];
        norm = std::sqrt(norm);
        if (norm < 1e-8f)
            continue;
        const float inv = gain / norm;
        for (int64_t c = 0; c < cols; ++c)
            vr[c] *= inv;
    }

    // Write out (truncate/pad if shape not exactly rows*cols contiguous desired).
    std::memcpy(data, m.data(), (size_t)std::min<int64_t>(shape.size(), (int64_t)m.size()) * sizeof(float));
}

}  // namespace weights
