#include "TemporalKernelVersioning.h"
#include <windows.h>

uint64_t TemporalKernelVersioning::GetNow() {
    return GetTickCount64();
}
