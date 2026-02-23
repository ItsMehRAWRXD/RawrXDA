#include <windows.h>
#include <cstdint>

bool hotpatch(void* target, void* replacement) {
    DWORD old;
    if (!VirtualProtect(target,16,PAGE_EXECUTE_READWRITE,&old)) return false;

    uint8_t* p=(uint8_t*)target;
    // JMP rel32 is E9 <offset>
    p[0]=0xE9; 
    // Calculate offset: dest - (src + 5)
    *(int32_t*)(p+1)=(int32_t)((uint8_t*)replacement-(p+5));

    VirtualProtect(target,16,old,&old);
    return true;
}
