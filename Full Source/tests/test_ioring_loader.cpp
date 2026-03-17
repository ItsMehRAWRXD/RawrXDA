// tests/test_ioring_loader.cpp
#include <iostream>
#include <vector>
#include <cassert>
#include "io/backend_interface.hpp"

#include "logging/logger.h"
static Logger s_logger("test_ioring_loader");

// v1.1.0 IORing Backend Unit Test
// Verifies kernel-bypass read logic

int main() {
    s_logger.info("--- RAWRXD v1.1.0 IORing Test ---");

#ifdef _WIN32
    auto backend = CreateIOBackend(IOBackendType::IORING_WINDOWS);
    if (!backend) {
        s_logger.error( "Failed to create Windows IORing backend" << std::endl;
        return 1;
    }

    // Use ourselves as a test file
    if (!backend->Initialize("test_ioring_loader.exe")) {
        // If not found, try common files
        if (!backend->Initialize("C:\\Windows\\System32\\kernel32.dll")) {
            s_logger.error( "Failed to initialize backend with test file" << std::endl;
            return 1;
        }
    }

    // Allocate aligned buffer (4K aligned for Direct IO)
    void* buffer = _aligned_malloc(4096, 4096);
    if (!backend->RegisterBuffers(buffer, 4096, 1)) {
        s_logger.error( "Failed to register buffers" << std::endl;
        return 1;
    }

    s_logger.info("✓ Backend initialized and buffers registered");

    // Submit a 4K read from start of file
    IORequest req;
    req.file_offset = 0;
    req.zone_index = 0;
    req.zone_offset = 0;
    req.size = 4096;
    req.request_id = 999;

    backend->SubmitRead(req);
    backend->Flush();
    s_logger.info("✓ Read request submitted and flushed");

    // Poll for completion
    std::vector<IOCompletion> completions;
    int timeout = 1000000;
    while (completions.empty() && timeout-- > 0) {
        backend->PollCompletions(completions);
    }

    if (completions.empty()) {
        s_logger.error( "Timeout waiting for completion" << std::endl;
        return 1;
    }

    s_logger.info("✓ Completion received! ID: ");

    if (completions[0].result_code != 0) {
        s_logger.error( "Read failed with HRESULT: 0x" << std::hex << (uint32_t)completions[0].result_code << std::dec << std::endl;
        return 1;
    }

    backend->Shutdown();
    delete backend;
    _aligned_free(buffer);

    s_logger.info("--- TEST PASSED (v1.1.0 IORing) ---");
    return 0;
#else
    s_logger.info("Skipping IORing test on non-Windows platform (v1.1.0 is Windows-first)");
    return 0;
#endif
}
