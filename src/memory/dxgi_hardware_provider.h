#pragma once
#include "hardware_provider_interface.h"
#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#pragma comment(lib, "dxgi.lib")

namespace RawrXD::Memory {
using Microsoft::WRL::ComPtr;
class DxgiHardwareProvider : public IHardwareProvider {
public:
    DxgiHardwareProvider() { CreateDXGIFactory1(IID_PPV_ARGS(&m_factory)); }
    HardwareMetrics poll() override {
        HardwareMetrics m;
        if (!m_factory) return m;
        ComPtr<IDXGIAdapter3> adapter;
        if (SUCCEEDED(m_factory->EnumAdapters1(0, (IDXGIAdapter1**)adapter.GetAddressOf()))) {
            DXGI_QUERY_VIDEO_MEMORY_INFO info;
            if (SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info))) {
                m.vramUsage = info.CurrentUsage;
                m.vramTotal = info.Budget;
            }
        }
        return m;
    }
private:
    ComPtr<IDXGIFactory1> m_factory;
};
}
