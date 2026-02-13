#include "crypto.h"
#include "aes.h"
void TinyHome::Crypto::aes128_enc(const uint8_t* key, const uint8_t* in, uint8_t* out) {
    aes32_encrypt(key, in, out);
}
