/* bcrypt_test.c — Minimal BCrypt AES-256-GCM test (same calls as carmilla_x64.asm)
 * Build:  cl /nologo bcrypt_test.c bcrypt.lib
 * Run:    bcrypt_test.exe
 */
#include <windows.h>
#include <bcrypt.h>
#include <stdio.h>

#pragma comment(lib, "bcrypt.lib")

#define CHK(fn, ...) do { \
    NTSTATUS _s = fn(__VA_ARGS__); \
    if (_s != 0) { printf("[FAIL] %s => 0x%08X\n", #fn, (unsigned)_s); return 1; } \
    else         { printf("[OK]   %s\n", #fn); } \
} while(0)

int main(void)
{
    BCRYPT_ALG_HANDLE hAES = NULL, hHMAC = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;

    /* 1. Open AES provider */
    CHK(BCryptOpenAlgorithmProvider, &hAES, BCRYPT_AES_ALGORITHM, NULL, 0);

    /* 2. Set chaining mode to GCM */
    CHK(BCryptSetProperty, hAES,
        BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
        sizeof(BCRYPT_CHAIN_MODE_GCM), 0);

    /* 3. Open HMAC-SHA512 provider (for PBKDF2) */
    CHK(BCryptOpenAlgorithmProvider, &hHMAC, BCRYPT_SHA512_ALGORITHM, NULL,
        BCRYPT_ALG_HANDLE_HMAC_FLAG);

    /* 4. PBKDF2 */
    UCHAR salt[32], derivedKey[32];
    memset(salt, 0x41, 32);
    CHK(BCryptDeriveKeyPBKDF2, hHMAC,
        (PUCHAR)"test-pass", 9,
        salt, 32,
        100000,
        derivedKey, 32, 0);

    /* 5. Query BCRYPT_OBJECT_LENGTH */
    DWORD objLen = 0, cbResult = 0;
    CHK(BCryptGetProperty, hAES, BCRYPT_OBJECT_LENGTH,
        (PUCHAR)&objLen, sizeof(objLen), &cbResult, 0);
    printf("       ObjectLength = %u\n", objLen);

    /* 6. Generate symmetric key */
    PUCHAR pKeyObj = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, objLen);
    CHK(BCryptGenerateSymmetricKey, hAES, &hKey,
        pKeyObj, objLen,
        derivedKey, 32, 0);

    /* 7. Encrypt with GCM */
    UCHAR iv[12]  = {1,2,3,4,5,6,7,8,9,10,11,12};
    UCHAR tag[16] = {0};
    UCHAR plain[256], cipher[256];
    memset(plain, 0xBB, 256);

    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce  = iv;
    authInfo.cbNonce  = 12;
    authInfo.pbTag    = tag;
    authInfo.cbTag    = 16;

    DWORD cbCipher = 0;
    CHK(BCryptEncrypt, hKey,
        plain, 256,
        &authInfo,
        NULL, 0,        /* pbIV, cbIV — NULL for GCM (nonce in authInfo) */
        cipher, 256,
        &cbCipher, 0);
    printf("       cbCipher = %u\n", cbCipher);

    /* 8. Decrypt */
    UCHAR decrypted[256] = {0};
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo2;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo2);
    authInfo2.pbNonce = iv;
    authInfo2.cbNonce = 12;
    authInfo2.pbTag   = tag;
    authInfo2.cbTag   = 16;

    /* Need a fresh key for decrypt (GCM is one-shot per key handle) */
    BCRYPT_KEY_HANDLE hKey2 = NULL;
    PUCHAR pKeyObj2 = (PUCHAR)HeapAlloc(GetProcessHeap(), 0, objLen);
    CHK(BCryptGenerateSymmetricKey, hAES, &hKey2,
        pKeyObj2, objLen,
        derivedKey, 32, 0);

    DWORD cbPlain = 0;
    CHK(BCryptDecrypt, hKey2,
        cipher, 256,
        &authInfo2,
        NULL, 0,
        decrypted, 256,
        &cbPlain, 0);

    /* Verify */
    if (memcmp(plain, decrypted, 256) == 0)
        printf("[OK]   Round-trip verified!\n");
    else
        printf("[FAIL] Decrypted data does not match!\n");

    /* Cleanup */
    BCryptDestroyKey(hKey);
    BCryptDestroyKey(hKey2);
    HeapFree(GetProcessHeap(), 0, pKeyObj);
    HeapFree(GetProcessHeap(), 0, pKeyObj2);
    BCryptCloseAlgorithmProvider(hAES, 0);
    BCryptCloseAlgorithmProvider(hHMAC, 0);

    printf("\nsizeof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO) = %zu\n",
           sizeof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO));
    printf("Offsets:\n");
    printf("  cbSize       @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbSize));
    printf("  dwInfoVersion@ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, dwInfoVersion));
    printf("  pbNonce      @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, pbNonce));
    printf("  cbNonce      @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbNonce));
    printf("  pbAuthData   @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, pbAuthData));
    printf("  cbAuthData   @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbAuthData));
    printf("  pbTag        @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, pbTag));
    printf("  cbTag        @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbTag));
    printf("  pbMacContext @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, pbMacContext));
    printf("  cbMacContext @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbMacContext));
    printf("  cbAAD        @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbAAD));
    printf("  cbData       @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, cbData));
    printf("  dwFlags      @ %zu\n", offsetof(BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO, dwFlags));

    return 0;
}
