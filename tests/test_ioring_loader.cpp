// tests/test_ioring_loader.cpp
#include <iostream>
#include <vector>
#include <cassert>
#include "io/backend_interface.hpp"

// v1.1.0 IORing Backend Unit Test
// Verifies kernel-bypass read logic

int main() {
    std::cout << "--- RAWRXD v1.1.0 IORing Test ---" << std::endl;

#ifdef _WIN32
    auto backend = CreateIOBackend(IOBackendType::IORING_WINDOWS);
    if (!backend) {
        std::cerr << "Failed to create Windows IORing backend" << std::endl;
        return 1;
    }

    // Use ourselves as a test file
    if (!backend->Initialize("test_ioring_loader.exe")) {
        // If not found, try common files
        if (!backend->Initialize("C:\\Windows\\System32\\kernel32.dll")) {
            std::cerr << "Failed to initialize backend with test file" << std::endl;
            return 1;
        }
    }

    // Allocate aligned buffer (4K aligned for Direct IO)
    void* buffer = _aligned_malloc(4096, 4096);
    if (!backend->RegisterBuffers(buffer, 4096, 1)) {
        std::cerr << "Failed to register buffers" << std::endl;
        return 1;
    }

    std::cout << "✓ Backend initialized and buffers registered" << std::endl;

    // Submit a 4K read from start of file
    IORequest req;
    req.file_offset = 0;
    req.zone_index = 0;
    req.zone_offset = 0;
    req.size = 4096;
    req.request_id = 999;

    backend->SubmitRead(req);
    backend->Flush();
    std::cout << "✓ Read request submitted and flushed" << std::endl;

    // Poll for completion
    std::vector<IOCompletion> completions;
    int timeout = 1000000;
    while (completions.empty() && timeout-- > 0) {
        backend->PollCompletions(completions);
    }

    if (completions.empty()) {
        std::cerr << "Timeout waiting for completion" << std::endl;
        return 1;
    }

    std::cout << "✓ Completion received! ID: " << completions[0].request_id 
              << ", Result: " << completions[0].result_code << std::endl;

    if (completions[0].result_code != 0) {
        std::cerr << "Read failed with HRESULT: 0x" << std::hex << (uint32_t)completions[0].result_code << std::dec << std::endl;
        return 1;
    }

    backend->Shutdown();
    delete backend;
    _aligned_free(buffer);

    std::cout << "--- TEST PASSED (v1.1.0 IORing) ---" << std::endl;
    return 0;
#else
    std::cout << "Skipping IORing test on non-Windows platform (v1.1.0 is Windows-first)" << std::endl;
    return 0;
#endif
}
