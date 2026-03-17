#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int RawrXD_StreamSetCallback(void* callbackFn, void* callbackCtx, uint32_t beaconSlot);
int RawrXD_StreamGenerate(const char* model,
                          const char* prompt,
                          const char* endpoint,
                          uint32_t timeoutMs);

#ifdef __cplusplus
}
#endif
