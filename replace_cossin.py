import re

with open(r"d:\rawrxd\src\gguf_d3d12_bridge.cpp", "r", encoding="utf-8") as f:
    text = f.read()

# Make the function signature accept cossin
text = text.replace(
    'ID3D12Resource* gamma, uint32_t gammaElements,',
    'ID3D12Resource* gamma, uint32_t gammaElements,\n                                      ID3D12Resource* cossin, uint32_t cossinElements,'
)

with open(r"d:\rawrxd\include\gguf_d3d12_bridge.h", "r", encoding="utf-8") as fh:
    h_text = fh.read()
    h_text = h_text.replace(
        'ID3D12Resource* gamma, uint32_t gammaElements,',
        'ID3D12Resource* gamma, uint32_t gammaElements,\n                                      ID3D12Resource* cossin, uint32_t cossinElements,'
    )
    with open(r"d:\rawrxd\include\gguf_d3d12_bridge.h", "w", encoding="utf-8") as hf:
        fh_out = h_text
        hf.write(h_text)

# Also update the SRV Count in Root Parameter
text = text.replace('srvRange.NumDescriptors = 3;', 'srvRange.NumDescriptors = 4; // t0, t1, t2, t3')
text = text.replace('heapDesc.NumDescriptors = 5;', 'heapDesc.NumDescriptors = 6;')

# Add t3 implementation:
t3_impl = '''
    // t3: StructuredBuffer<float2> (cossin tables for RoPE)
    cpu.ptr += inc;
    if (cossin) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
        srv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Format = DXGI_FORMAT_UNKNOWN;
        srv.Buffer.NumElements = cossinElements;
        srv.Buffer.StructureByteStride = sizeof(float) * 2;
        device_->CreateShaderResourceView(cossin, &srv, cpu);
    } else {
        D3D12_SHADER_RESOURCE_VIEW_DESC nullSrv{};
        nullSrv.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        nullSrv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullSrv.Format = DXGI_FORMAT_R32G32_FLOAT;
        nullSrv.Buffer.NumElements = 1;
        device_->CreateShaderResourceView(nullptr, &nullSrv, cpu);
    }
'''

# Find where u0 starts, insert t3 right before u0
text = text.replace(
    '// u0: RWStructuredBuffer<float> (in-place: RMSNorm, Softmax, RoPE, SiLU)',
    t3_impl + '\n    // u0: RWStructuredBuffer<float> (in-place: RMSNorm, Softmax, RoPE, SiLU)'
)

# Replace all calls:
# Find setupDescriptorsForDispatch( ... parameters ... )
# We can use regex to find the blocks:
# setupDescriptorsForDispatch(
#     ...
#     ...
#     arg_gamma, arg_gammaElements,
#     arg_inout, arg_inoutElements,
#     arg_out, arg_outElements);

def repl_func(m):
    # m.group(1) is everything from setup(...) to comma after first 3 resources
    # Actually wait. Let's just do a simpler search/replace manually because the format varies.
    pass

import sys
print("Done regex prep")
