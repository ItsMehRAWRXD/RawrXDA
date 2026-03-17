/*=============================================================================
 * Pure MASM Crypto Library Header
 * Zero dependencies - OpenSSL API compatible
 *=============================================================================
 */

#ifndef MASM_CRYPTO_H
#define MASM_CRYPTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

/*=============================================================================
 * Random Number Generation (RAND_bytes replacement)
 *=============================================================================*/

/**
 * @brief Initialize RNG
 * @return 0=success, -1=error
 */
int masm_crypto_rand_init(void);

/**
 * @brief Generate random bytes
 * @param buf Buffer to fill with random bytes
 * @param num Number of bytes to generate
 * @return 1=success, 0=failure
 */
int masm_crypto_rand_bytes(unsigned char *buf, int num);

/**
 * @brief Cleanup RNG resources
 */
void masm_crypto_rand_cleanup(void);

/*=============================================================================
 * AES-256-GCM Encryption (EVP_aes_256_gcm replacement)
 *=============================================================================*/

/**
 * @brief AES-256 context structure
 */
typedef struct {
    unsigned char encrypt_key[16 * 15];  /* 15 rounds * 16 bytes */
    unsigned char decrypt_key[16 * 15];  /* 15 rounds * 16 bytes */
    unsigned long long gcm_h;
    unsigned char gcm_iv[12];
    unsigned char gcm_tag[16];
    unsigned long long gcm_aad_len;
    unsigned long long gcm_cipher_len;
} masm_aes256_ctx;

/**
 * @brief Initialize AES-256 context
 * @param ctx AES-256 context
 * @param key 32-byte encryption key
 * @param iv 12-byte initialization vector
 * @return 0=success, -1=error
 */
int masm_crypto_aes256_init(masm_aes256_ctx *ctx, const unsigned char *key, const unsigned char *iv);

/**
 * @brief Encrypt data with AES-256-GCM
 * @param ctx AES-256 context
 * @param plaintext Plaintext data
 * @param plaintext_len Plaintext length
 * @param aad Additional authenticated data
 * @param aad_len AAD length
 * @param ciphertext Output ciphertext buffer
 * @param tag Output authentication tag (16 bytes)
 * @return 0=success, -1=error
 */
int masm_crypto_aes256_encrypt(masm_aes256_ctx *ctx, const unsigned char *plaintext, size_t plaintext_len,
                                 const unsigned char *aad, size_t aad_len,
                                 unsigned char *ciphertext, unsigned char *tag);

/**
 * @brief Decrypt data with AES-256-GCM
 * @param ctx AES-256 context
 * @param ciphertext Ciphertext data
 * @param ciphertext_len Ciphertext length
 * @param aad Additional authenticated data
 * @param aad_len AAD length
 * @param tag Authentication tag (16 bytes)
 * @param plaintext Output plaintext buffer
 * @return 0=success, -1=error (authentication failure)
 */
int masm_crypto_aes256_decrypt(masm_aes256_ctx *ctx, const unsigned char *ciphertext, size_t ciphertext_len,
                                 const unsigned char *aad, size_t aad_len,
                                 const unsigned char *tag, unsigned char *plaintext);

/**
 * @brief Cleanup AES-256 context
 * @param ctx AES-256 context
 */
void masm_crypto_aes256_cleanup(masm_aes256_ctx *ctx);

/*=============================================================================
 * EVP API (OpenSSL compatible)
 *=============================================================================*/

/**
 * @brief EVP cipher context structure (OpenSSL compatible)
 */
typedef struct evp_cipher_ctx_st {
    int cipher_type;           /* Cipher type */
    unsigned char key[32];     /* Encryption key */
    unsigned char iv[16];      /* Initialization vector */
    unsigned char buf[16];     /* Buffer for partial blocks */
    int buf_len;               /* Buffer length */
    int encrypt;               /* 1=encrypt, 0=decrypt */
    int num;                   /* Block counter */
    void *app_data;            /* Application data */
    int key_len;               /* Key length */
    int flags;                 /* Flags */
    int final_used;            /* Final block used */
    int block_mask;            /* Block mask */
    unsigned char final_buf[16];  /* Final buffer */
} EVP_CIPHER_CTX;

/**
 * @brief EVP cipher structure (OpenSSL compatible)
 */
typedef struct evp_cipher_st {
    int nid;                   /* NID (numeric identifier) */
    int block_size;            /* Block size */
    int key_len;               /* Key length */
    int iv_len;                /* IV length */
    unsigned long flags;       /* Flags */
    int (*init)(void *ctx, const unsigned char *key, const unsigned char *iv, int enc);
    int (*do_cipher)(void *ctx, unsigned char *out, const unsigned char *in, unsigned int inl);
    int (*cleanup)(void *ctx);
    int ctx_size;              /* Context size */
    int (*set_asn1_parameters)(void *ctx, void *type);
    int (*get_asn1_parameters)(void *ctx, void *type);
    int (*ctrl)(void *ctx, int type, int arg, void *ptr);
    void *app_data;            /* Application data */
} EVP_CIPHER;

/* EVP control types */
#define EVP_CTRL_GCM_SET_IVLEN  0x12
#define EVP_CTRL_GCM_GET_TAG    0x10
#define EVP_CTRL_GCM_SET_TAG    0x11
#define EVP_CTRL_GCM_SET_TAGLEN 0x13

/**
 * @brief Create new EVP cipher context
 * @return pointer to new context or NULL on error
 */
EVP_CIPHER_CTX *EVP_CIPHER_CTX_new(void);

/**
 * @brief Free EVP cipher context
 * @param ctx context pointer
 */
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX *ctx);

/**
 * @brief Initialize encryption context
 * @param ctx context pointer
 * @param cipher cipher type (must be EVP_aes_256_gcm)
 * @param impl engine implementation (NULL for default)
 * @param key encryption key (32 bytes for AES-256)
 * @param iv initialization vector (12 bytes for GCM)
 * @return 1=success, 0=error
 */
int EVP_EncryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                        void *impl, const unsigned char *key, const unsigned char *iv);

/**
 * @brief Encrypt data
 * @param ctx context pointer
 * @param out output buffer
 * @param outlen output length (returned)
 * @param in input buffer
 * @param inlen input length
 * @return 1=success, 0=error
 */
int EVP_EncryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outlen,
                        const unsigned char *in, int inlen);

/**
 * @brief Finalize encryption
 * @param ctx context pointer
 * @param out output buffer for final block
 * @param outlen output length (returned)
 * @return 1=success, 0=error
 */
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outlen);

/**
 * @brief Initialize decryption context
 * @param ctx context pointer
 * @param cipher cipher type (must be EVP_aes_256_gcm)
 * @param impl engine implementation (NULL for default)
 * @param key decryption key (32 bytes for AES-256)
 * @param iv initialization vector (12 bytes for GCM)
 * @return 1=success, 0=error
 */
int EVP_DecryptInit_ex(EVP_CIPHER_CTX *ctx, const EVP_CIPHER *cipher,
                        void *impl, const unsigned char *key, const unsigned char *iv);

/**
 * @brief Decrypt data
 * @param ctx context pointer
 * @param out output buffer
 * @param outlen output length (returned)
 * @param in input buffer
 * @param inlen input length
 * @return 1=success, 0=error
 */
int EVP_DecryptUpdate(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outlen,
                        const unsigned char *in, int inlen);

/**
 * @brief Finalize decryption
 * @param ctx context pointer
 * @param out output buffer for final block
 * @param outlen output length (returned)
 * @return 1=success, 0=error (authentication failure)
 */
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX *ctx, unsigned char *out, int *outlen);

/**
 * @brief Control cipher context
 * @param ctx context pointer
 * @param type control type (EVP_CTRL_GCM_GET_TAG, etc.)
 * @param arg argument
 * @param ptr pointer to data
 * @return 1=success, 0=error
 */
int EVP_CIPHER_CTX_ctrl(EVP_CIPHER_CTX *ctx, int type, int arg, void *ptr);

/**
 * @brief Get AES-256-GCM cipher
 * @return pointer to cipher structure
 */
const EVP_CIPHER *EVP_aes_256_gcm(void);

/**
 * @brief Generate random bytes
 * @param buf buffer to fill with random bytes
 * @param num number of bytes to generate
 * @return 1=success, 0=failure
 */
int RAND_bytes(unsigned char *buf, int num);

/**
 * @brief Cleanup RNG resources
 */
void RAND_cleanup(void);

/*=============================================================================
 * Convenience macros (OpenSSL compatible)
 *=============================================================================*/

#define EVP_MAX_BLOCK_LENGTH 32
#define EVP_MAX_IV_LENGTH 16
#define EVP_MAX_KEY_LENGTH 64

/*=============================================================================
 * Error handling
 *=============================================================================*/

/**
 * @brief Get last error code
 * @return error code
 */
unsigned long ERR_get_error(void);

/**
 * @brief Get error string
 * @param e error code
 * @param buf buffer for error string
 * @param len buffer length
 * @return pointer to error string
 */
char *ERR_error_string(unsigned long e, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* MASM_CRYPTO_H */