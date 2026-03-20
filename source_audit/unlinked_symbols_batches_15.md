# Unlinked Symbol Batches (15 each)

- Source audit: `/workspace/source_audit/unlinked_symbols_production_audit.json`
- Total symbols: **71**
- Batch size: **15**
- Total batches: **5**

## Batch 001 (1-15)

| # | Symbol | Classification | Occurrences | First log line |
|---:|---|---|---:|---:|
| 1 | `sgemv_avx2` | `unlinked_despite_wired` | 2 | 48 |
| 2 | `native_fused_mlp_avx2` | `unlinked_despite_wired` | 2 | 49 |
| 3 | `qgemv_q8_0_avx2` | `unlinked_despite_wired` | 1 | 50 |
| 4 | `qgemv_q4_0_avx2` | `unlinked_despite_wired` | 1 | 51 |
| 5 | `Swarm_ComputeNodeFitness` | `unlinked_despite_wired` | 5 | 54 |
| 6 | `Swarm_BuildPacketHeader` | `unlinked_despite_wired` | 4 | 55 |
| 7 | `Swarm_HeartbeatRecord` | `unlinked_despite_wired` | 3 | 56 |
| 8 | `Swarm_IOCP_Associate` | `unlinked_despite_wired` | 2 | 58 |
| 9 | `Swarm_RingBuffer_Init` | `unlinked_despite_wired` | 3 | 59 |
| 10 | `Swarm_IOCP_Create` | `unlinked_despite_wired` | 1 | 61 |
| 11 | `Swarm_HeartbeatCheck` | `unlinked_despite_wired` | 1 | 62 |
| 12 | `Swarm_ValidatePacketHeader` | `unlinked_despite_wired` | 3 | 67 |
| 13 | `Swarm_XXH64` | `unlinked_despite_wired` | 2 | 68 |
| 14 | `RawrXD::Agentic::AgenticTaskGraph::instance()` | `unlinked_despite_wired` | 1 | 78 |
| 15 | `RawrXD::Embeddings::EmbeddingEngine::instance()` | `unlinked_despite_wired` | 3 | 79 |

## Batch 002 (16-30)

| # | Symbol | Classification | Occurrences | First log line |
|---:|---|---|---:|---:|
| 16 | `RawrXD::Embeddings::EmbeddingEngine::loadModel(RawrXD::Embeddings::EmbeddingModelConfig const&)` | `dissolved_or_external` | 1 | 80 |
| 17 | `RawrXD::Embeddings::EmbeddingEngine::shutdown()` | `dissolved_or_external` | 1 | 82 |
| 18 | `RawrXD::Embeddings::EmbeddingEngine::indexDirectory(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Embeddings::ChunkingConfig const&)` | `dissolved_or_external` | 1 | 84 |
| 19 | `asm_symbol_hash_lookup` | `unlinked_despite_wired` | 1 | 85 |
| 20 | `MC_GapBuffer_LineCount` | `unwired_non_stub` | 20 | 86 |
| 21 | `MC_GapBuffer_Length` | `unwired_non_stub` | 8 | 88 |
| 22 | `MC_GapBuffer_GetLine` | `unwired_non_stub` | 12 | 90 |
| 23 | `MC_TokenizeLine` | `unlinked_despite_wired` | 1 | 93 |
| 24 | `MC_GapBuffer_Destroy` | `unwired_non_stub` | 32 | 97 |
| 25 | `MC_GapBuffer_Init` | `unwired_non_stub` | 5 | 98 |
| 26 | `MC_GapBuffer_Insert` | `unwired_non_stub` | 8 | 99 |
| 27 | `MC_GapBuffer_Delete` | `unwired_non_stub` | 4 | 105 |
| 28 | `RawrXD::Debugger::NativeDebuggerEngine::getStats() const` | `dissolved_or_external` | 1 | 176 |
| 29 | `RawrXD::Debugger::NativeDebuggerEngine::addBreakpointBySymbol(std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> > const&, RawrXD::Debugger::BreakpointType)` | `dissolved_or_external` | 1 | 177 |
| 30 | `KQuant_DequantizeQ4_K` | `unlinked_despite_wired` | 3 | 178 |

## Batch 003 (31-45)

| # | Symbol | Classification | Occurrences | First log line |
|---:|---|---|---:|---:|
| 31 | `KQuant_DequantizeQ6_K` | `unlinked_despite_wired` | 3 | 179 |
| 32 | `Quant_DequantQ4_0` | `unlinked_despite_wired` | 3 | 180 |
| 33 | `Quant_DequantQ8_0` | `unlinked_despite_wired` | 3 | 181 |
| 34 | `asm_kquant_cpuid_check` | `unlinked_despite_wired` | 1 | 186 |
| 35 | `KQuant_DequantizeF16` | `unlinked_despite_wired` | 1 | 187 |
| 36 | `swarm_build_discovery_packet` | `unlinked_despite_wired` | 2 | 192 |
| 37 | `swarm_receive_header` | `unlinked_despite_wired` | 1 | 193 |
| 38 | `swarm_compute_layer_crc32` | `unlinked_despite_wired` | 1 | 194 |
| 39 | `VecDb_Init` | `unwired_non_stub` | 2 | 196 |
| 40 | `GapCloser_GetPerfCounters` | `unlinked_despite_wired` | 1 | 197 |
| 41 | `Git_ExtractContext` | `unlinked_despite_wired` | 1 | 198 |
| 42 | `Composer_GetState` | `dissolved_or_external` | 1 | 199 |
| 43 | `Composer_GetTxCount` | `dissolved_or_external` | 2 | 200 |
| 44 | `Crdt_GetDocLength` | `dissolved_or_external` | 5 | 201 |
| 45 | `Crdt_GetLamport` | `dissolved_or_external` | 1 | 202 |

## Batch 004 (46-60)

| # | Symbol | Classification | Occurrences | First log line |
|---:|---|---|---:|---:|
| 46 | `VecDb_GetNodeCount` | `dissolved_or_external` | 4 | 203 |
| 47 | `g_VecDbMaxLevel` | `dissolved_or_external` | 1 | 204 |
| 48 | `g_VecDbEntryPoint` | `dissolved_or_external` | 1 | 205 |
| 49 | `g_VecDbNodes` | `unwired_non_stub` | 1 | 206 |
| 50 | `VecDb_Delete` | `unwired_non_stub` | 1 | 207 |
| 51 | `Composer_AddFileOp` | `dissolved_or_external` | 1 | 208 |
| 52 | `Composer_BeginTransaction` | `unwired_non_stub` | 1 | 210 |
| 53 | `Crdt_InitDocument` | `unwired_non_stub` | 1 | 211 |
| 54 | `Git_SetBranch` | `dissolved_or_external` | 1 | 212 |
| 55 | `Git_SetCommitHash` | `dissolved_or_external` | 1 | 213 |
| 56 | `Composer_Commit` | `unwired_non_stub` | 1 | 214 |
| 57 | `Crdt_InsertText` | `dissolved_or_external` | 1 | 217 |
| 58 | `Crdt_DeleteText` | `dissolved_or_external` | 1 | 220 |
| 59 | `VecDb_Search` | `unwired_non_stub` | 2 | 223 |
| 60 | `VecDb_Insert` | `dissolved_or_external` | 2 | 224 |

## Batch 005 (61-71)

| # | Symbol | Classification | Occurrences | First log line |
|---:|---|---|---:|---:|
| 61 | `BeaconRouterInit` | `unlinked_despite_wired` | 1 | 229 |
| 62 | `asm_scsi_inquiry_quick` | `unlinked_despite_wired` | 1 | 230 |
| 63 | `asm_scsi_read_capacity` | `unlinked_despite_wired` | 3 | 231 |
| 64 | `asm_scsi_hammer_read` | `unlinked_despite_wired` | 1 | 232 |
| 65 | `asm_extract_bridge_key` | `unlinked_despite_wired` | 1 | 234 |
| 66 | `RawrXD_WalkImports` | `unlinked_despite_wired` | 1 | 236 |
| 67 | `RawrXD_WalkExports` | `unlinked_despite_wired` | 1 | 237 |
| 68 | `OSExplorerInterceptor::OSExplorerInterceptor()` | `unwired_non_stub` | 1 | 238 |
| 69 | `OSExplorerInterceptor::Initialize(unsigned long, void (*)(void*))` | `unwired_non_stub` | 1 | 239 |
| 70 | `OSExplorerInterceptor::StartInterception()` | `unwired_non_stub` | 1 | 240 |
| 71 | `OSExplorerInterceptor::~OSExplorerInterceptor()` | `unwired_non_stub` | 1 | 241 |

