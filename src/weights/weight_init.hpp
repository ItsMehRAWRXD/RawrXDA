#pragma once

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace weights
{

enum class InitMethod
{
    Zeros,
    Ones,
    Constant,
    RandomNormal,
    RandomUniform,
    XavierNormal,
    XavierUniform,
    KaimingNormal,
    KaimingUniform,
    Orthogonal,
    FromFile,
    FromSafetensors,
};

struct InitConfig
{
    InitMethod method = InitMethod::RandomNormal;
    float constant_value = 0.0f;
    float mean = 0.0f;
    float std = 1.0f;
    float min_val = -1.0f;
    float max_val = 1.0f;
    float gain = 1.0f;
    bool fan_in = true;
    float orthogonal_gain = 1.0f;
    std::string file_path;
    std::string tensor_name;
    unsigned int seed = 0;
};

struct TensorShape
{
    std::vector<int64_t> dims;

    int64_t size() const
    {
        int64_t s = 1;
        for (int64_t d : dims)
            s *= d;
        return s;
    }

    size_t ndim() const { return dims.size(); }

    int64_t dim(int idx) const
    {
        if (dims.empty())
            return 0;
        if (idx < 0)
            idx += (int)dims.size();
        if (idx < 0 || (size_t)idx >= dims.size())
            return 0;
        return dims[(size_t)idx];
    }

    std::pair<int64_t, int64_t> calculate_fan() const
    {
        if (dims.size() < 2)
            return {1, 1};
        int64_t fan_in = dims[dims.size() - 1];
        int64_t fan_out = dims[dims.size() - 2];
        if (dims.size() > 2)
        {
            int64_t rf = 1;
            for (size_t i = 0; i + 2 < dims.size(); ++i)
                rf *= dims[i];
            fan_in *= rf;
            fan_out *= rf;
        }
        return {fan_in, fan_out};
    }
};

class WeightInitializer
{
  public:
    WeightInitializer();

    bool initialize(float* data, const TensorShape& shape, const InitConfig& cfg);
    bool load_from_safetensors(float* data, const TensorShape& shape, const std::string& path,
                               const std::string& tensor_name);
    bool load_from_file(float* data, const TensorShape& shape, const std::string& path);

    const std::string& last_error() const { return m_last_error; }

  private:
    void init_zeros_(float* data, int64_t size);
    void init_ones_(float* data, int64_t size);
    void init_constant_(float* data, int64_t size, float v);
    void init_random_normal_(float* data, int64_t size, float mean, float stddev, unsigned int seed);
    void init_random_uniform_(float* data, int64_t size, float a, float b, unsigned int seed);
    void init_xavier_normal_(float* data, const TensorShape& shape, float gain, unsigned int seed);
    void init_xavier_uniform_(float* data, const TensorShape& shape, float gain, unsigned int seed);
    void init_kaiming_normal_(float* data, const TensorShape& shape, float gain, bool fan_in, unsigned int seed);
    void init_kaiming_uniform_(float* data, const TensorShape& shape, float gain, bool fan_in, unsigned int seed);
    void init_orthogonal_(float* data, const TensorShape& shape, float gain, unsigned int seed);

    std::mt19937 m_rng;
    std::string m_last_error;
};

}  // namespace weights
