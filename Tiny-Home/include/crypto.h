#pragma once
#include <cstdint>
namespace TinyHome::Crypto { void aes128_enc(const uint8_t* key, const uint8_t* in, uint8_t* out); }
