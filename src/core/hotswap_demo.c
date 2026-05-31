/* hotswap_demo.c - Weight Hotswap Demonstration */
#include "weight_hotswap.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <model.gguf>\n", argv[0]);
        return 1;
    }
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  RAWRXD Weight Hotswap Demo\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    /* Create hotswap session */
    printf("Loading model: %s\n", argv[1]);
    HotswapSession* session = hotswap_create(argv[1]);
    if (!session) {
        printf("❌ Failed to load model\n");
        return 1;
    }
    
    printf("✅ Model loaded\n");
    printf("   Tensors: %u\n", session->tensor_count);
    printf("   Original size: %.2f MB\n", 
           session->total_original_size / (1024.0 * 1024.0));
    printf("\n");
    
    /* Test profile: Speed */
    printf("Applying SPEED profile (Q4_0)...\n");
    hotswap_apply_profile(session, &QUANT_PROFILE_SPEED);
    
    HotswapMemory mem_speed = hotswap_get_memory(session);
    printf("   Size: %.2f MB (%.1f%% savings)\n",
           mem_speed.current_bytes / (1024.0 * 1024.0),
           mem_speed.savings_percent);
    
    BatchQuality q_speed = hotswap_measure_all(session);
    printf("   Avg SNR: %.2f dB\n", q_speed.avg_snr_db);
    printf("   Acceptable tensors: %u/%u\n",
           q_speed.acceptable_count, q_speed.count);
    printf("\n");
    
    /* Restore original */
    printf("Restoring original...\n");
    hotswap_restore(session);
    
    /* Test profile: Balanced */
    printf("Applying BALANCED profile (Q4_K)...\n");
    hotswap_apply_profile(session, &QUANT_PROFILE_BALANCED);
    
    HotswapMemory mem_balanced = hotswap_get_memory(session);
    printf("   Size: %.2f MB (%.1f%% savings)\n",
           mem_balanced.current_bytes / (1024.0 * 1024.0),
           mem_balanced.savings_percent);
    
    BatchQuality q_balanced = hotswap_measure_all(session);
    printf("   Avg SNR: %.2f dB\n", q_balanced.avg_snr_db);
    printf("   Acceptable tensors: %u/%u\n",
           q_balanced.acceptable_count, q_balanced.count);
    printf("\n");
    
    /* Restore and cleanup */
    hotswap_restore(session);
    hotswap_destroy(session);
    
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("  Demo complete!\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return 0;
}