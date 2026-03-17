// ============================================================================
// phi3_merges.inc - Auto-generated Phi-3-Mini BPE Merge Table
// ============================================================================
// Generated from: models\phi-3-mini\tokenizer.json
// Date: 2026-03-12 14:53:18
// Vocab size: 97
// Merges: 27
// ============================================================================
//
// BPE Merge Operation Format:
//   { left_token_id, right_token_id, resulting_token_id }
// 
// Sorted by merge rank (index in tokenizer.json 'merges' array)
// Higher rank = earlier merge priority.
//
// Usage in bridge_layer.cpp:
//   static const BPEMerge g_phi3_merges[PHI3_NUM_MERGES] = {
//       #include "phi3_merges.inc"
//   };
// ============================================================================
    {64, 77, 257},    {72, 77, 259},    {82, 83, 261},    {68, 82, 262},    {64, 83, 263},    {68, 77, 264},    {83, 78, 265},    {78, 77, 266},    {81, 68, 267},    {68, 67, 268},
    {64, 75, 269},    {75, 68, 270},    {72, 82, 271},    {68, 76, 272},    {66, 78, 273},    {64, 81, 274},    {67, 68, 275},    {82, 68, 276},    {83, 71, 277},    {66, 71, 278},
    {64, 82, 279},    {78, 84, 280},    {75, 78, 281},    {86, 68, 282},    {84, 82, 283},    {82, 71, 284},    {68, 81, 285},
    {0, 0, 0}  // Sentinel

