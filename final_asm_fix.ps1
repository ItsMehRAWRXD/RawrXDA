
$filePath = "d:\rawrxd\src\asm\quantum_beaconism_backend.asm"
$content = Get-Content $filePath -Raw

# 1. Revert any accidental KERNEL_TYPE_ back to Kernel_
$content = $content -replace "KERNEL_TYPE_NF4_Decompress", "Kernel_NF4_Decompress"
$content = $content -replace "KERNEL_TYPE_PREFETCH", "Kernel_Prefetch"
$content = $content -replace "KERNEL_TYPE_COPY", "Kernel_Copy"

# 2. Fix the specific names expected by the calls at 580+
# They expect RawrXD_Kernel_..._Impl
$content = $content -replace "Kernel_NF4_Decompress_Impl", "RawrXD_Kernel_NF4_Decompress_Impl"
$content = $content -replace "Kernel_Prefetch_Impl", "RawrXD_Kernel_Prefetch_Impl"
$content = $content -replace "Kernel_Copy_Impl", "RawrXD_Kernel_Copy_Impl"

# 3. Ensure the PROCs at the top use the RawrXD_ names
# (Already handled by step 2 if they were named ..._Impl)

# 4. Fix the dispatch table at 1139 to use these names
$content = $content -replace "OFFSET Kernel_NF4_Decompress", "OFFSET RawrXD_Kernel_NF4_Decompress_Impl"
$content = $content -replace "OFFSET RawrXD_Kernel_Prefetch", "OFFSET RawrXD_Kernel_Prefetch_Impl"
$content = $content -replace "OFFSET Kernel_Copy", "OFFSET RawrXD_Kernel_Copy_Impl"

# 5. Handle any remaining KERNEL_TYPE_PREFETCH_Impl that might be in calls
$content = $content -replace "RawrXD_KERNEL_TYPE_PREFETCH_Impl", "RawrXD_Kernel_Prefetch_Impl"

Set-Content $filePath $content
