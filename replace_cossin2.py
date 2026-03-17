import re

with open(r"d:\rawrxd\src\gguf_d3d12_bridge.cpp", "r", encoding="utf-8") as f:
    text = f.read()

# Since some work might already be done by the previous run, let's just make sure we handle it robustly
patt_sig = r"ID3D12Resource\*\s+gamma,\s*uint32_t\s+gammaElements,"
if "ID3D12Resource* cossin," not in text:
    text = re.sub(patt_sig, "ID3D12Resource* gamma, uint32_t gammaElements,\n                                      ID3D12Resource* cossin, uint32_t cossinElements,", text)

if "srvRange.NumDescriptors = 4;" not in text:
    text = text.replace('srvRange.NumDescriptors = 3;', 'srvRange.NumDescriptors = 4; // t0, t1, t2, t3')
    text = text.replace('heapDesc.NumDescriptors = 5;', 'heapDesc.NumDescriptors = 6;')

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

if "t3: StructuredBuffer<float2>" not in text:
    text = text.replace(
        '// u0: RWStructuredBuffer<float> (in-place: RMSNorm, Softmax, RoPE, SiLU)',
        t3_impl + '\n    // u0: RWStructuredBuffer<float> (in-place: RMSNorm, Softmax, RoPE, SiLU)'
    )

# Now, target calls.
# We want to insert 2 arguments after the 3rd pair.
# We will just write a custom simple state machine pass over the source code.
new_text = ""
lines = text.split('\n')
i = 0
in_call = False
args_seen = 0
while i < len(lines):
    line = lines[i]
    if "setupDescriptorsForDispatch(" in line and "uint32_t" not in line and "cossinElements" not in line:
        in_call = True
        args_seen = 0
        new_text += line + '\n'
        i += 1
        continue
    
    if in_call:
        # Check if we have hit the 3rd pair (i.e. we read 3 lines with arguments, wait, usually it's one pair per line)
        if ");" in line:
            new_text += line + '\n'
            in_call = False
            i += 1
            continue
        
        # It's an argument line
        new_text += line + '\n'
        args_seen += 1
        
        # if this was the 3rd argument line, insert cossin
        if args_seen == 3:
            new_text += "            nullptr, 0,        // t3 cossin\n"
    else:
        new_text += line + '\n'
    
    i += 1

with open(r"d:\rawrxd\src\gguf_d3d12_bridge.cpp", "w", encoding="utf-8") as f:
    f.write(new_text)

print("regex calls updated")
