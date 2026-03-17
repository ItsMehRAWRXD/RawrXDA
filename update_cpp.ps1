bool GGUFD3D12Bridge::DispatchFlashAttention(ID3D12Resource* qBuffer,
                                             ID3D12Resource* kBuffer,
                                             ID3D12Resource* vBuffer,
                                             ID3D12Resource* outBuffer,
                                             uint32_t seqLen, uint32_t headDim, uint32_t numHeads) {
    if (!gpu_.psoFlashAttention || !qBuffer || !kBuffer || !vBuffer || !outBuffer) return false;

    // Constants
    struct FAConstants {
        uint32_t N;
        uint32_t d;
        uint32_t num_heads;
        float scale;
    } cb;
    cb.N = seqLen;
    cb.d = headDim;
    cb.num_heads = numHeads;
    cb.scale = 1.0f / sqrtf((float)headDim);

    transitionResource(qBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(kBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(vBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    transitionResource(outBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

    list_->SetPipelineState(gpu_.psoFlashAttention.Get());
    list_->SetComputeRootSignature(gpu_.rootSig.Get());
    
    // b0
    list_->SetComputeRoot32BitConstants(0, sizeof(cb) / 4, &cb, 0);
    // t0, t1, t2 -> 1, 2, 3 in typical bindings or SRV bindings
    // Let's assume standard RootSig from this class:
    // RootParam 0: CBV (b0) - Constants
    // RootParam 1: SRV (t0)
    // RootParam 3: UAV (u0)
    // Actually, look at the root sig of this class:
