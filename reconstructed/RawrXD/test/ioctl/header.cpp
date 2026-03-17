#include "RawrXD_3GAPI_IOCTL.h"
#include <windows.h>
#include <iostream>

// Simple test to validate IOCTL header compilation
int main() {
    std::cout << "RawrXD 3GAPI IOCTL Header Test" << std::endl;

    // Test structure sizes
    std::cout << "TENSOR_DESCRIPTOR size: " << sizeof(TENSOR_DESCRIPTOR) << std::endl;
    std::cout << "INIT_TENSOR_ENGINE_IN size: " << sizeof(INIT_TENSOR_ENGINE_IN) << std::endl;
    std::cout << "ALLOCATE_TENSOR_IN size: " << sizeof(ALLOCATE_TENSOR_IN) << std::endl;

    // Test IOCTL codes
    std::cout << "IOCTL_RAWRXD_INIT_TENSOR_ENGINE: 0x" << std::hex << IOCTL_RAWRXD_INIT_TENSOR_ENGINE << std::endl;
    std::cout << "IOCTL_RAWRXD_ALLOCATE_TENSOR: 0x" << std::hex << IOCTL_RAWRXD_ALLOCATE_TENSOR << std::endl;

    std::cout << "Header compilation successful!" << std::endl;
    return 0;
}