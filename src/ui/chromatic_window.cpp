#define NOMINMAX
#include "chromatic_window.h"
#include <chrono>
#include <cmath>

namespace RawrXD {
namespace UI {

// HLSL Shaders embedded as strings

static const char* g_vertexShaderSrc = R"(
cbuffer Constants : register(b0) {
    float time;
    float waveAmp;
    float waveFreq;
    float waveSpeed;
    float chromaSpeed;
    float chromaSat;
    float chromaBright;
    float aspectRatio;
    float screenWidth;
    float screenHeight;
    float2 padding;
};

struct VS_INPUT {
    float4 pos : POSITION;
    float2 uv : TEXCOORD0;
};

struct VS_OUTPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 waveColor : COLOR0;
};

VS_OUTPUT main(VS_INPUT input) {
    VS_OUTPUT output;
    
    // Apply wave displacement
    float waveY = sin(input.pos.x * waveFreq * 100.0 + time * waveSpeed) * waveAmp * 0.001;
    float waveX = cos(input.pos.y * waveFreq * 100.0 + time * waveSpeed * 0.7) * waveAmp * 0.0005;
    
    output.pos = input.pos;
    output.pos.x += waveX;
    output.pos.y += waveY;
    output.uv = input.uv;
    
    // Pre-calculate wave color for pixel shader
    float hue = frac(time * chromaSpeed * 0.1 + input.uv.x * 0.5 + input.uv.y * 0.3);
    output.waveColor = float4(hue, chromaSat, chromaBright, 1.0);
    
    return output;
}
)";

static const char* g_pixelShaderSrc = R"(
cbuffer Constants : register(b0) {
    float time;
    float waveAmp;
    float waveFreq;
    float waveSpeed;
    float chromaSpeed;
    float chromaSat;
    float chromaBright;
    float aspectRatio;
    float screenWidth;
    float screenHeight;
    float2 padding;
};

struct PS_INPUT {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 waveColor : COLOR0;
};

// HSL to RGB conversion
float3 hsl2rgb(float h, float s, float l) {
    float3 rgb;
    float c = (1.0 - abs(2.0 * l - 1.0)) * s;
    float x = c * (1.0 - abs(fmod(h * 6.0, 2.0) - 1.0));
    float m = l - c * 0.5;
    
    if (h < 1.0/6.0) rgb = float3(c, x, 0);
    else if (h < 2.0/6.0) rgb = float3(x, c, 0);
    else if (h < 3.0/6.0) rgb = float3(0, c, x);
    else if (h < 4.0/6.0) rgb = float3(0, x, c);
    else if (h < 5.0/6.0) rgb = float3(x, 0, c);
    else rgb = float3(c, 0, x);
    
    return rgb + m;
}

float4 main(PS_INPUT input) : SV_TARGET {
    float2 uv = input.uv;
    
    // Create wave pattern in background
    float wave1 = sin(uv.x * 20.0 + time * 2.0) * 0.5 + 0.5;
    float wave2 = sin(uv.y * 15.0 + time * 1.5) * 0.5 + 0.5;
    float wave3 = sin((uv.x + uv.y) * 10.0 + time * 3.0) * 0.5 + 0.5;
    
    // Combine waves
    float wavePattern = (wave1 * wave2 + wave3) / 2.0;
    
    // Chromatic/rainbow hue cycling
    float hue = frac(time * chromaSpeed * 0.1 + uv.x * 0.3 + uv.y * 0.2 + wavePattern * 0.2);
    float sat = chromaSat * (0.8 + wavePattern * 0.2);
    float light = 0.15 + wavePattern * 0.1;  // Dark background with wave brightness
    
    float3 bgColor = hsl2rgb(hue, sat * 0.6, light);
    
    // Add glow effect at wave peaks
    float glow = pow(wavePattern, 3.0) * 0.3;
    float3 glowColor = hsl2rgb(frac(hue + 0.5), 1.0, 0.7);
    
    float3 finalColor = bgColor + glowColor * glow;
    
    // Neon edge glow
    float edgeX = smoothstep(0.0, 0.05, uv.x) * smoothstep(1.0, 0.95, uv.x);
    float edgeY = smoothstep(0.0, 0.05, uv.y) * smoothstep(1.0, 0.95, uv.y);
    float edge = 1.0 - edgeX * edgeY;
    float3 edgeColor = hsl2rgb(frac(time * 0.2), 1.0, 0.6);
    finalColor += edgeColor * edge * 0.5;
    
    return float4(finalColor, 1.0);
}
)";

ChromaticWindow::ChromaticWindow() {
    QueryPerformanceFrequency(&m_frequency);
    QueryPerformanceCounter(&m_lastTime);
}

ChromaticWindow::~ChromaticWindow() {
    destroy();
}

bool ChromaticWindow::create(HINSTANCE hInstance, int width, int height) {
    m_hInstance = hInstance;
    m_width = width;
    m_height = height;
    
    // Register window class
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"RawrXD_Chromatic_Window";
    
    if (!RegisterClassExW(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }
    }
    
    // Create window
    DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rc = { 0, 0, width, height };
    AdjustWindowRect(&rc, style, FALSE);
    
    m_hwnd = CreateWindowExW(
        0,
        L"RawrXD_Chromatic_Window",
        L"RawrXD Chromatic Display - 540Hz @ 4K",
        style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInstance, this
    );
    
    if (!m_hwnd) {
        return false;
    }
    
    SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    
    // Initialize D3D11
    if (!initD3D11(width, height)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
        return false;
    }
    
    // Create shaders and buffers
    if (!createShaders() || !createBuffers()) {
        destroy();
        return false;
    }
    
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    
    return true;
}

bool ChromaticWindow::initD3D11(int width, int height) {
    // Create swap chain description
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = TARGET_REFRESH_HZ;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = m_hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
    
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels, 1,
        D3D11_SDK_VERSION,
        &scd,
        &m_swapChain,
        &m_device,
        &featureLevel,
        &m_context
    );
    
    if (FAILED(hr)) {
        // Try without tearing support
        scd.Flags = 0;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
            featureLevels, 1, D3D11_SDK_VERSION, &scd,
            &m_swapChain, &m_device, &featureLevel, &m_context
        );
        
        if (FAILED(hr)) {
            return false;
        }
    }
    
    // Create render target view
    ID3D11Texture2D* backBuffer = nullptr;
    hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    if (FAILED(hr)) return false;
    
    hr = m_device->CreateRenderTargetView(backBuffer, nullptr, &m_rtv);
    backBuffer->Release();
    
    if (FAILED(hr)) return false;
    
    // Set viewport
    D3D11_VIEWPORT vp = {};
    vp.Width = (float)width;
    vp.Height = (float)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);
    
    return true;
}

bool ChromaticWindow::createShaders() {
    HRESULT hr;
    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    
    // Compile vertex shader
    hr = D3DCompile(
        g_vertexShaderSrc, strlen(g_vertexShaderSrc),
        "VS", nullptr, nullptr, "main", "vs_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &vsBlob, &errorBlob
    );
    
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    
    hr = m_device->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, &m_vertexShader
    );
    
    if (FAILED(hr)) {
        vsBlob->Release();
        return false;
    }
    
    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    
    hr = m_device->CreateInputLayout(
        layout, 2,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        &m_inputLayout
    );
    vsBlob->Release();
    
    if (FAILED(hr)) return false;
    
    // Compile pixel shader
    hr = D3DCompile(
        g_pixelShaderSrc, strlen(g_pixelShaderSrc),
        "PS", nullptr, nullptr, "main", "ps_5_0",
        D3DCOMPILE_ENABLE_STRICTNESS, 0,
        &psBlob, &errorBlob
    );
    
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
        return false;
    }
    
    hr = m_device->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, &m_pixelShader
    );
    psBlob->Release();
    
    return SUCCEEDED(hr);
}

bool ChromaticWindow::createBuffers() {
    // Fullscreen quad vertices (pos + uv)
    struct Vertex {
        float x, y, z, w;
        float u, v;
    };
    
    Vertex vertices[] = {
        { -1.0f,  1.0f, 0.0f, 1.0f,  0.0f, 0.0f }, // Top-left
        {  1.0f,  1.0f, 0.0f, 1.0f,  1.0f, 0.0f }, // Top-right
        { -1.0f, -1.0f, 0.0f, 1.0f,  0.0f, 1.0f }, // Bottom-left
        {  1.0f,  1.0f, 0.0f, 1.0f,  1.0f, 0.0f }, // Top-right
        {  1.0f, -1.0f, 0.0f, 1.0f,  1.0f, 1.0f }, // Bottom-right
        { -1.0f, -1.0f, 0.0f, 1.0f,  0.0f, 1.0f }, // Bottom-left
    };
    
    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(vertices);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    
    D3D11_SUBRESOURCE_DATA vdata = {};
    vdata.pSysMem = vertices;
    
    HRESULT hr = m_device->CreateBuffer(&vbd, &vdata, &m_vertexBuffer);
    if (FAILED(hr)) return false;
    
    // Constant buffer
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.ByteWidth = sizeof(ConstantData);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    
    hr = m_device->CreateBuffer(&cbd, nullptr, &m_constantBuffer);
    return SUCCEEDED(hr);
}

void ChromaticWindow::updateConstants() {
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = m_context->Map(m_constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (SUCCEEDED(hr)) {
        ConstantData* data = (ConstantData*)mapped.pData;
        data->time = m_time;
        data->waveAmp = m_waveAmplitude;
        data->waveFreq = m_waveFrequency;
        data->waveSpeed = m_waveSpeed;
        data->chromaSpeed = m_chromaSpeed;
        data->chromaSat = m_chromaSat;
        data->chromaBright = m_chromaBright;
        data->aspectRatio = (float)m_width / (float)m_height;
        data->screenWidth = (float)m_width;
        data->screenHeight = (float)m_height;
        m_context->Unmap(m_constantBuffer, 0);
    }
}

void ChromaticWindow::render() {
    if (!m_context || !m_rtv) return;
    
    // Update time
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    float deltaTime = (float)(now.QuadPart - m_lastTime.QuadPart) / (float)m_frequency.QuadPart;
    m_lastTime = now;
    m_time += deltaTime;
    
    // Update constant buffer
    updateConstants();
    
    // Clear
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    m_context->ClearRenderTargetView(m_rtv, clearColor);
    
    // Set render target
    m_context->OMSetRenderTargets(1, &m_rtv, nullptr);
    
    // Set shaders
    m_context->VSSetShader(m_vertexShader, nullptr, 0);
    m_context->PSSetShader(m_pixelShader, nullptr, 0);
    m_context->VSSetConstantBuffers(0, 1, &m_constantBuffer);
    m_context->PSSetConstantBuffers(0, 1, &m_constantBuffer);
    
    // Set input layout and vertex buffer
    m_context->IASetInputLayout(m_inputLayout);
    UINT stride = 24; // 4 floats for pos + 2 floats for uv
    UINT offset = 0;
    m_context->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);
    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    // Draw fullscreen quad
    m_context->Draw(6, 0);
}

void ChromaticWindow::present() {
    if (m_swapChain) {
        // Try to present with minimal latency for high refresh rate
        m_swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING);
    }
}

void ChromaticWindow::destroy() {
    if (m_constantBuffer) { m_constantBuffer->Release(); m_constantBuffer = nullptr; }
    if (m_vertexBuffer) { m_vertexBuffer->Release(); m_vertexBuffer = nullptr; }
    if (m_inputLayout) { m_inputLayout->Release(); m_inputLayout = nullptr; }
    if (m_pixelShader) { m_pixelShader->Release(); m_pixelShader = nullptr; }
    if (m_vertexShader) { m_vertexShader->Release(); m_vertexShader = nullptr; }
    if (m_rtv) { m_rtv->Release(); m_rtv = nullptr; }
    if (m_swapChain) { m_swapChain->Release(); m_swapChain = nullptr; }
    if (m_context) { m_context->Release(); m_context = nullptr; }
    if (m_device) { m_device->Release(); m_device = nullptr; }
    
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

LRESULT CALLBACK ChromaticWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    ChromaticWindow* self = reinterpret_cast<ChromaticWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
        
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            DestroyWindow(hwnd);
            return 0;
        }
        // Adjust parameters with keys
        if (self) {
            switch (wParam) {
            case VK_UP:
                self->m_waveAmplitude += 2.0f;
                break;
            case VK_DOWN:
                self->m_waveAmplitude = (std::max)(0.0f, self->m_waveAmplitude - 2.0f);
                break;
            case VK_LEFT:
                self->m_chromaSpeed = (std::max)(0.1f, self->m_chromaSpeed - 0.1f);
                break;
            case VK_RIGHT:
                self->m_chromaSpeed += 0.1f;
                break;
            case 'W':
                self->m_waveSpeed += 0.5f;
                break;
            case 'S':
                self->m_waveSpeed = (std::max)(0.1f, self->m_waveSpeed - 0.5f);
                break;
            }
        }
        return 0;
        
    case WM_SIZE:
        if (self && self->m_swapChain && wParam != SIZE_MINIMIZED) {
            // Handle resize - recreate swap chain buffers
            self->m_width = LOWORD(lParam);
            self->m_height = HIWORD(lParam);
            
            if (self->m_rtv) {
                self->m_rtv->Release();
                self->m_rtv = nullptr;
            }
            
            self->m_swapChain->ResizeBuffers(0, self->m_width, self->m_height, 
                DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
            
            ID3D11Texture2D* backBuffer = nullptr;
            self->m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
            if (backBuffer) {
                self->m_device->CreateRenderTargetView(backBuffer, nullptr, &self->m_rtv);
                backBuffer->Release();
            }
            
            D3D11_VIEWPORT vp = {};
            vp.Width = (float)self->m_width;
            vp.Height = (float)self->m_height;
            vp.MinDepth = 0.0f;
            vp.MaxDepth = 1.0f;
            self->m_context->RSSetViewports(1, &vp);
        }
        return 0;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

} // namespace UI
} // namespace RawrXD
