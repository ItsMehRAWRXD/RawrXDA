# Dequantization Implementation - piram_reverse_quantization.asm

## Overview
Comprehensive implementation of dequantization functions for Q4, Q5, and Q8 quantization formats, supporting conversion to both FP16 and FP32 floating-point representations.

## Implemented Functions

### Public API Functions

#### Q4 Dequantization
- `ReverseQuant_Q4toF16` - Dequantizes Q4 quantized blocks to FP16
- `ReverseQuant_Q4toF32` - Dequantizes Q4 quantized blocks to FP32

#### Q5 Dequantization  
- `ReverseQuant_Q5toF16` - Dequantizes Q5 quantized blocks to FP16
- `ReverseQuant_Q5toF32` - Dequantizes Q5 quantized blocks to FP32

#### Q8 Dequantization
- `ReverseQuant_Q8toF16` - Dequantizes Q8 quantized blocks to FP16
- `ReverseQuant_Q8toF32` - Dequantizes Q8 quantized blocks to FP32

#### Utility Functions
- `ReverseQuant_Init` - Initializes dequantization lookup tables
- `ReverseQuant_GetFormat` - Detects quantization format from tensor header
- `ReverseQuant_Batch` - Batch dequantization dispatcher supporting all formats

### Helper Functions (Low-level bit conversion)
- `Dequant4BitToF16` - Converts single 4-bit value to FP16
- `Dequant5BitToF16` - Converts single 5-bit value to FP16
- `Dequant8BitToF16` - Converts single 8-bit value to FP16
- `Dequant4BitToF32` - Converts single 4-bit value to FP32
- `Dequant5BitToF32` - Converts single 5-bit value to FP32
- `Dequant8BitToF32` - Converts single 8-bit value to FP32

### Lookup Table Builders
- `BuildQ4Lookup` - Builds Q4 dequantization lookup table
- `BuildQ8Lookup` - Builds Q8 dequantization lookup table

## Block Structure

### Q4Block
```asm
Q4Block STRUCT
    scale   dw ?        ; FP16 scale factor
    data    db 16 dup(?)  ; 32 x 4-bit values (2 per byte)
Q4Block ENDS
```
- 32 values per block
- Each value: 4 bits
- Total: 16 bytes data + 2 bytes scale = 18 bytes

### Q5Block
```asm
Q5Block STRUCT
    scale   dw ?        ; FP16 scale factor
    min     dw ?        ; FP16 minimum value (Q5_1 only)
    data    db 20 dup(?)  ; 32 x 5-bit values
Q5Block ENDS
```
- 32 values per block
- Each value: 5 bits
- Total: 20 bytes data + 2 bytes scale = 22 bytes

### Q8Block
```asm
Q8Block STRUCT
    scale   dw ?        ; FP16 scale factor
    min     dw ?        ; FP16 minimum value (Q8_1 only)
    data    db 32 dup(?)  ; 32 x 8-bit values
Q8Block ENDS
```
- 32 values per block
- Each value: 8 bits (signed)
- Total: 32 bytes data + 2 bytes scale = 34 bytes

## Dequantization Formulas

### Q4 Dequantization
```
value = (q4_value - 8) * scale
Range: q4_value ∈ [0, 15] → signed range [-8, 7]
```

### Q5 Dequantization
```
value = (q5_value - 16) * scale
Range: q5_value ∈ [0, 31] → signed range [-16, 15]
```

### Q8 Dequantization
```
value = q8_value * scale
Range: q8_value ∈ [-128, 127] (signed)
```

## Statistics Tracking

The implementation tracks dequantization statistics:
- `g_Stats_Q4Blocks` - Number of Q4 blocks dequantized
- `g_Stats_Q5Blocks` - Number of Q5 blocks dequantized
- `g_Stats_Q8Blocks` - Number of Q8 blocks dequantized
- `g_Stats_TotalValues` - Total number of values dequantized

## Supported Quantization Formats

Dispatched by `ReverseQuant_Batch`:
- `QUANT_FMT_Q4_0` (2) - Q4 with global scale
- `QUANT_FMT_Q4_1` (3) - Q4 with per-block scale and minimum
- `QUANT_FMT_Q5_0` (6) - Q5 with global scale
- `QUANT_FMT_Q5_1` (7) - Q5 with per-block scale and minimum
- `QUANT_FMT_Q8_0` (8) - Q8 with global scale
- `QUANT_FMT_Q8_1` (9) - Q8 with per-block scale and minimum

## Performance Considerations

1. **Block Processing**: Each function processes blocks of 32 values for cache efficiency
2. **Lookup Tables**: Optional lookup table optimization for repeated conversions
3. **Integer Arithmetic**: Uses integer arithmetic (imul + sar) for scale multiplication
4. **Register Allocation**: Efficient use of x86 registers (EAX, EBX, ECX, EDX, ESI, EDI, EBP)

## Future Enhancements

1. SSE/AVX vectorization for batch processing
2. Complete Q5 bit boundary handling (currently simplified)
3. K-variant format support (Q4_K, Q5_K, Q8_K with per-group scales)
4. Lookup table population in BuildQ4Lookup/BuildQ8Lookup
5. FP16 arithmetic instructions (if available on target platform)

## Testing Recommendations

1. Verify dequantized values match quantization round-trip tests
2. Check statistics tracking for block and value counts
3. Test all format variants (Q4_0/1, Q5_0/1, Q8_0/1)
4. Validate boundary conditions for min/max values
5. Performance benchmark against reference implementations
