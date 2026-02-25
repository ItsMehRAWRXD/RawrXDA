; ═══════════════════════════════════════════════════════════════════════════════
; RawrXD_Shared_Data.asm
; Shared definitions for linking standalone interconnect modules
; ═══════════════════════════════════════════════════════════════════════════════


; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc

.DATA
ALIGN 16

PUBLIC RawrXD_DMA_RingBuffer_Base
RawrXD_DMA_RingBuffer_Base QWORD 0

PUBLIC RawrXD_DMA_RingBuffer_ConsumerOffset
RawrXD_DMA_RingBuffer_ConsumerOffset QWORD 0

PUBLIC RawrXD_DMA_RingBuffer_ProducerOffset
RawrXD_DMA_RingBuffer_ProducerOffset QWORD 0

PUBLIC RawrXD_DMA_RingBuffer_Lock
RawrXD_DMA_RingBuffer_Lock QWORD 0 ; Pointer to CRITICAL_SECTION

PUBLIC RawrXD_DMA_RingBuffer_Semaphore
RawrXD_DMA_RingBuffer_Semaphore QWORD 0 ; Handle

END
