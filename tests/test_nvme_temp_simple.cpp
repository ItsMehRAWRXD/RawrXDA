// Minimal test for NVMe temperature reading via MASM SMART log query
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// Link against nvme_thermal_stressor_asm.lib
extern "C" {
    int32_t NVMe_GetTemperature(uint32_t driveId);
    int32_t NVMe_PollAllDrives(int32_t* outTemps, const uint32_t* driveIds, uint32_t count);
    uint32_t NVMe_GetLastError();
}

int main() {
    printf("NVMe Temperature Test (SMART Log Page 02h)\n");
    printf("==========================================\n\n");
    
    uint32_t drives[] = {0, 1, 2, 4, 5};
    int32_t temps[5] = {-1, -1, -1, -1, -1};
    
    printf("Testing individual drive queries:\n");
    for (int i = 0; i < 5; i++) {
        int32_t temp = NVMe_GetTemperature(drives[i]);
        temps[i] = temp;
        printf("  Drive %u: ", drives[i]);
        if (temp == -1) {
            printf("FAILED (LastError=%u)\n", NVMe_GetLastError());
        } else {
            printf("%d C\n", temp);
        }
    }
    
    printf("\nTesting batch poll:\n");
    int32_t batchTemps[5];
    int32_t success = NVMe_PollAllDrives(batchTemps, drives, 5);
    printf("  Successful reads: %d/5\n", success);
    for (int i = 0; i < 5; i++) {
        printf("  Drive %u: %d C\n", drives[i], batchTemps[i]);
    }
    
    return 0;
}
