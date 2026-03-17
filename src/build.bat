@echo off
REM Build script for RawrXD Zero Transformer Kernel
REM Requires Visual Studio Build Tools with MASM64

echo Building RawrXD Zero Transformer Kernel...

REM Assemble all ASM files
ml64 /c /Fo"matrix_mul_avx512.obj" matrix_mul_avx512.asm
ml64 /c /Fo"gguf_graph_interpreter.obj" gguf_graph_interpreter.asm
ml64 /c /Fo"kv_cache.obj" kv_cache.asm
ml64 /c /Fo"tokenizer.obj" tokenizer.asm
ml64 /c /Fo"RawrXD_ZeroTransformer.obj" RawrXD_ZeroTransformer.asm

REM Link into DLL
link /DLL /OUT:RawrXD_ZeroTransformer.dll matrix_mul_avx512.obj gguf_graph_interpreter.obj kv_cache.obj tokenizer.obj RawrXD_ZeroTransformer.obj kernel32.lib

echo Build complete. Check for errors above.