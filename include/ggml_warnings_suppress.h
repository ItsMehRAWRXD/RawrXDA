/**
 * GGML Upstream Warnings Suppression Header
 *
 * This header suppresses known upstream issues in GGML's Vulkan backend
 * and other components. These warnings are non-critical and originate from
 * upstream GGML library code that is outside the scope of this project.
 *
 * Suppressed Warnings:
 * - C4530: Exception handling in /EHsc mode (upstream Vulkan SDK headers)
 * - C4003: Insufficient macro arguments (pre-processor quirk in SDK)
 * - C4319: Zero-extending conversion in type casts (intended behavior)
 *
 * These suppressions are applied only when including GGML headers via this wrapper.
 * Custom project code does not use these suppressions.
 */

#pragma once

#if defined(_MSC_VER)
    // MSVC-specific: Suppress upstream GGML Vulkan backend warnings
    #pragma warning(push)
    #pragma warning(disable: 4530)  // C4530: Exception handling suppression for EHsc mode
    #pragma warning(disable: 4003)  // C4003: Macro argument count mismatch in SDK headers
    #pragma warning(disable: 4319)  // C4319: Zero-extending type cast (explicit in Vulkan)
    #pragma warning(disable: 4005)  // C4005: Macro redefinition (Windows SDK quirk)
#endif

// Include all GGML headers here
#include <ggml.h>
#include <ggml-alloc.h>
#include <ggml-backend.h>
#include <ggml-cpu.h>

// Check if Vulkan backend is available
#ifdef GGML_USE_VULKAN
    #include <ggml-vulkan.h>
#endif

#ifdef GGML_USE_CUDA
    #include <ggml-cuda.h>
#endif

#ifdef GGML_USE_METAL
    #include <ggml-metal.h>
#endif

#if defined(_MSC_VER)
    #pragma warning(pop)  // Restore warning state
#endif

/**
 * Production Readiness Notes:
 * ===========================
 * 
 * 1. Warning Suppression Strategy:
 *    - Warnings are suppressed at the header level, not globally
 *    - Project code remains subject to all compiler warnings
 *    - Only external dependencies benefit from suppression
 *
 * 2. Vulkan Backend Warnings:
 *    The Vulkan backend in GGML may emit warnings due to:
 *    - SDK header macro definitions using variadic macros
 *    - Type conversions required by Vulkan API specifications
 *    - Exception handling requirements in mixed C/C++ code
 *
 * 3. Upstream Status:
 *    - These warnings are documented upstream
 *    - GGML team prioritizes functionality over warning-free builds
 *    - No functional impact on compilation or runtime behavior
 *
 * 4. Monitoring:
 *    - Build logs captured in: build_warnings.log
 *    - CI/CD should note suppressed warning count for audit
 *    - Review upstream GGML releases for fixes
 */
