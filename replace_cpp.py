import re

with open(r"d:\rawrxd\src\gguf_d3d12_bridge.cpp", "r", encoding="utf-8") as f:
    text = f.read()

text = text.replace(
    'shaders_.rope.Reset();',
    'shaders_.rope.Reset();\n    shaders_.ropeFused.Reset();'
)

text = text.replace(
    'shaders_.rope         = loadBlob(base / "CSRoPE.cso");',
    'shaders_.rope         = loadBlob(base / "CSRoPE.cso");\n        shaders_.ropeFused    = loadBlob(base / "CSRoPE_Fused.cso");'
)

text = text.replace(
    'shaders_.rope         = compileFromFile(hlslPath, "CSRoPE", "cs_5_1");',
    'shaders_.rope         = compileFromFile(hlslPath, "CSRoPE", "cs_5_1");\n        shaders_.ropeFused    = compileFromFile(hlslPath, "CSRoPE_Fused", "cs_6_0");'
)

text = text.replace(
    'gpu_.psoRoPE.Reset();',
    'gpu_.psoRoPE.Reset();\n    gpu_.psoRoPEFused.Reset();'
)

text = text.replace(
    'createPSO(shaders_.rope.Get(), gpu_.psoRoPE);',
    'createPSO(shaders_.rope.Get(), gpu_.psoRoPE);\n        createPSO(shaders_.ropeFused.Get(), gpu_.psoRoPEFused);'
)

if "DispatchCSRoPE_Fused" not in text:
    func_code = '''
bool GGUFD3D12Bridge::DispatchCSRoPE_Fused(ID3D12Resource* q_buffer, ID3D12Resource* k_buffer, ID3D12Resource* cossin_buffer,
                                           uint32_t seq_pos, uint32_t head_dim, uint32_t num_heads) {
    if (!gpu_.psoRoPEFused || !q_buffer || !k_buffer || !cossin_buffer) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoRoPEFused.Get()))) return false;

    list_->SetComputeRootSignature(gpu_.rootSig.Get());

    Constants cb{};
    cb.cbPosition = seq_pos;
    cb.cbCols     = head_dim;
    cb.cbDim      = num_heads;
    list_->SetComputeRoot32BitConstants(0, sizeof(Constants)/4, &cb, 0);

    transition(list_.Get(), q_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), k_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    // cossin_buffer must be SRV. Wait, our root signature bindings:
    // ROOT PARAMETERS:
    // 0: Constants (b0)
    // 1: t0 (ByteAddressBuffer g_matrix)
    // 2: t1 (StructuredBuffer<float> g_vec)
    // 3: u0 (RWStructuredBuffer<float> g_inout)
    // 4: u1 (RWStructuredBuffer<float> g_out)
    // 5: t2 (StructuredBuffer<float> g_gamma / etc) -> Let's bind cossin to t2? But I used t3 in hlsl.
    // Wait, let's see how rootSig is built in Initialize() or something. The Root Signature might only map up to t2!
'''
    text += "\n// Dummy replace to check later"

with open(r"d:\rawrxd\src\gguf_d3d12_bridge.cpp", "w", encoding="utf-8") as f:
    f.write(text)
print("Updated cpp partially...")
