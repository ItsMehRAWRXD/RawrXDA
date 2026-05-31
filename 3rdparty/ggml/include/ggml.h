#pragma once

// Build metadata is typically injected by CMake; provide safe fallbacks.
#ifndef GGML_RXD_VERSION
#define GGML_RXD_VERSION "unknown"
#endif

#ifndef GGML_RXD_COMMIT
#define GGML_RXD_COMMIT "unknown"
#endif

//
// GGML Tensor Library
//
// This documentation is still a work in progress.
// If you wish some specific topics to be covered, feel free to drop a comment:
//
//   https://github.com/ggerganov/whisper.cpp/issues/40
//
// ## Overview
//
// This library implements:
//
//  - a set of tensor operations
//  - automatic differentiation
//  - basic optimization algorithms
//
// The aim of this library is to provide a minimalistic approach for various machine learning tasks. This includes,
// but is not limited to, the following:
//
//  - linear regression
//  - support vector machines
//  - neural networks
//
// The library allows the user to define a certain function using the available tensor operations. This function
// definition is represented internally via a computation graph. Each tensor operation in the function definition
// corresponds to a node in the graph. Having the computation graph defined, the user can choose to compute the
// function's value and/or its gradient with respect to the input variables. Optionally, the function can be optimized
// using one of the available optimization algorithms.
//
// For example, here we define the function: f(x) = a*x^2 + b
//
//   {
//       struct ggml_rxd_init_params params = {
//           .mem_size   = 16*1024*1024,
//           .mem_buffer = NULL,
//       };
//
//       // memory allocation happens here
//       struct ggml_rxd_context * ctx = ggml_rxd_init(params);
//
//       struct ggml_rxd_tensor * x = ggml_rxd_new_tensor_1d(ctx, GGML_RXD_TYPE_F32, 1);
//
//       ggml_rxd_set_param(ctx, x); // x is an input variable
//
//       struct ggml_rxd_tensor * a  = ggml_rxd_new_tensor_1d(ctx, GGML_RXD_TYPE_F32, 1);
//       struct ggml_rxd_tensor * b  = ggml_rxd_new_tensor_1d(ctx, GGML_RXD_TYPE_F32, 1);
//       struct ggml_rxd_tensor * x2 = ggml_rxd_mul(ctx, x, x);
//       struct ggml_rxd_tensor * f  = ggml_rxd_add(ctx, ggml_rxd_mul(ctx, a, x2), b);
//
//       ...
//   }
//
// Notice that the function definition above does not involve any actual computation. The computation is performed only
// when the user explicitly requests it. For example, to compute the function's value at x = 2.0:
//
//   {
//       ...
//
//       struct ggml_rxd_cgraph * gf = ggml_rxd_new_graph(ctx);
//       ggml_rxd_build_forward_expand(gf, f);
//
//       // set the input variable and parameter values
//       ggml_rxd_set_f32(x, 2.0f);
//       ggml_rxd_set_f32(a, 3.0f);
//       ggml_rxd_set_f32(b, 4.0f);
//
//       ggml_rxd_graph_compute_with_ctx(ctx, &gf, n_threads);
//
//       printf("f = %f\n", ggml_rxd_get_f32_1d(f, 0));
//
//       ...
//   }
//
// The actual computation is performed in the ggml_rxd_graph_compute() function.
//
// The ggml_rxd_new_tensor_...() functions create new tensors. They are allocated in the memory buffer provided to the
// ggml_rxd_init() function. You have to be careful not to exceed the memory buffer size. Therefore, you have to know
// in advance how much memory you need for your computation. Alternatively, you can allocate a large enough memory
// and after defining the computation graph, call the ggml_rxd_used_mem() function to find out how much memory was
// actually needed.
//
// The ggml_rxd_set_param() function marks a tensor as an input variable. This is used by the automatic
// differentiation and optimization algorithms.
//
// The described approach allows to define the function graph once and then compute its forward or backward graphs
// multiple times. All computations will use the same memory buffer allocated in the ggml_rxd_init() function. This way
// the user can avoid the memory allocation overhead at runtime.
//
// The library supports multi-dimensional tensors - up to 4 dimensions. The FP16 and FP32 data types are first class
// citizens, but in theory the library can be extended to support FP8 and integer data types.
//
// Each tensor operation produces a new tensor. Initially the library was envisioned to support only the use of unary
// and binary operations. Most of the available operations fall into one of these two categories. With time, it became
// clear that the library needs to support more complex operations. The way to support these operations is not clear
// yet, but a few examples are demonstrated in the following operations:
//
//   - ggml_rxd_permute()
//   - ggml_rxd_conv_1d_1s()
//   - ggml_rxd_conv_1d_2s()
//
// For each tensor operator, the library implements a forward and backward computation function. The forward function
// computes the output tensor value given the input tensor values. The backward function computes the adjoint of the
// input tensors given the adjoint of the output tensor. For a detailed explanation of what this means, take a
// calculus class, or watch the following video:
//
//   What is Automatic Differentiation?
//   https://www.youtube.com/watch?v=wG_nF1awSSY
//
//
// ## Tensor data (struct ggml_rxd_tensor)
//
// The tensors are stored in memory via the ggml_rxd_tensor struct. The structure provides information about the size of
// the tensor, the data type, and the memory buffer where the tensor data is stored. Additionally, it contains
// pointers to the "source" tensors - i.e. the tensors that were used to compute the current tensor. For example:
//
//   {
//       struct ggml_rxd_tensor * c = ggml_rxd_add(ctx, a, b);
//
//       assert(c->src[0] == a);
//       assert(c->src[1] == b);
//   }
//
// The multi-dimensional tensors are stored in row-major order. The ggml_rxd_tensor struct contains fields for the
// number of elements in each dimension ("ne") as well as the number of bytes ("nb", a.k.a. stride). This allows
// to store tensors that are not contiguous in memory, which is useful for operations such as transposition and
// permutation. All tensor operations have to take the stride into account and not assume that the tensor is
// contiguous in memory.
//
// The data of the tensor is accessed via the "data" pointer. For example:
//
//   {
//       const int nx = 2;
//       const int ny = 3;
//
//       struct ggml_rxd_tensor * a = ggml_rxd_new_tensor_2d(ctx, GGML_RXD_TYPE_F32, nx, ny);
//
//       for (int y = 0; y < ny; y++) {
//           for (int x = 0; x < nx; x++) {
//               *(float *) ((char *) a->data + y*a->nb[1] + x*a->nb[0]) = x + y;
//           }
//       }
//
//       ...
//   }
//
// Alternatively, there are helper functions, such as ggml_rxd_get_f32_1d() and ggml_rxd_set_f32_1d() that can be used.
//
// ## The matrix multiplication operator (ggml_rxd_mul_mat)
//
// TODO
//
//
// ## Multi-threading
//
// TODO
//
//
// ## Overview of ggml.c
//
// TODO
//
//
// ## SIMD optimizations
//
// TODO
//
//
// ## Debugging ggml
//
// TODO
//
//

#ifdef GGML_RXD_SHARED
#    if defined(_WIN32) && !defined(__MINGW32__)
#        ifdef GGML_RXD_BUILD
#            define GGML_RXD_API __declspec(dllexport) extern
#        else
#            define GGML_RXD_API __declspec(dllimport) extern
#        endif
#    else
#        define GGML_RXD_API __attribute__ ((visibility ("default"))) extern
#    endif
#else
#    define GGML_RXD_API extern
#endif

// TODO: support for clang
#ifdef __GNUC__
#    define GGML_RXD_DEPRECATED(func, hint) func __attribute__((deprecated(hint)))
#elif defined(_MSC_VER)
#    define GGML_RXD_DEPRECATED(func, hint) __declspec(deprecated(hint)) func
#else
#    define GGML_RXD_DEPRECATED(func, hint) func
#endif

#ifndef __GNUC__
#    define GGML_RXD_ATTRIBUTE_FORMAT(...)
#elif defined(__MINGW32__) && !defined(__clang__)
#    define GGML_RXD_ATTRIBUTE_FORMAT(...) __attribute__((format(gnu_printf, __VA_ARGS__)))
#else
#    define GGML_RXD_ATTRIBUTE_FORMAT(...) __attribute__((format(printf, __VA_ARGS__)))
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GGML_RXD_FILE_MAGIC   0x67676d6c // "ggml"
#define GGML_RXD_FILE_VERSION 2

#define GGML_RXD_QNT_VERSION        2    // bump this on quantization format changes
#define GGML_RXD_QNT_VERSION_FACTOR 1000 // do not change this

#define GGML_RXD_MAX_DIMS           4
#define GGML_RXD_MAX_PARAMS         2048
#define GGML_RXD_MAX_SRC            10
#define GGML_RXD_MAX_N_THREADS      512
#define GGML_RXD_MAX_OP_PARAMS      64

#ifndef GGML_RXD_MAX_NAME
#   define GGML_RXD_MAX_NAME        64
#endif

#define GGML_RXD_DEFAULT_N_THREADS  4
#define GGML_RXD_DEFAULT_GRAPH_SIZE 2048

#if UINTPTR_MAX == 0xFFFFFFFF
    #define GGML_RXD_MEM_ALIGN 4
#else
    #define GGML_RXD_MEM_ALIGN 16
#endif

#define GGML_RXD_EXIT_SUCCESS 0
#define GGML_RXD_EXIT_ABORTED 1

// TODO: convert to enum https://github.com/ggml-org/llama.cpp/pull/16187#discussion_r2388538726
#define GGML_RXD_ROPE_TYPE_NORMAL 0
#define GGML_RXD_ROPE_TYPE_NEOX   2
#define GGML_RXD_ROPE_TYPE_MROPE  8
#define GGML_RXD_ROPE_TYPE_VISION 24
#define GGML_RXD_ROPE_TYPE_IMROPE 40 // binary: 101000

#define GGML_RXD_MROPE_SECTIONS   4

#define GGML_RXD_UNUSED(x) (void)(x)
#ifdef __CUDACC__
template<typename... Args>
__host__ __device__ constexpr inline void ggml_rxd_unused_vars_impl(Args&&...) noexcept {}
#define GGML_RXD_UNUSED_VARS(...) ggml_rxd_unused_vars_impl(__VA_ARGS__)
#else
#define GGML_RXD_UNUSED_VARS(...) do { (void)sizeof((__VA_ARGS__, 0)); } while(0)
#endif // __CUDACC__

#define GGML_RXD_PAD(x, n) (((x) + (n) - 1) & ~((n) - 1))

#ifndef NDEBUG
#   define GGML_RXD_UNREACHABLE() do { fprintf(stderr, "statement should be unreachable\n"); abort(); } while(0)
#elif defined(__GNUC__)
#   define GGML_RXD_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#   define GGML_RXD_UNREACHABLE() __assume(0)
#else
#   define GGML_RXD_UNREACHABLE() ((void) 0)
#endif

#ifdef __cplusplus
#   define GGML_RXD_NORETURN [[noreturn]]
#elif defined(_MSC_VER)
#   define GGML_RXD_NORETURN __declspec(noreturn)
#else
#   define GGML_RXD_NORETURN _Noreturn
#endif

#define GGML_RXD_ABORT(...) ggml_rxd_abort(__FILE__, __LINE__, __VA_ARGS__)
#define GGML_RXD_ASSERT(x) if (!(x)) GGML_RXD_ABORT("GGML_RXD_ASSERT(%s) failed", #x)

// used to copy the number of elements and stride in bytes of tensors into local variables.
// main purpose is to reduce code duplication and improve readability.
//
// example:
//
//    GGML_RXD_TENSOR_LOCALS(int64_t, ne1, src1, ne);
//    GGML_RXD_TENSOR_LOCALS(size_t,  nb1, src1, nb);
//
#define GGML_RXD_TENSOR_LOCALS_1(type, prefix, pointer, array) \
    const type prefix##0 = (pointer) ? (pointer)->array[0] : 0; \
    GGML_RXD_UNUSED(prefix##0);
#define GGML_RXD_TENSOR_LOCALS_2(type, prefix, pointer, array) \
    GGML_RXD_TENSOR_LOCALS_1    (type, prefix, pointer, array) \
    const type prefix##1 = (pointer) ? (pointer)->array[1] : 0; \
    GGML_RXD_UNUSED(prefix##1);
#define GGML_RXD_TENSOR_LOCALS_3(type, prefix, pointer, array) \
    GGML_RXD_TENSOR_LOCALS_2    (type, prefix, pointer, array) \
    const type prefix##2 = (pointer) ? (pointer)->array[2] : 0; \
    GGML_RXD_UNUSED(prefix##2);
#define GGML_RXD_TENSOR_LOCALS(type, prefix, pointer, array) \
    GGML_RXD_TENSOR_LOCALS_3  (type, prefix, pointer, array) \
    const type prefix##3 = (pointer) ? (pointer)->array[3] : 0; \
    GGML_RXD_UNUSED(prefix##3);

#define GGML_RXD_TENSOR_UNARY_OP_LOCALS \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne0, src0, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb0, src0, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne,  dst,  ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb,  dst,  nb)

#define GGML_RXD_TENSOR_BINARY_OP_LOCALS \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne0, src0, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb0, src0, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne1, src1, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb1, src1, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne,  dst,  ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb,  dst,  nb)

#define GGML_RXD_TENSOR_TERNARY_OP_LOCALS \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne0, src0, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb0, src0, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne1, src1, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb1, src1, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne2, src2, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb2, src2, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne,  dst,  ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb,  dst,  nb)

#define GGML_RXD_TENSOR_BINARY_OP_LOCALS01 \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne0, src0, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb0, src0, nb) \
    GGML_RXD_TENSOR_LOCALS(int64_t, ne1, src1, ne) \
    GGML_RXD_TENSOR_LOCALS(size_t,  nb1, src1, nb)

#ifdef  __cplusplus
extern "C" {
#endif

    // Function type used in fatal error callbacks
    typedef void (*ggml_rxd_abort_callback_t)(const char * error_message);

    // Set the abort callback (passing null will restore original abort functionality: printing a message to stdout)
    // Returns the old callback for chaining
    GGML_RXD_API ggml_rxd_abort_callback_t ggml_rxd_set_abort_callback(ggml_rxd_abort_callback_t callback);

    GGML_RXD_NORETURN GGML_RXD_ATTRIBUTE_FORMAT(3, 4)
    GGML_RXD_API void ggml_rxd_abort(const char * file, int line, const char * fmt, ...);

    enum ggml_rxd_status {
        GGML_RXD_STATUS_ALLOC_FAILED = -2,
        GGML_RXD_STATUS_FAILED = -1,
        GGML_RXD_STATUS_SUCCESS = 0,
        GGML_RXD_STATUS_ABORTED = 1,
    };

    // get ggml_rxd_status name string
    GGML_RXD_API const char * ggml_rxd_status_to_string(enum ggml_rxd_status status);

    // ieee 754-2008 half-precision float16
    // todo: make this not an integral type
    typedef uint16_t ggml_rxd_fp16_t;
    GGML_RXD_API float       ggml_rxd_fp16_to_fp32(ggml_rxd_fp16_t);
    GGML_RXD_API ggml_rxd_fp16_t ggml_rxd_fp32_to_fp16(float);
    GGML_RXD_API void        ggml_rxd_fp16_to_fp32_row(const ggml_rxd_fp16_t *, float *, int64_t);
    GGML_RXD_API void        ggml_rxd_fp32_to_fp16_row(const float *, ggml_rxd_fp16_t *, int64_t);

    // google brain half-precision bfloat16
    typedef struct { uint16_t bits; } ggml_rxd_bf16_t;
    GGML_RXD_API ggml_rxd_bf16_t ggml_rxd_fp32_to_bf16(float);
    GGML_RXD_API float       ggml_rxd_bf16_to_fp32(ggml_rxd_bf16_t);  // consider just doing << 16
    GGML_RXD_API void        ggml_rxd_bf16_to_fp32_row(const ggml_rxd_bf16_t *, float *, int64_t);
    GGML_RXD_API void        ggml_rxd_fp32_to_bf16_row_ref(const float *, ggml_rxd_bf16_t *, int64_t);
    GGML_RXD_API void        ggml_rxd_fp32_to_bf16_row(const float *, ggml_rxd_bf16_t *, int64_t);

    struct ggml_rxd_object;
    struct ggml_rxd_context;
    struct ggml_rxd_cgraph;

    // NOTE: always add types at the end of the enum to keep backward compatibility
    enum ggml_rxd_type {
        GGML_RXD_TYPE_F32     = 0,
        GGML_RXD_TYPE_F16     = 1,
        GGML_RXD_TYPE_Q4_0    = 2,
        GGML_RXD_TYPE_Q4_1    = 3,
        // GGML_RXD_TYPE_Q4_2 = 4, support has been removed
        // GGML_RXD_TYPE_Q4_3 = 5, support has been removed
        GGML_RXD_TYPE_Q5_0    = 6,
        GGML_RXD_TYPE_Q5_1    = 7,
        GGML_RXD_TYPE_Q8_0    = 8,
        GGML_RXD_TYPE_Q8_1    = 9,
        GGML_RXD_TYPE_Q2_K    = 10,
        GGML_RXD_TYPE_Q3_K    = 11,
        GGML_RXD_TYPE_Q4_K    = 12,
        GGML_RXD_TYPE_Q5_K    = 13,
        GGML_RXD_TYPE_Q6_K    = 14,
        GGML_RXD_TYPE_Q8_K    = 15,
        GGML_RXD_TYPE_IQ2_XXS = 16,
        GGML_RXD_TYPE_IQ2_XS  = 17,
        GGML_RXD_TYPE_IQ3_XXS = 18,
        GGML_RXD_TYPE_IQ1_S   = 19,
        GGML_RXD_TYPE_IQ4_NL  = 20,
        GGML_RXD_TYPE_IQ3_S   = 21,
        GGML_RXD_TYPE_IQ2_S   = 22,
        GGML_RXD_TYPE_IQ4_XS  = 23,
        GGML_RXD_TYPE_I8      = 24,
        GGML_RXD_TYPE_I16     = 25,
        GGML_RXD_TYPE_I32     = 26,
        GGML_RXD_TYPE_I64     = 27,
        GGML_RXD_TYPE_F64     = 28,
        GGML_RXD_TYPE_IQ1_M   = 29,
        GGML_RXD_TYPE_BF16    = 30,
        // GGML_RXD_TYPE_Q4_0_4_4 = 31, support has been removed from gguf files
        // GGML_RXD_TYPE_Q4_0_4_8 = 32,
        // GGML_RXD_TYPE_Q4_0_8_8 = 33,
        GGML_RXD_TYPE_TQ1_0   = 34,
        GGML_RXD_TYPE_TQ2_0   = 35,
        // GGML_RXD_TYPE_IQ4_NL_4_4 = 36,
        // GGML_RXD_TYPE_IQ4_NL_4_8 = 37,
        // GGML_RXD_TYPE_IQ4_NL_8_8 = 38,
        GGML_RXD_TYPE_MXFP4   = 39, // MXFP4 (1 block)
        GGML_RXD_TYPE_COUNT   = 40,
    };

    // precision
    enum ggml_rxd_prec {
        GGML_RXD_PREC_DEFAULT =  0, // stored as ggml_rxd_tensor.op_params, 0 by default
        GGML_RXD_PREC_F32     = 10,
    };

    // model file types
    enum ggml_rxd_ftype {
        GGML_RXD_FTYPE_UNKNOWN        = -1,
        GGML_RXD_FTYPE_ALL_F32        = 0,
        GGML_RXD_FTYPE_MOSTLY_F16     = 1,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q4_0    = 2,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q4_1    = 3,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q4_1_SOME_F16 = 4, // tok_embeddings.weight and output.weight are F16
        GGML_RXD_FTYPE_MOSTLY_Q8_0    = 7,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q5_0    = 8,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q5_1    = 9,  // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q2_K    = 10, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q3_K    = 11, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q4_K    = 12, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q5_K    = 13, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_Q6_K    = 14, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ2_XXS = 15, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ2_XS  = 16, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ3_XXS = 17, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ1_S   = 18, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ4_NL  = 19, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ3_S   = 20, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ2_S   = 21, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ4_XS  = 22, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_IQ1_M   = 23, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_BF16    = 24, // except 1d tensors
        GGML_RXD_FTYPE_MOSTLY_MXFP4   = 25, // except 1d tensors
    };

    // available tensor operations:
    enum ggml_rxd_op {
        GGML_RXD_OP_NONE = 0,

        GGML_RXD_OP_DUP,
        GGML_RXD_OP_ADD,
        GGML_RXD_OP_ADD_ID,
        GGML_RXD_OP_ADD1,
        GGML_RXD_OP_ACC,
        GGML_RXD_OP_SUB,
        GGML_RXD_OP_MUL,
        GGML_RXD_OP_DIV,
        GGML_RXD_OP_SQR,
        GGML_RXD_OP_SQRT,
        GGML_RXD_OP_LOG,
        GGML_RXD_OP_SIN,
        GGML_RXD_OP_COS,
        GGML_RXD_OP_SUM,
        GGML_RXD_OP_SUM_ROWS,
        GGML_RXD_OP_CUMSUM,
        GGML_RXD_OP_MEAN,
        GGML_RXD_OP_ARGMAX,
        GGML_RXD_OP_COUNT_EQUAL,
        GGML_RXD_OP_REPEAT,
        GGML_RXD_OP_REPEAT_BACK,
        GGML_RXD_OP_CONCAT,
        GGML_RXD_OP_SILU_BACK,
        GGML_RXD_OP_NORM, // normalize
        GGML_RXD_OP_RMS_NORM,
        GGML_RXD_OP_RMS_NORM_BACK,
        GGML_RXD_OP_GROUP_NORM,
        GGML_RXD_OP_L2_NORM,

        GGML_RXD_OP_MUL_MAT,
        GGML_RXD_OP_MUL_MAT_ID,
        GGML_RXD_OP_OUT_PROD,

        GGML_RXD_OP_SCALE,
        GGML_RXD_OP_SET,
        GGML_RXD_OP_CPY,
        GGML_RXD_OP_CONT,
        GGML_RXD_OP_RESHAPE,
        GGML_RXD_OP_VIEW,
        GGML_RXD_OP_PERMUTE,
        GGML_RXD_OP_TRANSPOSE,
        GGML_RXD_OP_GET_ROWS,
        GGML_RXD_OP_GET_ROWS_BACK,
        GGML_RXD_OP_SET_ROWS,
        GGML_RXD_OP_DIAG,
        GGML_RXD_OP_DIAG_MASK_INF,
        GGML_RXD_OP_DIAG_MASK_ZERO,
        GGML_RXD_OP_SOFT_MAX,
        GGML_RXD_OP_SOFT_MAX_BACK,
        GGML_RXD_OP_ROPE,
        GGML_RXD_OP_ROPE_BACK,
        GGML_RXD_OP_CLAMP,
        GGML_RXD_OP_CONV_TRANSPOSE_1D,
        GGML_RXD_OP_IM2COL,
        GGML_RXD_OP_IM2COL_BACK,
        GGML_RXD_OP_IM2COL_3D,
        GGML_RXD_OP_CONV_2D,
        GGML_RXD_OP_CONV_3D,
        GGML_RXD_OP_CONV_2D_DW,
        GGML_RXD_OP_CONV_TRANSPOSE_2D,
        GGML_RXD_OP_POOL_1D,
        GGML_RXD_OP_POOL_2D,
        GGML_RXD_OP_POOL_2D_BACK,
        GGML_RXD_OP_UPSCALE,
        GGML_RXD_OP_PAD,
        GGML_RXD_OP_PAD_REFLECT_1D,
        GGML_RXD_OP_ROLL,
        GGML_RXD_OP_ARANGE,
        GGML_RXD_OP_TIMESTEP_EMBEDDING,
        GGML_RXD_OP_ARGSORT,
        GGML_RXD_OP_LEAKY_RELU,
        GGML_RXD_OP_TRI,
        GGML_RXD_OP_FILL,

        GGML_RXD_OP_FLASH_ATTN_EXT,
        GGML_RXD_OP_FLASH_ATTN_BACK,
        GGML_RXD_OP_SSM_CONV,
        GGML_RXD_OP_SSM_SCAN,
        GGML_RXD_OP_WIN_PART,
        GGML_RXD_OP_WIN_UNPART,
        GGML_RXD_OP_GET_REL_POS,
        GGML_RXD_OP_ADD_REL_POS,
        GGML_RXD_OP_RWKV_WKV6,
        GGML_RXD_OP_GATED_LINEAR_ATTN,
        GGML_RXD_OP_RWKV_WKV7,
        GGML_RXD_OP_SOLVE_TRI,

        GGML_RXD_OP_UNARY,

        GGML_RXD_OP_MAP_CUSTOM1,
        GGML_RXD_OP_MAP_CUSTOM2,
        GGML_RXD_OP_MAP_CUSTOM3,

        GGML_RXD_OP_CUSTOM,

        GGML_RXD_OP_CROSS_ENTROPY_LOSS,
        GGML_RXD_OP_CROSS_ENTROPY_LOSS_BACK,
        GGML_RXD_OP_OPT_STEP_ADAMW,
        GGML_RXD_OP_OPT_STEP_SGD,

        GGML_RXD_OP_GLU,

        GGML_RXD_OP_COUNT,
    };

    enum ggml_rxd_unary_op {
        GGML_RXD_UNARY_OP_ABS,
        GGML_RXD_UNARY_OP_SGN,
        GGML_RXD_UNARY_OP_NEG,
        GGML_RXD_UNARY_OP_STEP,
        GGML_RXD_UNARY_OP_TANH,
        GGML_RXD_UNARY_OP_ELU,
        GGML_RXD_UNARY_OP_RELU,
        GGML_RXD_UNARY_OP_SIGMOID,
        GGML_RXD_UNARY_OP_GELU,
        GGML_RXD_UNARY_OP_GELU_QUICK,
        GGML_RXD_UNARY_OP_SILU,
        GGML_RXD_UNARY_OP_HARDSWISH,
        GGML_RXD_UNARY_OP_HARDSIGMOID,
        GGML_RXD_UNARY_OP_EXP,
        GGML_RXD_UNARY_OP_EXPM1,
        GGML_RXD_UNARY_OP_SOFTPLUS,
        GGML_RXD_UNARY_OP_GELU_ERF,
        GGML_RXD_UNARY_OP_XIELU,
        GGML_RXD_UNARY_OP_FLOOR,
        GGML_RXD_UNARY_OP_CEIL,
        GGML_RXD_UNARY_OP_ROUND,
        GGML_RXD_UNARY_OP_TRUNC,

        GGML_RXD_UNARY_OP_COUNT,
    };

    enum ggml_rxd_glu_op {
        GGML_RXD_GLU_OP_REGLU,
        GGML_RXD_GLU_OP_GEGLU,
        GGML_RXD_GLU_OP_SWIGLU,
        GGML_RXD_GLU_OP_SWIGLU_OAI,
        GGML_RXD_GLU_OP_GEGLU_ERF,
        GGML_RXD_GLU_OP_GEGLU_QUICK,

        GGML_RXD_GLU_OP_COUNT,
    };

    enum ggml_rxd_object_type {
        GGML_RXD_OBJECT_TYPE_TENSOR,
        GGML_RXD_OBJECT_TYPE_GRAPH,
        GGML_RXD_OBJECT_TYPE_WORK_BUFFER
    };

    enum ggml_rxd_log_level {
        GGML_RXD_LOG_LEVEL_NONE  = 0,
        GGML_RXD_LOG_LEVEL_DEBUG = 1,
        GGML_RXD_LOG_LEVEL_INFO  = 2,
        GGML_RXD_LOG_LEVEL_WARN  = 3,
        GGML_RXD_LOG_LEVEL_ERROR = 4,
        GGML_RXD_LOG_LEVEL_CONT  = 5, // continue previous log
    };

    // this tensor...
    enum ggml_rxd_tensor_flag {
        GGML_RXD_TENSOR_FLAG_INPUT  =  1, // ...is an input for the GGML compute graph
        GGML_RXD_TENSOR_FLAG_OUTPUT =  2, // ...is an output for the GGML compute graph
        GGML_RXD_TENSOR_FLAG_PARAM  =  4, // ...contains trainable parameters
        GGML_RXD_TENSOR_FLAG_LOSS   =  8, // ...defines loss for numerical optimization (multiple loss tensors add up)
    };

    enum ggml_rxd_tri_type {
        GGML_RXD_TRI_TYPE_UPPER_DIAG = 0,
        GGML_RXD_TRI_TYPE_UPPER      = 1,
        GGML_RXD_TRI_TYPE_LOWER_DIAG = 2,
        GGML_RXD_TRI_TYPE_LOWER      = 3
    };

    struct ggml_rxd_init_params {
        // memory pool
        size_t mem_size;   // bytes
        void * mem_buffer; // if NULL, memory will be allocated internally
        bool   no_alloc;   // don't allocate memory for the tensor data
    };

    // n-dimensional tensor
    struct ggml_rxd_tensor {
        enum ggml_rxd_type type;

        struct ggml_rxd_backend_buffer * buffer;

        int64_t ne[GGML_RXD_MAX_DIMS]; // number of elements
        size_t  nb[GGML_RXD_MAX_DIMS]; // stride in bytes:
                                   // nb[0] = ggml_rxd_type_size(type)
                                   // nb[1] = nb[0]   * (ne[0] / ggml_rxd_blck_size(type)) + padding
                                   // nb[i] = nb[i-1] * ne[i-1]

        // compute data
        enum ggml_rxd_op op;

        // op params - allocated as int32_t for alignment
        int32_t op_params[GGML_RXD_MAX_OP_PARAMS / sizeof(int32_t)];

        int32_t flags;

        struct ggml_rxd_tensor * src[GGML_RXD_MAX_SRC];

        // source tensor and offset for views
        struct ggml_rxd_tensor * view_src;
        size_t               view_offs;

        void * data;

        char name[GGML_RXD_MAX_NAME];

        void * extra; // extra things e.g. for ggml-cuda.cu

        char padding[8];
    };

    static const size_t GGML_RXD_TENSOR_SIZE = sizeof(struct ggml_rxd_tensor);

    // Abort callback
    // If not NULL, called before ggml computation
    // If it returns true, the computation is aborted
    typedef bool (*ggml_rxd_abort_callback)(void * data);


    //
    // GUID
    //

    // GUID types
    typedef uint8_t ggml_rxd_guid[16];
    typedef ggml_rxd_guid * ggml_rxd_guid_t;

    GGML_RXD_API bool ggml_rxd_guid_matches(ggml_rxd_guid_t guid_a, ggml_rxd_guid_t guid_b);

    // misc

    GGML_RXD_API const char * ggml_rxd_version(void);
    GGML_RXD_API const char * ggml_rxd_commit(void);

    GGML_RXD_API void    ggml_rxd_time_init(void); // call this once at the beginning of the program
    GGML_RXD_API int64_t ggml_rxd_time_ms(void);
    GGML_RXD_API int64_t ggml_rxd_time_us(void);
    GGML_RXD_API int64_t ggml_rxd_cycles(void);
    GGML_RXD_API int64_t ggml_rxd_cycles_per_ms(void);

    // accepts a UTF-8 path, even on Windows
    GGML_RXD_API FILE *  ggml_rxd_fopen(const char * fname, const char * mode);

    GGML_RXD_API void    ggml_rxd_print_object (const struct ggml_rxd_object * obj);
    GGML_RXD_API void    ggml_rxd_print_objects(const struct ggml_rxd_context * ctx);

    GGML_RXD_API int64_t ggml_rxd_nelements (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API int64_t ggml_rxd_nrows     (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API size_t  ggml_rxd_nbytes    (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API size_t  ggml_rxd_nbytes_pad(const struct ggml_rxd_tensor * tensor); // same as ggml_rxd_nbytes() but padded to GGML_RXD_MEM_ALIGN

    GGML_RXD_API int64_t ggml_rxd_blck_size(enum ggml_rxd_type type);
    GGML_RXD_API size_t  ggml_rxd_type_size(enum ggml_rxd_type type);             // size in bytes for all elements in a block
    GGML_RXD_API size_t  ggml_rxd_row_size (enum ggml_rxd_type type, int64_t ne); // size in bytes for all elements in a row

    GGML_RXD_DEPRECATED(
    GGML_RXD_API double ggml_rxd_type_sizef(enum ggml_rxd_type type), // ggml_rxd_type_size()/ggml_rxd_blck_size() as float
    "use ggml_rxd_row_size() instead");

    GGML_RXD_API const char * ggml_rxd_type_name(enum ggml_rxd_type type);
    GGML_RXD_API const char * ggml_rxd_op_name  (enum ggml_rxd_op   op);
    GGML_RXD_API const char * ggml_rxd_op_symbol(enum ggml_rxd_op   op);

    GGML_RXD_API const char * ggml_rxd_unary_op_name(enum ggml_rxd_unary_op op);
    GGML_RXD_API const char * ggml_rxd_glu_op_name(enum ggml_rxd_glu_op op);
    GGML_RXD_API const char * ggml_rxd_op_desc(const struct ggml_rxd_tensor * t); // unary or op name

    GGML_RXD_API size_t  ggml_rxd_element_size(const struct ggml_rxd_tensor * tensor);

    GGML_RXD_API bool    ggml_rxd_is_quantized(enum ggml_rxd_type type);

    // TODO: temporary until model loading of ggml examples is refactored
    GGML_RXD_API enum ggml_rxd_type ggml_rxd_ftype_to_ggml_type(enum ggml_rxd_ftype ftype);

    GGML_RXD_API bool ggml_rxd_is_transposed(const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_permuted  (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_empty     (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_scalar    (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_vector    (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_matrix    (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_3d        (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API int  ggml_rxd_n_dims       (const struct ggml_rxd_tensor * tensor); // returns 1 for scalars

    // returns whether the tensor elements can be iterated over with a flattened index (no gaps, no permutation)
    GGML_RXD_API bool ggml_rxd_is_contiguous  (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API bool ggml_rxd_is_contiguous_0(const struct ggml_rxd_tensor * tensor); // same as ggml_rxd_is_contiguous()
    GGML_RXD_API bool ggml_rxd_is_contiguous_1(const struct ggml_rxd_tensor * tensor); // contiguous for dims >= 1
    GGML_RXD_API bool ggml_rxd_is_contiguous_2(const struct ggml_rxd_tensor * tensor); // contiguous for dims >= 2

    // returns whether the tensor elements are allocated as one contiguous block of memory (no gaps, but permutation ok)
    GGML_RXD_API bool ggml_rxd_is_contiguously_allocated(const struct ggml_rxd_tensor * tensor);

    // true for tensor that is stored in memory as CxWxHxN and has been permuted to WxHxCxN
    GGML_RXD_API bool ggml_rxd_is_contiguous_channels(const struct ggml_rxd_tensor * tensor);

    // true if the elements in dimension 0 are contiguous, or there is just 1 block of elements
    GGML_RXD_API bool ggml_rxd_is_contiguous_rows(const struct ggml_rxd_tensor * tensor);

    GGML_RXD_API bool ggml_rxd_are_same_shape (const struct ggml_rxd_tensor * t0, const struct ggml_rxd_tensor * t1);
    GGML_RXD_API bool ggml_rxd_are_same_stride(const struct ggml_rxd_tensor * t0, const struct ggml_rxd_tensor * t1);

    GGML_RXD_API bool ggml_rxd_can_repeat(const struct ggml_rxd_tensor * t0, const struct ggml_rxd_tensor * t1);

    // use this to compute the memory overhead of a tensor
    GGML_RXD_API size_t ggml_rxd_tensor_overhead(void);

    GGML_RXD_API bool ggml_rxd_validate_row_data(enum ggml_rxd_type type, const void * data, size_t nbytes);

    // main

    GGML_RXD_API struct ggml_rxd_context * ggml_rxd_init (struct ggml_rxd_init_params params);
    GGML_RXD_API void                  ggml_rxd_reset(struct ggml_rxd_context * ctx);
    GGML_RXD_API void                  ggml_rxd_free (struct ggml_rxd_context * ctx);

    GGML_RXD_API size_t  ggml_rxd_used_mem(const struct ggml_rxd_context * ctx);

    GGML_RXD_API bool    ggml_rxd_get_no_alloc(struct ggml_rxd_context * ctx);
    GGML_RXD_API void    ggml_rxd_set_no_alloc(struct ggml_rxd_context * ctx, bool no_alloc);

    GGML_RXD_API void *  ggml_rxd_get_mem_buffer     (const struct ggml_rxd_context * ctx);
    GGML_RXD_API size_t  ggml_rxd_get_mem_size       (const struct ggml_rxd_context * ctx);
    GGML_RXD_API size_t  ggml_rxd_get_max_tensor_size(const struct ggml_rxd_context * ctx);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_new_tensor(
            struct ggml_rxd_context * ctx,
            enum   ggml_rxd_type type,
            int    n_dims,
            const int64_t *ne);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_new_tensor_1d(
            struct ggml_rxd_context * ctx,
            enum   ggml_rxd_type type,
            int64_t ne0);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_new_tensor_2d(
            struct ggml_rxd_context * ctx,
            enum   ggml_rxd_type type,
            int64_t ne0,
            int64_t ne1);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_new_tensor_3d(
            struct ggml_rxd_context * ctx,
            enum   ggml_rxd_type type,
            int64_t ne0,
            int64_t ne1,
            int64_t ne2);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_new_tensor_4d(
            struct ggml_rxd_context * ctx,
            enum   ggml_rxd_type type,
            int64_t ne0,
            int64_t ne1,
            int64_t ne2,
            int64_t ne3);

    GGML_RXD_API void * ggml_rxd_new_buffer(struct ggml_rxd_context * ctx, size_t nbytes);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_dup_tensor (struct ggml_rxd_context * ctx, const struct ggml_rxd_tensor * src);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_view_tensor(struct ggml_rxd_context * ctx, struct ggml_rxd_tensor * src);

    // Context tensor enumeration and lookup
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_first_tensor(const struct ggml_rxd_context * ctx);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_next_tensor (const struct ggml_rxd_context * ctx, struct ggml_rxd_tensor * tensor);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_tensor(struct ggml_rxd_context * ctx, const char * name);

    // Converts a flat index into coordinates
    GGML_RXD_API void ggml_rxd_unravel_index(const struct ggml_rxd_tensor * tensor, int64_t i, int64_t * i0, int64_t * i1, int64_t * i2, int64_t * i3);

    GGML_RXD_API enum ggml_rxd_unary_op ggml_rxd_get_unary_op(const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API enum ggml_rxd_glu_op ggml_rxd_get_glu_op(const struct ggml_rxd_tensor * tensor);

    GGML_RXD_API void *  ggml_rxd_get_data    (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API float * ggml_rxd_get_data_f32(const struct ggml_rxd_tensor * tensor);

    GGML_RXD_API const char *         ggml_rxd_get_name   (const struct ggml_rxd_tensor * tensor);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_name   (      struct ggml_rxd_tensor * tensor, const char * name);
    GGML_RXD_ATTRIBUTE_FORMAT(2, 3)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_format_name(      struct ggml_rxd_tensor * tensor, const char * fmt, ...);

    // Tensor flags
    GGML_RXD_API void ggml_rxd_set_input(struct ggml_rxd_tensor * tensor);
    GGML_RXD_API void ggml_rxd_set_output(struct ggml_rxd_tensor * tensor);
    GGML_RXD_API void ggml_rxd_set_param(struct ggml_rxd_tensor * tensor);
    GGML_RXD_API void ggml_rxd_set_loss(struct ggml_rxd_tensor * tensor);

    //
    // operations on tensors with backpropagation
    //

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_dup(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_dup_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add_cast(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            enum   ggml_rxd_type      type);

    // dst[i0, i1, i2] = a[i0, i1, i2] + b[i0, ids[i1, i2]]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add_id(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * ids);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add1(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add1_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // dst = a
    // view(dst, nb1, nb2, nb3, offset) += b
    // return dst
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_acc(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                nb2,
            size_t                nb3,
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_acc_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                nb2,
            size_t                nb3,
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sub(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sub_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_mul(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_mul_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_div(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_div_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sqr(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sqr_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sqrt(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sqrt_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_log(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_log_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_expm1(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_expm1_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_softplus(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_softplus_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sin(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sin_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cos(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cos_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // return scalar
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sum(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // sums along rows, with input shape [a,b,c,d] return shape [1,b,c,d]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sum_rows(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cumsum(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a);

    // mean along rows
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_mean(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // argmax along rows
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_argmax(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // count number of equal elements in a and b
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_count_equal(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // if a is the same shape as b, and a is not parameter, return a
    // otherwise, return a new tensor: repeat(a) to fit in b
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_repeat(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // repeat a to the specified shape
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_repeat_4d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
                       int64_t    ne0,
                       int64_t    ne1,
                       int64_t    ne2,
                       int64_t    ne3);

    // sums repetitions in a into shape of b
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_repeat_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b); // sum up values that are adjacent in dims > 0 instead of repeated with same stride

    // concat a and b along dim
    // used in stable-diffusion
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_concat(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   dim);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_abs(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_abs_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sgn(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sgn_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_neg(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_neg_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_step(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_step_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_tanh(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_tanh_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_elu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_elu_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_relu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_leaky_relu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a, float negative_slope, bool inplace);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_relu_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sigmoid(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_sigmoid_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // GELU using erf (error function) when possible
    // some backends may fallback to approximation based on Abramowitz and Stegun formula
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu_erf(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu_erf_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu_quick(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gelu_quick_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_silu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_silu_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // a - x
    // b - dy
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_silu_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // hardswish(x) = x * relu6(x + 3) / 6
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_hardswish(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // hardsigmoid(x) = relu6(x + 3) / 6
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_hardsigmoid(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_exp(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_exp_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_floor(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_floor_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_ceil(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_ceil_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_round(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_round_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

     /**
     * Truncates the fractional part of each element in the tensor (towards zero).
     * For example: trunc(3.7) = 3.0, trunc(-2.9) = -2.0
     * Similar to std::trunc in C/C++.
     */

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_trunc(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_trunc_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);



    // xIELU activation function
    // x = x * (c_a(alpha_n) + c_b(alpha_p, beta) * sigmoid(beta * x)) + eps * (x > 0)
    // where c_a = softplus and c_b(a, b) = softplus(a) + b are constraining functions
    // that constrain the positive and negative source alpha values respectively
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_xielu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float alpha_n,
            float alpha_p,
            float beta,
            float eps);

    // gated linear unit ops
    // A: n columns, r rows,
    // result is n / 2 columns, r rows,
    // expects gate in second half of row, unless swapped is true
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_glu(
            struct ggml_rxd_context * ctx,
             struct ggml_rxd_tensor * a,
             enum ggml_rxd_glu_op     op,
             bool                 swapped);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reglu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reglu_swapped(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_swapped(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_swiglu(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_swiglu_swapped(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_erf(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_erf_swapped(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_quick(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_quick_swapped(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // A: n columns, r rows,
    // B: n columns, r rows,
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_glu_split(
            struct ggml_rxd_context * ctx,
             struct ggml_rxd_tensor * a,
             struct ggml_rxd_tensor * b,
             enum ggml_rxd_glu_op     op);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reglu_split(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_split(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_swiglu_split(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_erf_split(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_geglu_quick_split(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_swiglu_oai(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            float                 alpha,
            float                 limit);

    // normalize along rows
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_norm(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_norm_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rms_norm(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rms_norm_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    // group normalize along ne0*ne1*n_groups
    // used in stable-diffusion
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_group_norm(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_groups,
            float                 eps);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_group_norm_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_groups,
            float                 eps);

    // l2 normalize along rows
    // used in rwkv v7
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_l2_norm(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_l2_norm_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 eps);

    // a - x
    // b - dy
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rms_norm_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            float                 eps);

    // A: k columns, n rows => [ne03, ne02, n, k]
    // B: k columns, m rows  (i.e. we transpose it internally) => [ne03 * x, ne02 * y, m, k]
    // result is n columns, m rows => [ne03 * x, ne02 * y, m, n]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_mul_mat(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // change the precision of a matrix multiplication
    // set to GGML_RXD_PREC_F32 for higher precision (useful for phi-2)
    GGML_RXD_API void ggml_rxd_mul_mat_set_prec(
            struct ggml_rxd_tensor * a,
            enum ggml_rxd_prec       prec);

    // indirect matrix multiplication
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_mul_mat_id(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * as,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * ids);

    // A: m columns, n rows,
    // B: p columns, n rows,
    // result is m columns, p rows
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_out_prod(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    //
    // operations on tensors without backpropagation
    //

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_scale(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 s);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_scale_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 s);

    // x = s * a + b
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_scale_bias(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a,
        float                 s,
        float                 b);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_scale_bias_inplace(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a,
        float                 s,
        float                 b);

    // b -> view(a,offset,nb1,nb2,3), return modified a
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                nb2,
            size_t                nb3,
            size_t                offset); // in bytes

    // b -> view(a,offset,nb1,nb2,3), return view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                nb2,
            size_t                nb3,
            size_t                offset); // in bytes

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                offset); // in bytes

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_1d_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                offset); // in bytes

    // b -> view(a,offset,nb1,nb2,3), return modified a
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                offset); // in bytes

    // b -> view(a,offset,nb1,nb2,3), return view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_2d_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            size_t                nb1,
            size_t                offset); // in bytes

    // a -> b, return view(b)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cpy(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // note: casting from f32 to i32 will discard the fractional part
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cast(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            enum   ggml_rxd_type      type);

    // make contiguous
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cont(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // make contiguous, with new shape
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cont_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cont_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cont_3d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cont_4d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            int64_t               ne3);

    // return view(a), b specifies the new shape
    // TODO: when we start computing gradient, make a copy instead of view
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reshape(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // return view(a)
    // TODO: when we start computing gradient, make a copy instead of view
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reshape_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reshape_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1);

    // return view(a)
    // TODO: when we start computing gradient, make a copy instead of view
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reshape_3d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_reshape_4d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            int64_t               ne3);

    // offset in bytes
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_view_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_view_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            size_t                nb1, // row stride in bytes
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_view_3d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            size_t                nb1, // row   stride in bytes
            size_t                nb2, // slice stride in bytes
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_view_4d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            int64_t               ne3,
            size_t                nb1, // row   stride in bytes
            size_t                nb2, // slice stride in bytes
            size_t                nb3,
            size_t                offset);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_permute(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   axis0,
            int                   axis1,
            int                   axis2,
            int                   axis3);

    // alias for ggml_rxd_permute(ctx, a, 1, 0, 2, 3)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_transpose(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // supports 4D a:
    // a     [n_embd, ne1, ne2, ne3]
    // b I32 [n_rows, ne2, ne3, 1]
    //
    // return [n_embd, n_rows, ne2, ne3]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_rows(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // data
            struct ggml_rxd_tensor  * b); // row indices

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_rows_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // gradients of ggml_rxd_get_rows result
            struct ggml_rxd_tensor  * b,  // row indices
            struct ggml_rxd_tensor  * c); // data for ggml_rxd_get_rows, only used for its shape

    // a TD  [n_embd, ne1,    ne2,    ne3]
    // b TS  [n_embd, n_rows, ne02,   ne03] | ne02 == ne2, ne03 == ne3
    // c I64 [n_rows, ne11,   ne12,   1]    | c[i] in [0, ne1)
    //
    // undefined behavior if destination rows overlap
    //
    // broadcast:
    //   ne2 % ne11 == 0
    //   ne3 % ne12 == 0
    //
    // return view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_rows(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // destination
            struct ggml_rxd_tensor  * b,  // source
            struct ggml_rxd_tensor  * c); // row indices

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_diag(
        struct ggml_rxd_context     * ctx,
        struct ggml_rxd_tensor      * a);

    // set elements above the diagonal to -INF
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_diag_mask_inf(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_past);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_diag_mask_inf_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_past);

    // set elements above the diagonal to 0
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_diag_mask_zero(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_past);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_diag_mask_zero_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   n_past);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a);

    // a    [ne0, ne01, ne02, ne03]
    // mask [ne0, ne11, ne12, ne13] | ne11 >= ne01, F16 or F32, optional
    //
    // broadcast:
    //   ne02 % ne12 == 0
    //   ne03 % ne13 == 0
    //
    // fused soft_max(a*scale + mask*(ALiBi slope))
    // max_bias = 0.0f for no ALiBi
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max_ext(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * mask,
            float                 scale,
            float                 max_bias);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max_ext_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * mask,
            float                 scale,
            float                 max_bias);

    GGML_RXD_API void ggml_rxd_soft_max_add_sinks(
            struct ggml_rxd_tensor * a,
            struct ggml_rxd_tensor * sinks);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max_ext_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            float                 scale,
            float                 max_bias);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_soft_max_ext_back_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            float                 scale,
            float                 max_bias);

    // rotary position embedding
    // if (mode & 1) - skip n_past elements (NOT SUPPORTED)
    // if (mode & GGML_RXD_ROPE_TYPE_NEOX) - GPT-NeoX style
    //
    // b is an int32 vector with size a->ne[2], it contains the positions
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   n_dims,
            int                   mode);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   n_dims,
            int                   mode);

    // custom RoPE
    // c is freq factors (e.g. phi3-128k), (optional)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_ext(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * c,
            int                   n_dims,
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_multi(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * c,
            int                   n_dims,
            int                   sections[GGML_RXD_MROPE_SECTIONS],
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);

    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_ext_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * c,
            int                   n_dims,
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_multi_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * c,
            int                   n_dims,
            int                   sections[GGML_RXD_MROPE_SECTIONS],
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);

    GGML_RXD_DEPRECATED(GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_custom(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   n_dims,
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow),
        "use ggml_rxd_rope_ext instead");

    GGML_RXD_DEPRECATED(GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_custom_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   n_dims,
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow),
        "use ggml_rxd_rope_ext_inplace instead");

    // compute correction dims for YaRN RoPE scaling
    GGML_RXD_API void ggml_rxd_rope_yarn_corr_dims(
        int n_dims, int n_ctx_orig, float freq_base, float beta_fast, float beta_slow, float dims[2]);

    // rotary position embedding backward, i.e compute dx from dy
    // a - dy
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_ext_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a, // gradients of ggml_rxd_rope result
            struct ggml_rxd_tensor  * b, // positions
            struct ggml_rxd_tensor  * c, // freq factors
            int                   n_dims,
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rope_multi_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * c,
            int                   n_dims,
            int                   sections[4],
            int                   mode,
            int                   n_ctx_orig,
            float                 freq_base,
            float                 freq_scale,
            float                 ext_factor,
            float                 attn_factor,
            float                 beta_fast,
            float                 beta_slow);


    // clamp
    // in-place, returns view(a)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_clamp(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 min,
            float                 max);

    // im2col
    // converts data into a format that effectively results in a convolution when combined with matrix multiplication
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_im2col(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // convolution kernel
            struct ggml_rxd_tensor  * b,  // data
            int                   s0, // stride dimension 0
            int                   s1, // stride dimension 1
            int                   p0, // padding dimension 0
            int                   p1, // padding dimension 1
            int                   d0, // dilation dimension 0
            int                   d1, // dilation dimension 1
            bool                  is_2D,
            enum ggml_rxd_type        dst_type);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_im2col_back(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a,  // convolution kernel
        struct ggml_rxd_tensor  * b,  // gradient of im2col output
        int64_t             * ne, // shape of im2col input
        int                   s0, // stride dimension 0
        int                   s1, // stride dimension 1
        int                   p0, // padding dimension 0
        int                   p1, // padding dimension 1
        int                   d0, // dilation dimension 0
        int                   d1, // dilation dimension 1
        bool                  is_2D);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel
            struct ggml_rxd_tensor  * b,   // data
            int                   s0,  // stride
            int                   p0,  // padding
            int                   d0); // dilation

    // conv_1d with padding = half
    // alias for ggml_rxd_conv_1d(a, b, s, a->ne[0]/2, d)
    GGML_RXD_API struct ggml_rxd_tensor* ggml_rxd_conv_1d_ph(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // convolution kernel
            struct ggml_rxd_tensor  * b,  // data
            int                   s,  // stride
            int                   d); // dilation

    // depthwise
    // TODO: this is very likely wrong for some cases! - needs more testing
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_1d_dw(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel
            struct ggml_rxd_tensor  * b,   // data
            int                   s0,  // stride
            int                   p0,  // padding
            int                   d0); // dilation

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_1d_dw_ph(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel
            struct ggml_rxd_tensor  * b,   // data
            int                   s0,  // stride
            int                   d0); // dilation

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_transpose_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel
            struct ggml_rxd_tensor  * b,   // data
            int                   s0,  // stride
            int                   p0,  // padding
            int                   d0); // dilation

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel
            struct ggml_rxd_tensor  * b,   // data
            int                   s0,  // stride dimension 0
            int                   s1,  // stride dimension 1
            int                   p0,  // padding dimension 0
            int                   p1,  // padding dimension 1
            int                   d0,  // dilation dimension 0
            int                   d1); // dilation dimension 1

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_im2col_3d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int64_t               IC,
            int                   s0, // stride width
            int                   s1, // stride height
            int                   s2, // stride depth
            int                   p0, // padding width
            int                   p1, // padding height
            int                   p2, // padding depth
            int                   d0, // dilation width
            int                   d1, // dilation height
            int                   d2, // dilation depth
            enum ggml_rxd_type        dst_type);

    // a: [OC*IC, KD, KH, KW]
    // b: [N*IC, ID, IH, IW]
    // result: [N*OC, OD, OH, OW]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_3d(
                struct ggml_rxd_context * ctx,
                struct ggml_rxd_tensor  * a,
                struct ggml_rxd_tensor  * b,
                int64_t               IC,
                int                   s0, // stride width
                int                   s1, // stride height
                int                   s2, // stride depth
                int                   p0, // padding width
                int                   p1, // padding height
                int                   p2, // padding depth
                int                   d0, // dilation width
                int                   d1, // dilation height
                int                   d2  // dilation depth
        );

    // kernel size is a->ne[0] x a->ne[1]
    // stride is equal to kernel size
    // padding is zero
    // example:
    // a:     16   16    3  768
    // b:   1024 1024    3    1
    // res:   64   64  768    1
    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d_sk_p0(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // kernel size is a->ne[0] x a->ne[1]
    // stride is 1
    // padding is half
    // example:
    // a:      3    3    256  256
    // b:     64   64    256    1
    // res:   64   64    256    1
    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d_s1_ph(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b);

    // depthwise (via im2col and mul_mat)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d_dw(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // convolution kernel
            struct ggml_rxd_tensor  * b,  // data
            int                  s0,  // stride dimension 0
            int                  s1,  // stride dimension 1
            int                  p0,  // padding dimension 0
            int                  p1,  // padding dimension 1
            int                  d0,  // dilation dimension 0
            int                  d1); // dilation dimension 1

    // Depthwise 2D convolution
    // may be faster than ggml_rxd_conv_2d_dw, but not available in all backends
    // a:   KW    KH    1    C    convolution kernel
    // b:   W     H     C    N    input data
    // res: W_out H_out C    N
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d_dw_direct(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   stride0,
            int                   stride1,
            int                   pad0,
            int                   pad1,
            int                   dilation0,
            int                   dilation1);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_transpose_2d_p0(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            int                   stride);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_2d_direct(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // convolution kernel [KW, KH, IC, OC]
            struct ggml_rxd_tensor  * b,   // input data [W, H, C, N]
            int                   s0,  // stride dimension 0
            int                   s1,  // stride dimension 1
            int                   p0,  // padding dimension 0
            int                   p1,  // padding dimension 1
            int                   d0,  // dilation dimension 0
            int                   d1); // dilation dimension 1

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_conv_3d_direct(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,   // kernel [KW, KH, KD, IC * OC]
            struct ggml_rxd_tensor  * b,   // input  [W, H, D, C * N]
            int                   s0,  // stride
            int                   s1,
            int                   s2,
            int                   p0,  // padding
            int                   p1,
            int                   p2,
            int                   d0,  // dilation
            int                   d1,
            int                   d2,
            int                   n_channels,
            int                   n_batch,
            int                   n_channels_out);

    enum ggml_rxd_op_pool {
        GGML_RXD_OP_POOL_MAX,
        GGML_RXD_OP_POOL_AVG,
        GGML_RXD_OP_POOL_COUNT,
    };

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pool_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            enum ggml_rxd_op_pool     op,
            int                   k0, // kernel size
            int                   s0, // stride
            int                   p0); // padding

    // the result will have 2*p0 padding for the first dimension
    // and 2*p1 padding for the second dimension
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pool_2d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            enum ggml_rxd_op_pool     op,
            int                   k0,
            int                   k1,
            int                   s0,
            int                   s1,
            float                 p0,
            float                 p1);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pool_2d_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * af, // "a"/input used in forward pass
            enum ggml_rxd_op_pool     op,
            int                   k0,
            int                   k1,
            int                   s0,
            int                   s1,
            float                 p0,
            float                 p1);

    enum ggml_rxd_scale_mode {
        GGML_RXD_SCALE_MODE_NEAREST  = 0,
        GGML_RXD_SCALE_MODE_BILINEAR = 1,
        GGML_RXD_SCALE_MODE_BICUBIC  = 2,

        GGML_RXD_SCALE_MODE_COUNT
    };

    enum ggml_rxd_scale_flag {
        GGML_RXD_SCALE_FLAG_ALIGN_CORNERS = (1 << 8)
    };

    // interpolate
    // multiplies ne0 and ne1 by scale factor
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_upscale(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   scale_factor,
            enum ggml_rxd_scale_mode  mode);

    // interpolate
    // interpolate scale to specified dimensions
    GGML_RXD_DEPRECATED(GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_upscale_ext(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   ne0,
            int                   ne1,
            int                   ne2,
            int                   ne3,
            enum ggml_rxd_scale_mode  mode),
        "use ggml_rxd_interpolate instead");

    // Up- or downsamples the input to the specified size.
    // 2D scale modes (eg. bilinear) are applied to the first two dimensions.
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_interpolate(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            int64_t               ne3,
            uint32_t              mode); // ggml_rxd_scale_mode [ | ggml_rxd_scale_flag...]

    // pad each dimension with zeros: [x, ..., x] -> [x, ..., x, 0, ..., 0]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pad(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                  p0,
            int                  p1,
            int                  p2,
            int                  p3);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pad_ext(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                  lp0,
            int                  rp0,
            int                  lp1,
            int                  rp1,
            int                  lp2,
            int                  rp2,
            int                  lp3,
            int                  rp3
            );

    // pad each dimension with reflection: [a, b, c, d] -> [b, a, b, c, d, c]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_pad_reflect_1d(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   p0,
            int                   p1);

    // Move tensor elements by an offset given for each dimension. Elements that
    // are shifted beyond the last position are wrapped around to the beginning.
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_roll(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   shift0,
            int                   shift1,
            int                   shift2,
            int                   shift3);

    // Convert matrix into a triangular one (upper, strict upper, lower or strict lower) by writing
    // zeroes everywhere outside the masked area
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_tri(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            enum ggml_rxd_tri_type    type);

    // Fill tensor a with constant c
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_fill(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 c);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_fill_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            float                 c);

    // Ref: https://github.com/CompVis/stable-diffusion/blob/main/ldm/modules/diffusionmodules/util.py#L151
    // timesteps: [N,]
    // return: [N, dim]
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_timestep_embedding(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * timesteps,
            int                   dim,
            int                   max_period);

    // sort rows
    enum ggml_rxd_sort_order {
        GGML_RXD_SORT_ORDER_ASC,
        GGML_RXD_SORT_ORDER_DESC,
    };

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_argsort(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            enum ggml_rxd_sort_order  order);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_arange(
            struct ggml_rxd_context * ctx,
            float                 start,
            float                 stop,
            float                 step);

    // top k elements per row
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_top_k(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   k);

#define GGML_RXD_KQ_MASK_PAD 64

    // q:    [n_embd_k, n_batch,     n_head,    ne3 ]
    // k:    [n_embd_k, n_kv,        n_head_kv, ne3 ]
    // v:    [n_embd_v, n_kv,        n_head_kv, ne3 ] !! not transposed !!
    // mask: [n_kv,     n_batch_pad, ne32,      ne33] !! n_batch_pad = GGML_RXD_PAD(n_batch, GGML_RXD_KQ_MASK_PAD) !!
    // res:  [n_embd_v, n_head,      n_batch,   ne3 ] !! permuted !!
    //
    // broadcast:
    //   n_head % n_head_kv == 0
    //   n_head % ne32      == 0
    //   ne3    % ne33      == 0
    //
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_flash_attn_ext(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * q,
            struct ggml_rxd_tensor  * k,
            struct ggml_rxd_tensor  * v,
            struct ggml_rxd_tensor  * mask,
            float                 scale,
            float                 max_bias,
            float                 logit_softcap);

    GGML_RXD_API void ggml_rxd_flash_attn_ext_set_prec(
            struct ggml_rxd_tensor * a,
            enum ggml_rxd_prec       prec);

    GGML_RXD_API enum ggml_rxd_prec ggml_rxd_flash_attn_ext_get_prec(
            const struct ggml_rxd_tensor * a);

    GGML_RXD_API void ggml_rxd_flash_attn_ext_add_sinks(
            struct ggml_rxd_tensor * a,
            struct ggml_rxd_tensor * sinks);

    // TODO: needs to be adapted to ggml_rxd_flash_attn_ext
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_flash_attn_back(
           struct ggml_rxd_context * ctx,
           struct ggml_rxd_tensor  * q,
           struct ggml_rxd_tensor  * k,
           struct ggml_rxd_tensor  * v,
           struct ggml_rxd_tensor  * d,
           bool                  masked);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_ssm_conv(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * sx,
            struct ggml_rxd_tensor  * c);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_ssm_scan(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * s,
            struct ggml_rxd_tensor  * x,
            struct ggml_rxd_tensor  * dt,
            struct ggml_rxd_tensor  * A,
            struct ggml_rxd_tensor  * B,
            struct ggml_rxd_tensor  * C,
            struct ggml_rxd_tensor  * ids);

    // partition into non-overlapping windows with padding if needed
    // example:
    // a:   768   64   64    1
    // w:    14
    // res: 768   14   14    25
    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_win_part(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   w);

    // reverse of ggml_rxd_win_part
    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_win_unpart(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   w0,
            int                   h0,
            int                   w);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_unary(
            struct ggml_rxd_context * ctx,
             struct ggml_rxd_tensor * a,
             enum ggml_rxd_unary_op op);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_unary_inplace(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a,
        enum ggml_rxd_unary_op op);

    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_get_rel_pos(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            int                   qh,
            int                   kh);

    // used in sam
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add_rel_pos(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * pw,
            struct ggml_rxd_tensor  * ph);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_add_rel_pos_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * pw,
            struct ggml_rxd_tensor  * ph);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rwkv_wkv6(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * k,
            struct ggml_rxd_tensor  * v,
            struct ggml_rxd_tensor  * r,
            struct ggml_rxd_tensor  * tf,
            struct ggml_rxd_tensor  * td,
            struct ggml_rxd_tensor  * state);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_gated_linear_attn(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * k,
            struct ggml_rxd_tensor  * v,
            struct ggml_rxd_tensor  * q,
            struct ggml_rxd_tensor  * g,
            struct ggml_rxd_tensor  * state,
            float scale);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_rwkv_wkv7(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * r,
            struct ggml_rxd_tensor  * w,
            struct ggml_rxd_tensor  * k,
            struct ggml_rxd_tensor  * v,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * b,
            struct ggml_rxd_tensor  * state);

    /* Solves a specific equation of the form Ax=B, where A is a triangular matrix
    *  without zeroes on the diagonal (i.e. invertible).
    *  B can have any number of columns, but must have the same number of rows as A
    *  If A is [n, n] and B is [n, m], then the result will be [n, m] as well
    *  Has O(n^3) complexity (unlike most matrix ops out there), so use on cases
    *  where n > 100 sparingly, pre-chunk if necessary.
    *
    *  If left = false, solves xA=B instead
    *  If lower = false, assumes upper triangular instead
    *  If uni = true, assumes diagonal of A to be all ones (will override actual values)
    *
    *  TODO: currently only lower, right, non-unitriangular variant is implemented
    */
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_solve_tri(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor  * a,
        struct ggml_rxd_tensor  * b,
        bool                  left,
        bool                  lower,
        bool                  uni);

    // custom operators

    typedef void (*ggml_rxd_custom1_op_t)(struct ggml_rxd_tensor * dst , const struct ggml_rxd_tensor * a, int ith, int nth, void * userdata);
    typedef void (*ggml_rxd_custom2_op_t)(struct ggml_rxd_tensor * dst , const struct ggml_rxd_tensor * a, const struct ggml_rxd_tensor * b, int ith, int nth, void * userdata);
    typedef void (*ggml_rxd_custom3_op_t)(struct ggml_rxd_tensor * dst , const struct ggml_rxd_tensor * a, const struct ggml_rxd_tensor * b, const struct ggml_rxd_tensor * c, int ith, int nth, void * userdata);

#define GGML_RXD_N_TASKS_MAX (-1)
    // n_tasks == GGML_RXD_N_TASKS_MAX means to use max number of tasks

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom1(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            ggml_rxd_custom1_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom1_inplace(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            ggml_rxd_custom1_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom2(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            struct ggml_rxd_tensor    * b,
            ggml_rxd_custom2_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom2_inplace(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            struct ggml_rxd_tensor    * b,
            ggml_rxd_custom2_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom3(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            struct ggml_rxd_tensor    * b,
            struct ggml_rxd_tensor    * c,
            ggml_rxd_custom3_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_map_custom3_inplace(
            struct ggml_rxd_context   * ctx,
            struct ggml_rxd_tensor    * a,
            struct ggml_rxd_tensor    * b,
            struct ggml_rxd_tensor    * c,
            ggml_rxd_custom3_op_t       fun,
            int                     n_tasks,
            void                  * userdata);

    typedef void (*ggml_rxd_custom_op_t)(struct ggml_rxd_tensor * dst , int ith, int nth, void * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_custom_4d(
            struct ggml_rxd_context * ctx,
            enum ggml_rxd_type        type,
            int64_t               ne0,
            int64_t               ne1,
            int64_t               ne2,
            int64_t               ne3,
            struct ggml_rxd_tensor ** args,
            int                   n_args,
            ggml_rxd_custom_op_t      fun,
            int                   n_tasks,
            void                * userdata);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_custom_inplace(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor ** args,
            int                   n_args,
            ggml_rxd_custom_op_t      fun,
            int                   n_tasks,
            void                * userdata);

    // loss function

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cross_entropy_loss(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // logits
            struct ggml_rxd_tensor  * b); // labels

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_cross_entropy_loss_back(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,  // logits
            struct ggml_rxd_tensor  * b,  // labels
            struct ggml_rxd_tensor  * c); // gradients of cross_entropy_loss result

    // AdamW optimizer step
    // Paper: https://arxiv.org/pdf/1711.05101v3.pdf
    // PyTorch: https://pytorch.org/docs/stable/generated/torch.optim.AdamW.html
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_opt_step_adamw(
            struct ggml_rxd_context * ctx,
            struct ggml_rxd_tensor  * a,
            struct ggml_rxd_tensor  * grad,
            struct ggml_rxd_tensor  * m,
            struct ggml_rxd_tensor  * v,
            struct ggml_rxd_tensor  * adamw_params); // parameters such as the learning rate

    // stochastic gradient descent step (with weight decay)
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_opt_step_sgd(
        struct ggml_rxd_context * ctx,
        struct ggml_rxd_tensor *  a,
        struct ggml_rxd_tensor *  grad,
        struct ggml_rxd_tensor *  sgd_params); // alpha, weight decay

    //
    // automatic differentiation
    //

    GGML_RXD_API void ggml_rxd_build_forward_expand(struct ggml_rxd_cgraph * cgraph, struct ggml_rxd_tensor * tensor);
    GGML_RXD_API void ggml_rxd_build_backward_expand(
        struct ggml_rxd_context *  ctx,        // context for gradient computation
        struct ggml_rxd_cgraph  *  cgraph,
        struct ggml_rxd_tensor  ** grad_accs);

    // graph allocation in a context
    GGML_RXD_API struct ggml_rxd_cgraph * ggml_rxd_new_graph       (struct ggml_rxd_context * ctx); // size = GGML_RXD_DEFAULT_GRAPH_SIZE, grads = false
    GGML_RXD_API struct ggml_rxd_cgraph * ggml_rxd_new_graph_custom(struct ggml_rxd_context * ctx, size_t size, bool grads);
    GGML_RXD_API struct ggml_rxd_cgraph * ggml_rxd_graph_dup       (struct ggml_rxd_context * ctx, struct ggml_rxd_cgraph * cgraph, bool force_grads);
    GGML_RXD_API void                 ggml_rxd_graph_cpy       (struct ggml_rxd_cgraph * src, struct ggml_rxd_cgraph * dst);
    GGML_RXD_API void                 ggml_rxd_graph_reset     (struct ggml_rxd_cgraph * cgraph); // set regular grads + optimizer momenta to 0, set loss grad to 1
    GGML_RXD_API void                 ggml_rxd_graph_clear     (struct ggml_rxd_cgraph * cgraph);

    GGML_RXD_API int                   ggml_rxd_graph_size   (struct ggml_rxd_cgraph * cgraph);
    GGML_RXD_API struct ggml_rxd_tensor *  ggml_rxd_graph_node   (struct ggml_rxd_cgraph * cgraph, int i); // if i < 0, returns nodes[n_nodes + i]
    GGML_RXD_API struct ggml_rxd_tensor ** ggml_rxd_graph_nodes  (struct ggml_rxd_cgraph * cgraph);
    GGML_RXD_API int                   ggml_rxd_graph_n_nodes(struct ggml_rxd_cgraph * cgraph);

    GGML_RXD_API void   ggml_rxd_graph_add_node(struct ggml_rxd_cgraph * cgraph, struct ggml_rxd_tensor * tensor);

    GGML_RXD_API size_t ggml_rxd_graph_overhead(void);
    GGML_RXD_API size_t ggml_rxd_graph_overhead_custom(size_t size, bool grads);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_graph_get_tensor  (const struct ggml_rxd_cgraph * cgraph, const char * name);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_graph_get_grad    (const struct ggml_rxd_cgraph * cgraph, const struct ggml_rxd_tensor * node);
    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_graph_get_grad_acc(const struct ggml_rxd_cgraph * cgraph, const struct ggml_rxd_tensor * node);

    // print info and performance information for the graph
    GGML_RXD_API void ggml_rxd_graph_print(const struct ggml_rxd_cgraph * cgraph);

    // dump the graph into a file using the dot format
    GGML_RXD_API void ggml_rxd_graph_dump_dot(const struct ggml_rxd_cgraph * gb, const struct ggml_rxd_cgraph * gf, const char * filename);

    // TODO these functions were sandwiched in the old optimization interface, is there a better place for them?
    typedef void (*ggml_rxd_log_callback)(enum ggml_rxd_log_level level, const char * text, void * user_data);

    // Set callback for all future logging events.
    // If this is not called, or NULL is supplied, everything is output on stderr.
    GGML_RXD_API void ggml_rxd_log_set(ggml_rxd_log_callback log_callback, void * user_data);

    GGML_RXD_API struct ggml_rxd_tensor * ggml_rxd_set_zero(struct ggml_rxd_tensor * tensor);

    //
    // quantization
    //

    // - ggml_rxd_quantize_init can be called multiple times with the same type
    //   it will only initialize the quantization tables for the first call or after ggml_rxd_quantize_free
    //   automatically called by ggml_rxd_quantize_chunk for convenience
    //
    // - ggml_rxd_quantize_free will free any memory allocated by ggml_rxd_quantize_init
    //   call this at the end of the program to avoid memory leaks
    //
    // note: these are thread-safe
    //
    GGML_RXD_API void ggml_rxd_quantize_init(enum ggml_rxd_type type);
    GGML_RXD_API void ggml_rxd_quantize_free(void);

    // some quantization type cannot be used without an importance matrix
    GGML_RXD_API bool ggml_rxd_quantize_requires_imatrix(enum ggml_rxd_type type);

    // calls ggml_rxd_quantize_init internally (i.e. can allocate memory)
    GGML_RXD_API size_t ggml_rxd_quantize_chunk(
            enum ggml_rxd_type   type,
               const float * src,
                      void * dst,
                   int64_t   start,
                   int64_t   nrows,
                   int64_t   n_per_row,
               const float * imatrix);

#ifdef __cplusplus
    // restrict not standard in C++
#    if defined(__GNUC__)
#        define GGML_RXD_RESTRICT __restrict__
#    elif defined(__clang__)
#        define GGML_RXD_RESTRICT __restrict
#    elif defined(_MSC_VER)
#        define GGML_RXD_RESTRICT __restrict
#    else
#        define GGML_RXD_RESTRICT
#    endif
#else
#    if defined (_MSC_VER) && (__STDC_VERSION__ < 201112L)
#        define GGML_RXD_RESTRICT __restrict
#    else
#        define GGML_RXD_RESTRICT restrict
#    endif
#endif
    typedef void (*ggml_rxd_to_float_t)  (const void  * GGML_RXD_RESTRICT x, float * GGML_RXD_RESTRICT y, int64_t k);
    typedef void (*ggml_rxd_from_float_t)(const float * GGML_RXD_RESTRICT x, void  * GGML_RXD_RESTRICT y, int64_t k);

    struct ggml_rxd_type_traits {
        const char             * type_name;
        int64_t                  blck_size;
        int64_t                  blck_size_interleave; // interleave elements in blocks
        size_t                   type_size;
        bool                     is_quantized;
        ggml_rxd_to_float_t          to_float;
        ggml_rxd_from_float_t        from_float_ref;
    };

    GGML_RXD_API const struct ggml_rxd_type_traits * ggml_rxd_get_type_traits(enum ggml_rxd_type type);

    // ggml threadpool
    // TODO: currently, only a few functions are in the base ggml API, while the rest are in the CPU backend
    // the goal should be to create an API that other backends can use move everything to the ggml base

    // scheduling priorities
    enum ggml_rxd_sched_priority {
        GGML_RXD_SCHED_PRIO_LOW = -1,
        GGML_RXD_SCHED_PRIO_NORMAL,
        GGML_RXD_SCHED_PRIO_MEDIUM,
        GGML_RXD_SCHED_PRIO_HIGH,
        GGML_RXD_SCHED_PRIO_REALTIME
    };

    // threadpool params
    // Use ggml_rxd_threadpool_params_default() or ggml_rxd_threadpool_params_init() to populate the defaults
    struct ggml_rxd_threadpool_params {
        bool                cpumask[GGML_RXD_MAX_N_THREADS]; // mask of cpu cores (all-zeros means use default affinity settings)
        int                 n_threads;                   // number of threads
        enum ggml_rxd_sched_priority prio;                   // thread priority
        uint32_t            poll;                        // polling level (0 - no polling, 100 - aggressive polling)
        bool                strict_cpu;                  // strict cpu placement
        bool                paused;                      // start in paused state
    };

    struct ggml_rxd_threadpool;     // forward declaration, see ggml.c

    typedef struct ggml_rxd_threadpool * ggml_rxd_threadpool_t;

    GGML_RXD_API struct ggml_rxd_threadpool_params ggml_rxd_threadpool_params_default(int n_threads);
    GGML_RXD_API void                          ggml_rxd_threadpool_params_init   (struct ggml_rxd_threadpool_params * p, int n_threads);
    GGML_RXD_API bool                          ggml_rxd_threadpool_params_match  (const struct ggml_rxd_threadpool_params * p0, const struct ggml_rxd_threadpool_params * p1);

#ifdef  __cplusplus
}
#endif
