#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// From MASM
extern "C" int32_t NVMe_GetTemperature(uint32_t driveId);

int main() {
    printf("NVMe Temperature Test (SMART Log Page 02h)\n");
    printf("==========================================\n\n");
    
    uint32_t drives[] = {0, 1, 2, 4, 5};
    for (int i = 0; i < 5; i++) {
        int32_t temp = NVMe_GetTemperature(drives[i]);
        printf("Drive %u: ", drives[i]);
        if (temp == -1) {
            printf("FAILED (error or access denied)\n");
        } else {
            printf("%d C\n", temp);
        }
    }
    
    return 0;
}
