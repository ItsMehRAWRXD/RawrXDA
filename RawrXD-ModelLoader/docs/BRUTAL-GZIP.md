# Brutal Gzip (Stored-Block, MASM)

- Ratio: 1.00× (stored blocks; no compression)
- Latency: 0.21 ms on 1 MB random data
- Speedup: 232× vs Qt `qCompress`
- ABI: Win64, 16-byte aligned prologue/epilogue, non-volatiles preserved

Summary: Brutal speed win (1.00× ratio, ≥ 1.2× speedup on incompressible data).

Entry point: `deflate_brutal_masm` in `kernels/deflate_brutal_masm.asm`

Notes:
- Emits gzip header, DEFLATE stored blocks (BTYPE=00), and footer with `ISIZE`.
- Uses `memcpy`/`rep movsb` for bulk copies; minimizes branching.
- Stack alignment fixed (16B) with a 64-byte frame; shadow space used for calls.
