// src/io/direct_io_ring_win.hpp
#pragma once
#include "backend_interface.hpp"
#include <windows.h>
#include <ioringapi.h>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <iostream>

// v1.1.0 Windows IORing Implementation
// Direct NVMe-to-RAM DMA Bypass

class DirectIORingWindows : public IDirectIOBackend {
public:
    DirectIORingWindows() : hRing_(nullptr), hFile_(INVALID_HANDLE_VALUE) {}
    virtual ~DirectIORingWindows() { Shutdown(); }

    bool Initialize(const char* filepath) override {
        // Open with FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED for Direct I/O
        hFile_ = CreateFileA(filepath, 
                             GENERIC_READ, 
                             FILE_SHARE_READ, 
                             nullptr, 
                             OPEN_EXISTING, 
                             FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 
                             nullptr);

        if (hFile_ == INVALID_HANDLE_VALUE) return false;

        // Initialize IORing (Windows 11 22H2+)
        IORING_CREATE_FLAGS flags = {};
        flags.Required = IORING_CREATE_REQUIRED_FLAGS_NONE;
        flags.Advisory = IORING_CREATE_ADVISORY_FLAGS_NONE;

        HRESULT hr = CreateIoRing(IORING_VERSION_3, flags, 128, 128, &hRing_);
        if (FAILED(hr)) {
            hr = CreateIoRing(IORING_VERSION_2, flags, 128, 128, &hRing_);
        }

        return SUCCEEDED(hr);
    }

    bool RegisterBuffers(void* ring_base_ptr, size_t total_size, size_t zone_count) override {
        if (!hRing_) return false;

        ring_base_ = static_cast<uint8_t*>(ring_base_ptr);
        zone_size_ = total_size / zone_count;
        zone_count_ = zone_count;

        std::vector<IORING_BUFFER_INFO> buffer_infos;
        for (size_t i = 0; i < zone_count; ++i) {
            IORING_BUFFER_INFO info = {};
            info.Address = ring_base_ + (i * zone_size_);
            info.Length = (uint32_t)zone_size_;
            buffer_infos.push_back(info);
        }

        // Register buffers for zero-copy DMA
        HRESULT hr = BuildIoRingRegisterBuffers(hRing_, (uint32_t)zone_count, buffer_infos.data(), 0);
        if (FAILED(hr)) return false;

        uint32_t submitted = 0;
        hr = SubmitIoRing(hRing_, 0, 0, &submitted);
        if (FAILED(hr)) return false;

        // Wait for registration to complete
        IORING_CQE cqe;
        hr = PopIoRingCompletion(hRing_, &cqe);
        return SUCCEEDED(hr) && cqe.ResultCode == S_OK;
    }

    void SubmitRead(const IORequest& req) override {
        if (!hRing_ || hFile_ == INVALID_HANDLE_VALUE) return;

        IORING_HANDLE_REF handle_ref = IoRingHandleRefFromHandle(hFile_);
        IORING_BUFFER_REF buffer_ref = IoRingBufferRefFromIndexAndOffset(req.zone_index, req.zone_offset);

        // Build the read operation
        // Note: In a real implementation, we might batch these before BuildIoRingReadFile
        BuildIoRingReadFile(hRing_, 
                            handle_ref, 
                            buffer_ref, 
                            (uint32_t)req.size, 
                            req.file_offset, 
                            req.request_id, 
                            IOSQE_FLAGS_NONE);
        
        pending_count_++;
    }

    void Flush() override {
        if (!hRing_) return;
        uint32_t submitted = 0;
        SubmitIoRing(hRing_, 0, 0, &submitted);
    }

    int PollCompletions(std::vector<IOCompletion>& out_events) override {
        if (!hRing_) return 0;

        IORING_CQE cqe;
        int count = 0;
        HRESULT hr;
        while ((hr = PopIoRingCompletion(hRing_, &cqe)) == S_OK) {
            IOCompletion completion;
            completion.request_id = (int)cqe.UserData;
            completion.result_code = (HRESULT)cqe.ResultCode; // Keep the raw HRESULT for debugging
            out_events.push_back(completion);
            count++;
            pending_count_--;
        }
        return count;
    }

    void Shutdown() override {
        if (hRing_) {
            CloseIoRing(hRing_);
            hRing_ = nullptr;
        }
        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
    }

private:
    HIORING hRing_;
    HANDLE hFile_;
    uint8_t* ring_base_ = nullptr;
    size_t zone_size_ = 0;
    size_t zone_count_ = 0;
    std::atomic<int> pending_count_{0};
};
