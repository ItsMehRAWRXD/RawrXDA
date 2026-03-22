;;; RawrXD / BGzipXD — chat WASM ABI (host-shaped, non-streaming).
;;;
;;; Contract (mirrors main-process Ollama lane: one user blob in, one assistant blob out):
;;; - JS writes UTF-8 prompt at rawrxd_input_base, length <= rawrxd_input_cap.
;;; - rawrxd_chat(inPtr, inLen, outPtr, outCap) writes up to min(inLen, outCap) bytes starting at outPtr;
;;;   return value = byte count written (same as legacy byte-echo bootstrap).
;;; - Layout constants are exported so the host never drifts from this module (reverse-ABI).
;;; - Memory: initial pages must cover input region + output region (see sizes below).

(module
  (memory (export "memory") 2)

  ;; --- Exported layout (read by JS instead of duplicating magic numbers) ---
  (func (export "rawrxd_abi_version") (result i32)
    (i32.const 2))
  (func (export "rawrxd_input_base") (result i32)
    (i32.const 0))
  (func (export "rawrxd_input_cap") (result i32)
    (i32.const 32768))
  (func (export "rawrxd_output_base") (result i32)
    (i32.const 65536))
  (func (export "rawrxd_output_cap") (result i32)
    (i32.const 65536))

  ;; Bootstrap runner: copy-through (generative runner replaces this body; exports stay stable).
  (func (export "rawrxd_chat")
    (param $inPtr i32) (param $inLen i32) (param $outPtr i32) (param $outCap i32) (result i32)
    (local $i i32)
    (local $n i32)
    (local.get $inLen)
    (local.get $outCap)
    (i32.gt_u)
    (if (result i32)
      (then (local.get $outCap))
      (else (local.get $inLen))
    )
    (local.set $n)
    (i32.const 0)
    (local.set $i)
    (block $done
      (loop $again
        (local.get $i)
        (local.get $n)
        (i32.ge_u)
        (br_if $done)
        (local.get $outPtr)
        (local.get $i)
        (i32.add)
        (local.get $inPtr)
        (local.get $i)
        (i32.add)
        (i32.load8_u)
        (i32.store8)
        (local.get $i)
        (i32.const 1)
        (i32.add)
        (local.set $i)
        (br $again)
      )
    )
    (local.get $n)
  )
)
