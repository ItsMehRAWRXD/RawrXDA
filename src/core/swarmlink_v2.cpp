#include "swarmlink_v2.hpp"
#include <iostream>

SwarmLink_RingBuffer::SwarmLink_RingBuffer(const wchar_t* filePath, uint32_t size) : chunkSize(size) {
    // Create File with NO_BUFFERING for direct NVMe access and OVERLAPPED for async
    hFile = CreateFileW(
        filePath,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        // Handle error
        return;
    }

    // Allocate page-aligned memory for NO_BUFFERING requirements
    buffer = VirtualAlloc(NULL, chunkSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    // Create IOCP
    hIOCP = CreateIoCompletionPort(hFile, NULL, 0, 0);
}

SwarmLink_RingBuffer::~SwarmLink_RingBuffer() {
    if (buffer) VirtualFree(buffer, 0, MEM_RELEASE);
    if (hFile != INVALID_HANDLE_VALUE) CloseHandle(hFile);
    if (hIOCP) CloseHandle(hIOCP);
}

bool SwarmLink_RingBuffer::LoadChunkAsync(uint64_t offset, OVERLAPPED* overlapped) {
    overlapped->Offset = (DWORD)(offset & 0xFFFFFFFF);
    overlapped->OffsetHigh = (DWORD)(offset >> 32);

    DWORD bytesRead;
    BOOL bResult = ReadFile(hFile, buffer, chunkSize, &bytesRead, overlapped);
    
    // ERROR_IO_PENDING is expected for true async operations
    return (bResult || GetLastError() == ERROR_IO_PENDING);
}

void SwarmLink_RingBuffer::WaitAndProcessIOCP() {
    DWORD bytesTransferred;
    ULONG_PTR completionKey;
    OVERLAPPED* pOverlapped;

    BOOL result = GetQueuedCompletionStatus(
        hIOCP,
        &bytesTransferred,
        &completionKey,
        &pOverlapped,
        INFINITE
    );
    
    // IOCP chunk loading processing complete
}

extern "C" void SwarmLink_UpdateTensor(ggml_tensor* tensor, void* new_data_ptr) {
    if (tensor) {
        // Hot-swap the underlying GGML tensor pointer safely
        InterlockedExchangePointer(&tensor->data, new_data_ptr);
    }
}
