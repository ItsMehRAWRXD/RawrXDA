
bool GGUFD3D12Bridge::DispatchRoPEFused(ID3D12Resource* q_buffer,
                                        ID3D12Resource* k_buffer,
                                        ID3D12Resource* cossin_buffer,
                                        uint32_t seq_len,
                                        uint32_t head_dim,
                                        uint32_t num_heads) {
    if (!gpu_.psoRoPEFused || !q_buffer || !k_buffer || !cossin_buffer) return false;

    if (FAILED(allocator_->Reset())) return false;
    if (FAILED(list_->Reset(allocator_.Get(), gpu_.psoRoPEFused.Get()))) return false;

    transition(list_.Get(), q_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), k_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), cossin_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        q_buffer, seq_len * num_heads * head_dim,      // t0: Q SRV
        k_buffer, seq_len * num_heads * head_dim,      // t1: K SRV  
        nullptr, 0,                                    // t2: unused
        cossin_buffer, seq_len * (head_dim / 2),       // t3: cossin SRV
        q_buffer, seq_len * num_heads * head_dim,      // u0: Q UAV
        k_buffer, seq_len * num_heads * head_dim       // u1: K UAV
    );

    struct RoPEConstants {
        uint32_t seq_len;
        uint32_t head_dim;
        uint32_t num_heads;
        float theta;
    } constants = { seq_len, head_dim, num_heads, 10000.0f };

    list_->SetComputeRoot32BitConstants(0, 4, &constants, 0);

    uint32_t total_threads = seq_len * num_heads * (head_dim / 2);
    uint32_t groups = (total_threads + 63) / 64;
    if (groups == 0) groups = 1;

    list_->Dispatch(groups, 1, 1);
    return executeAndWait();
}

bool GGUFD3D12Bridge::RecordRoPEFused(ID3D12Resource* q_buffer,
                                      ID3D12Resource* k_buffer,
                                      ID3D12Resource* cossin_buffer,
                                      uint32_t seq_len,
                                      uint32_t head_dim,
                                      uint32_t num_heads) {
    if (!fusedRecording_ || !gpu_.psoRoPEFused || !q_buffer || !k_buffer || !cossin_buffer) return false;

    if (fusedOpsRecorded_ > 0) insertUAVBarrier();

    list_->SetPipelineState(gpu_.psoRoPEFused.Get());

    transition(list_.Get(), q_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), k_buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    transition(list_.Get(), cossin_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    setupDescriptorsForDispatch(
        q_buffer, seq_len * num_heads * head_dim,
        k_buffer, seq_len * num_heads * head_dim,
        nullptr, 0,
        cossin_buffer, seq_len * (head_dim / 2),
        q_buffer, seq_len * num_heads * head_dim,
        k_buffer, seq_len * num_heads * head_dim
    );

    struct RoPEConstants {
        uint32_t seq_len;
        uint32_t head_dim;
        uint32_t num_heads;
        float theta;
    } constants = { seq_len, head_dim, num_heads, 10000.0f };

    list_->SetComputeRoot32BitConstants(0, 4, &constants, 0);

    uint32_t total_threads = seq_len * num_heads * (head_dim / 2);
    uint32_t groups = (total_threads + 63) / 64;
    if (groups == 0) groups = 1;

    list_->Dispatch(groups, 1, 1);
    fusedOpsRecorded_++;
    return true;
}

