/**
 * test_inference.c - Quick test harness for inference_client.dll
 * Compile: gcc -O2 -o test_inference.exe test_inference.c -L../bin -linference_client
 *     or:  gcc -O2 -o test_inference.exe test_inference.c ../bin/inference_client.dll
 */
#include <stdio.h>
#include <windows.h>
#include "inference_client.h"

/* Token callback for streaming test */
static int __stdcall on_token(const char* token, int len, void* user_data) {
    (void)user_data;
    /* Print token immediately (simulates AppendTokenToChat) */
    fwrite(token, 1, len, stdout);
    fflush(stdout);
    return 0; /* continue */
}

int main(void) {
    printf("=== Inference Client Test ===\n\n");

    int rc = Infer_Init();
    if (rc != INFER_OK) {
        printf("Infer_Init failed: %d\n", rc);
        return 1;
    }

    InferConfig cfg;
    Infer_DefaultConfig(&cfg);
    cfg.max_tokens = 32;

    /* --- Test 1: Blocking completion --- */
    printf("--- Test 1: Blocking Completion ---\n");
    InferResult result;
    rc = Infer_Complete("What is 2+2? One word.", &cfg, &result);
    printf("Status: %d\n", result.status);
    printf("Response: %s\n", result.text);
    printf("Prompt tokens: %d, Completion tokens: %d\n",
           result.prompt_tokens, result.completion_tokens);
    printf("Elapsed: %.2f ms\n\n", result.elapsed_us / 1000.0);

    /* --- Test 2: Streaming --- */
    printf("--- Test 2: Streaming ---\n");
    printf("Tokens: ");
    InferResult stream_result;
    rc = Infer_Stream("Write a haiku about assembly language.", &cfg, on_token, NULL, &stream_result);
    printf("\nStatus: %d\n", stream_result.status);
    printf("Total tokens: %d\n", stream_result.completion_tokens);
    printf("Elapsed: %.2f ms\n", stream_result.elapsed_us / 1000.0);
    printf("Accumulated: %s\n", stream_result.text);

    Infer_Shutdown();
    printf("\n=== Done ===\n");
    return 0;
}
