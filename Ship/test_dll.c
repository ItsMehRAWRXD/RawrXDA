// Test loader for RawrXD_NativeModelBridge.dll
#include <windows.h>
#include <stdio.h>

typedef int (*LoadModelNative_t)(void* pCtx, const char* path);
typedef int (*UnloadModelNative_t)(void* pCtx);
typedef int (*ForwardPass_t)(void* pCtx, int* tokens, int numTokens, int* output);
typedef int (*DequantizeRow_Q4_0_t)(void* pSrc, float* pDst, int numElements);
typedef int (*SoftMax_SSE_t)(float* pLogits, float* pOutput, int vocabSize);
typedef int (*SampleToken_Argmax_t)(float* pProbs, int vocabSize);

int main() {
    printf("=== RawrXD Native Bridge DLL Test ===\n\n");
    
    HMODULE hDll = LoadLibraryA("RawrXD_NativeModelBridge.dll");
    if (!hDll) {
        printf("ERROR: Failed to load DLL. Error: %lu\n", GetLastError());
        return 1;
    }
    printf("[OK] DLL loaded successfully\n");
    
    // Get function pointers
    LoadModelNative_t LoadModelNative = (LoadModelNative_t)GetProcAddress(hDll, "LoadModelNative");
    UnloadModelNative_t UnloadModelNative = (UnloadModelNative_t)GetProcAddress(hDll, "UnloadModelNative");
    ForwardPass_t ForwardPass = (ForwardPass_t)GetProcAddress(hDll, "ForwardPass");
    DequantizeRow_Q4_0_t DequantizeRow_Q4_0 = (DequantizeRow_Q4_0_t)GetProcAddress(hDll, "DequantizeRow_Q4_0");
    SoftMax_SSE_t SoftMax_SSE = (SoftMax_SSE_t)GetProcAddress(hDll, "SoftMax_SSE");
    SampleToken_Argmax_t SampleToken_Argmax = (SampleToken_Argmax_t)GetProcAddress(hDll, "SampleToken_Argmax");
    
    printf("[OK] LoadModelNative: %p\n", LoadModelNative);
    printf("[OK] UnloadModelNative: %p\n", UnloadModelNative);
    printf("[OK] ForwardPass: %p\n", ForwardPass);
    printf("[OK] DequantizeRow_Q4_0: %p\n", DequantizeRow_Q4_0);
    printf("[OK] SoftMax_SSE: %p\n", SoftMax_SSE);
    printf("[OK] SampleToken_Argmax: %p\n", SampleToken_Argmax);
    
    // Test SoftMax
    printf("\n--- Testing SoftMax ---\n");
    float logits[5] = {1.0f, 2.0f, 3.0f, 2.0f, 1.0f};
    float probs[5] = {0};
    
    if (SoftMax_SSE) {
        int result = SoftMax_SSE(logits, probs, 5);
        printf("SoftMax result: %d\n", result);
        printf("Probabilities: [");
        for (int i = 0; i < 5; i++) {
            printf("%.4f%s", probs[i], i < 4 ? ", " : "");
        }
        printf("]\n");
    }
    
    // Test Argmax
    printf("\n--- Testing Argmax ---\n");
    if (SampleToken_Argmax) {
        float test_probs[5] = {0.1f, 0.2f, 0.5f, 0.15f, 0.05f};
        int token = SampleToken_Argmax(test_probs, 5);
        printf("Argmax of [0.1, 0.2, 0.5, 0.15, 0.05] = %d (expected: 2)\n", token);
    }
    
    FreeLibrary(hDll);
    printf("\n[OK] DLL unloaded\n");
    printf("=== Test Complete ===\n");
    return 0;
}
