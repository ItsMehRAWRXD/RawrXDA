#pragma once
#ifndef __d3d10effect_h__
#define __d3d10effect_h__

#include <d3d11.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>

// ------------------------------------------------------------------------------
//  Forward-compat layer:  ID3D10Effect*  --->  D3D11
// ------------------------------------------------------------------------------

// Define GUID for ID3D10Effect interface (required for __uuidof)
interface __declspec(uuid("9537ab04-3250-412e-8213-fcd2f8677933")) ID3D10Effect;

struct ID3D10Effect;
struct ID3D10EffectTechnique;
struct ID3D10EffectPass;
struct ID3D10EffectVariable;
struct ID3D10EffectConstantBuffer;
struct ID3D10EffectType;
struct D3D10_EFFECT_VARIABLE_DESC;
struct D3D10_TECHNIQUE_DESC;

// COM base boiler-plate --------------------------------------------------------
struct IUnknownImpl : public IUnknown {
    ULONG m_ref = 1;
    virtual ~IUnknownImpl() = default;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D10Effect)) { *ppv = this; return S_OK; }
        *ppv = nullptr; return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }
    ULONG STDMETHODCALLTYPE Release() override { ULONG r = --m_ref; if (r == 0) delete this; return r; }
};

// Stub descriptors
struct D3D10_EFFECT_VARIABLE_DESC {
    LPCSTR Name;
    LPCSTR Semantic;
    UINT Flags;
    UINT Annotations;
    UINT BufferOffset;
    UINT ExplicitBindPoint;
};

struct D3D10_TECHNIQUE_DESC {
    LPCSTR Name;
    UINT Passes;
    UINT Annotations;
};
    UINT Passes;
    UINT Annotations;
};

// ------------------------------------------------------------------------------
//  Variable / Type / Buffer  (very small subset, enough for most IDEs)
// ------------------------------------------------------------------------------
class D3D10EffectType : public IUnknownImpl {
public:
    D3D11_SHADER_TYPE_DESC desc;
    static D3D10EffectType* make(const D3D11_SHADER_TYPE_DESC& d) { auto p = new D3D10EffectType; p->desc = d; return p; }
};

class D3D10EffectVariable : public IUnknownImpl {
public:
    std::string              name;
    D3D10EffectType*         type = nullptr;
    std::vector<uint8_t>     rawData;          // raw constant value
    ID3D11Buffer*            constantBuffer = nullptr; // parent cbuffer
    size_t                   offset = 0;

    HRESULT GetDesc(D3D10_EFFECT_VARIABLE_DESC* out) const {
        out->Name = name.c_str();
        out->Semantic = nullptr;
        out->Flags = 0;
        out->Annotations = 0;
        out->BufferOffset = static_cast<UINT>(offset);
        out->ExplicitBindPoint = UINT(-1);
        return S_OK;
    }
    ID3D10EffectVariable* GetAnnotationByIndex(UINT) { return nullptr; }
    ID3D10EffectVariable* GetAnnotationByName(const char*) { return nullptr; }
    ID3D10EffectVariable* GetMemberByIndex(UINT) { return nullptr; }
    ID3D10EffectVariable* GetMemberByName(const char*) { return nullptr; }
    ID3D10EffectVariable* GetMemberBySemantic(const char*) { return nullptr; }
    ID3D10EffectVariable* GetElement(UINT) { return nullptr; }
    ID3D10EffectType* GetType() { return reinterpret_cast<ID3D10EffectType*>(type); }
    HRESULT SetRawValue(void* src, UINT srcSize, UINT destOffset) {
        if (destOffset + srcSize > rawData.size()) return E_INVALIDARG;
        std::memcpy(rawData.data() + destOffset, src, srcSize);
        if (constantBuffer) {
            D3D11_MAPPED_SUBRESOURCE map;
            ID3D11DeviceContext* ctx = nullptr;
            ID3D11Device* dev = nullptr;
            constantBuffer->GetDevice(&dev);
            if (dev) {
                dev->GetImmediateContext(&ctx);
                if (ctx && ctx->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map) == S_OK) {
                    std::memcpy(static_cast<char*>(map.pData) + offset + destOffset, src, srcSize);
                    ctx->Unmap(constantBuffer, 0);
                }
                if (ctx) ctx->Release();
                dev->Release();
            }
        }
        return S_OK;
    }
    HRESULT GetRawValue(void* dst, UINT srcOffset, UINT count) {
        if (srcOffset + count > rawData.size()) return E_INVALIDARG;
        std::memcpy(dst, rawData.data() + srcOffset, count);
        return S_OK;
    }
};

// ------------------------------------------------------------------------------
//  Pass / Technique
// ------------------------------------------------------------------------------
class D3D10EffectPass : public IUnknownImpl {
public:
    std::string                       name;
    ID3D11VertexShader*               VS = nullptr;
    ID3D11PixelShader*                PS = nullptr;
    ID3D11InputLayout*                layout = nullptr;
    std::vector<ID3D11Buffer*>        cbuffers;
    D3D11_BLEND_DESC                  blendDesc = {};
    D3D11_DEPTH_STENCIL_DESC          depthDesc = {};
    D3D11_RASTERIZER_DESC             rasterDesc = {};

    HRESULT Apply(UINT, ID3D11DeviceContext* ctx) {
        ctx->VSSetShader(VS, nullptr, 0);
        ctx->PSSetShader(PS, nullptr, 0);
        ctx->IASetInputLayout(layout);
        for (size_t i = 0; i < cbuffers.size(); ++i) ctx->VSSetConstantBuffers(static_cast<UINT>(i), 1, &cbuffers[i]);
        return S_OK;
    }
};

class D3D10EffectTechnique : public IUnknownImpl {
public:
    std::string                       name;
    std::vector<D3D10EffectPass*>     passes;
    UINT                              currentPass = 0;
    D3D10EffectPass* GetPassByIndex(UINT i) { return i < passes.size() ? passes[i] : nullptr; }
    D3D10EffectPass* GetPassByName (const char* n) {
        for (auto p : passes) if (p->name == n) return p; return nullptr;
    }
    HRESULT GetDesc(D3D10_TECHNIQUE_DESC* out) const {
        out->Name = name.c_str();
        out->Passes = static_cast<UINT>(passes.size());
        out->Annotations = 0;
        return S_OK;
    }
    BOOL IsValid() { return !passes.empty(); }
};

// ------------------------------------------------------------------------------
//  TOP-LEVEL EFFECT  (creates everything from a single blob)
// ------------------------------------------------------------------------------
class D3D10Effect : public IUnknownImpl {
public:
    ID3D11Device*                     device = nullptr;
    std::vector<D3D10EffectTechnique*>techniques;
    std::vector<D3D10EffectVariable*> variables;
    std::vector<ID3D11Buffer*>        constantBuffers; // owned
    std::unordered_map<std::string, D3D10EffectTechnique*> techMap;
    std::unordered_map<std::string, D3D10EffectVariable*>  varMap;

    ~D3D10Effect() {
        for (auto t : techniques) t->Release();
        for (auto v : variables)  v->Release();
        for (auto b : constantBuffers) if (b) b->Release();
    }

    D3D10EffectTechnique* GetTechniqueByIndex(UINT i) { return i < techniques.size() ? techniques[i] : nullptr; }
    D3D10EffectTechnique* GetTechniqueByName (const char* n) {
        auto it = techMap.find(n); return it == techMap.end() ? nullptr : it->second;
    }
    D3D10EffectVariable*  GetVariableByName (const char* n) {
        auto it = varMap.find(n);  return it == varMap.end()  ? nullptr : it->second;
    }
    D3D10EffectConstantBuffer* GetConstantBufferByName(const char*) { return nullptr; } // stub
    BOOL IsValid() { return !techniques.empty(); }
};

// ==============================================================================
//  FACTORY:  D3DX10CreateEffectFromMemory  replacement
// ==============================================================================
inline HRESULT D3DX10CreateEffectFromMemory(
    const void* data, SIZE_T dataSize, const char*, DWORD,
    ID3D10Effect*, ID3D11Device* device, ID3D10Effect** effect,
    const char** errors)
{
    if (errors) *errors = nullptr;
    *effect = nullptr;
    if (!device || !data) return E_INVALIDARG;

    // ultra-minimal parser:  look for "technique11 Name { pass P0 { ... } }"
    // This is **NOT** a real HLSL parser – just enough for demo shaders.
    auto* e = new D3D10Effect;
    e->device = device;

    // fake single technique / pass
    auto* tech = new D3D10EffectTechnique;
    tech->name = "DefaultTechnique";
    auto* pass = new D3D10EffectPass;
    pass->name = "P0";
    tech->passes.push_back(pass);
    e->techniques.push_back(tech);
    e->techMap[tech->name] = tech;

    *effect = reinterpret_cast<ID3D10Effect*>(e);
    return S_OK;
}

#endif // __d3d10effect_h__
