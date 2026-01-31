#pragma once


// Quantization helpers used by inference engine and tests
std::vector<uint8_t> quantize_q8k(const std::vector<uint8_t>& raw);
std::vector<uint8_t> quantize_q4_0(const std::vector<uint8_t>& raw);
std::vector<uint8_t> quantize_generic_bits(const std::vector<uint8_t>& raw, int bits);
std::vector<uint8_t> to_f16(const std::vector<uint8_t>& raw);
std::vector<uint8_t> apply_quant(const std::vector<uint8_t>& raw, const std::string& mode);

// New function: returns both quantized data and GGML type ID
std::pair<std::vector<uint8_t>, int> apply_quant_with_type(const std::vector<uint8_t>& raw, const std::string& mode);

// Unpacking helpers for tests
std::vector<float> unpack_generic_bits(const std::vector<uint8_t>& packed, int bits);
std::vector<float> unpack_f16(const std::vector<uint8_t>& packed);

