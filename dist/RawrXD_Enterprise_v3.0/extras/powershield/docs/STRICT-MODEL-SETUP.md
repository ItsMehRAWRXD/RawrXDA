# Strict Agentic Model Setup

## Overview

Create a strict JSON-compliant agentic model variant that requires `ANSWER: {"result":"value"}` format.

## Two Options

### Option 1: From Base Model (bigdaddyg-personalized)
```powershell
.\Create-Strict-Agentic-Model.ps1
```
Creates: `bigdaddyg-personalized-agentic:strict`

### Option 2: From Quantized Q5_K_M File (Recommended)
```powershell
# After moving bigdaddyg-q5_k_m.gguf to D drive
.\Create-Strict-Q5-Model.ps1
```
Or manually:
```powershell
@'
FROM D:\BigDaddyG-Standalone-40GB\model\bigdaddyg-q5_k_m.gguf
SYSTEM "After every tool call you MUST write exactly:
ANSWER: {\"result\":\"<return-value>\"}
with no extra commentary."
'@ | ollama create bigdaddyg-personalized-agentic:strict-q5_k_m -f -
```

Creates: `bigdaddyg-personalized-agentic:strict-q5_k_m`

## Testing

After creation, test with:
```powershell
.\Test-Agentic-Raw.ps1 -Model "bigdaddyg-personalized-agentic:strict-q5_k_m" -MaxIter 10
```

## Expected Results

- ✅ 100% JSON compliance in tool calls
- ✅ All answers in `ANSWER: {"result":"value"}` format
- ✅ No format drift across iterations
- ✅ No hallucinated paths or results

## File Locations

- **Modelfile**: `Modelfiles/bigdaddyg-personalized-agentic-strict-q5.Modelfile`
- **Script**: `Create-Strict-Q5-Model.ps1`
- **Test Script**: `Test-Agentic-Raw.ps1`

## Notes

- The q5_k_m variant is ~4-5 GB (vs 45 GB unquantized)
- Strict prompt enforces JSON-only answers
- Works with improved parser (Path A) as fallback

