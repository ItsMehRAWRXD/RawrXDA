#include "hardware_provider_interface.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

namespace RawrXD::Memory {

class DxgiHardwareProvider : public IHardwareProvider {
public:
    DxgiHardwareProvider() = default;

    bool initialize() override {
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_factory));
        if (FAILED(hr)) return false;

        ComPtr<IDXGIAdapter1> adapter;
        if (m_factory->EnumAdapters1(0, &adapter) == DXGI_ERROR_NOT_FOUND) return false;
        
        adapter.As(&m_adapter3);
        return m_adapter3 != nullptr;
    }

    HardwareMetrics sample() override {
        HardwareMetrics m{};
        if (!m_adapter3) return m;

        DXGI_QUERY_VIDEO_MEMORY_INFO memInfo;
        if (SUCCEEDED(m_adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo))) {
            m.vramUsedBytes = memInfo.CurrentUsage;
            m.vramTotalBytes = memInfo.Budget; // Note: Budget can be less than total phys
        }

        // DXGI doesn't provide GPU load or Temp directly without vendor-specific extensions (NVML/ADL)
        // We leave these at 0.0f for the pure DXGI implementation.
        return m;
    }

    std::string getProviderName() const override { return "DXGI/Direct3D"; }

private:
    ComPtr<IDXGIFactory1> m_factory;
    ComPtr<IDXGIAdapter3> m_adapter3;
};

} // namespace RawrXD::Memory
