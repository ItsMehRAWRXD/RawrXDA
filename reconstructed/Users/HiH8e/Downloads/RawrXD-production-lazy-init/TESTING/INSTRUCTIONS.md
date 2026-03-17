# Test Model Files for Phase 1 Universal Format Loader

## SafeTensors Test File
File: test_simple.safetensors
- Contains minimal SafeTensors structure
- 8-byte metadata size header
- Simple JSON metadata
- Small tensor data block

## PyTorch Test File  
File: test_simple.pt
- Contains ZIP signature (0x50 0x4B 0x03 0x04)
- Minimal PyTorch format structure

## Testing Instructions
1. Launch RawrXD-QtShell.exe
2. Use File → Open Model
3. Select test_simple.safetensors
4. Verify format detection works
5. Verify conversion pipeline executes
6. Repeat with test_simple.pt

## Expected Results
- Format should be detected as SafeTensors/PyTorch
- Conversion to GGUF should happen transparently
- Model should appear in loaded models list
- Inference should be possible (with dummy data)

## File Structure
```
test_simple.safetensors:
  [8 bytes] metadata size (0x0800000000000000)
  [JSON] {"test":"model"}
  [8 bytes] tensor data (0x0102030405060708)

test_simple.pt:
  [4 bytes] ZIP signature (0x504B0304)
  [100 bytes] dummy data
```