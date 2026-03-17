import re

with open(r"d:\rawrxd\include\gguf_d3d12_bridge.h", "r", encoding="utf-8") as f:
    text = f.read()

text = text.replace(
    '''Microsoft::WRL::ComPtr<ID3DBlob> rope;''',
    '''Microsoft::WRL::ComPtr<ID3DBlob> rope;
        Microsoft::WRL::ComPtr<ID3DBlob> ropeFused;'''
)

text = text.replace(
    '''Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRoPE;''',
    '''Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRoPE;
        Microsoft::WRL::ComPtr<ID3D12PipelineState> psoRoPEFused;'''
)

if "DispatchCSRoPE_Fused" not in text:
    sig = '''    bool DispatchCSRoPE_Fused(ID3D12Resource* q_buffer, ID3D12Resource* k_buffer, ID3D12Resource* cossin_buffer,
                              uint32_t seq_pos, uint32_t head_dim, uint32_t num_heads);'''
    text = text.replace(
        '''bool DispatchRoPE(ID3D12Resource* inoutBuffer,''',
        sig + "\n    bool DispatchRoPE(ID3D12Resource* inoutBuffer,"
    )

with open(r"d:\rawrxd\include\gguf_d3d12_bridge.h", "w", encoding="utf-8") as f:
    f.write(text)
print("Updated header!")
