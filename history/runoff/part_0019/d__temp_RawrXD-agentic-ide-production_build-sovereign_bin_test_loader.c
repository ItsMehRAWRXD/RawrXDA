#include "../../src/sovereign_loader.h"
#include <stdio.h>

int main(void) {
    printf("Testing RawrXD Sovereign Loader...\n");

    if (sovereign_loader_init(64, 16384) != 0) {
        printf("Failed to initialize loader\n");
        return 1;
    }

    LoaderMetrics metrics = {0};
    sovereign_loader_get_metrics(&metrics);
    printf("Loader ready. Models loaded: %llu\n", (unsigned long long)metrics.models_loaded);

    uint64_t model_size = 0;
    const char* model_path = "D:/models/phi-3-mini-4k-instruct-q4.gguf"; // adjust to an existing GGUF
    void* model = sovereign_loader_load_model(model_path, &model_size);

    if (model) {
        printf("SUCCESS: Loaded model (%.2f MB)\n", model_size / 1024.0 / 1024.0);
        sovereign_loader_unload_model(model);
    } else {
        printf("Load failed (check path): %s\n", model_path);
    }

    sovereign_loader_shutdown();
    return 0;
}
