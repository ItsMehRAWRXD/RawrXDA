#ifndef RAWRXD_SWARM_PROTOCOL_H
#define RAWRXD_SWARM_PROTOCOL_H

#include <stdint.h>

#pragma pack(push, 1)

// Swarm Node identification
typedef struct _SWARM_NODE_ID {
    uint8_t  Guid[16];
    uint32_t IPv4;
    uint16_t Port;
} SWARM_NODE_ID;

// Swarm Message Magic
#define SWARM_MAGIC 0x58445753 // 'SWXD'

typedef enum _SWARM_MSG_TYPE {
    REQ_SWARM_DISCOVERY = 1,
    RES_SWARM_PONG      = 2,
    REQ_SWARM_SYNC      = 3,
    MSG_TENSOR_CHUNK    = 4,
    MSG_INFERENCE_EXEC  = 5,
    MSG_RESULT_COLLECT  = 6
} SWARM_MSG_TYPE;

// Header for all swarm packets
typedef struct _SWARM_HEADER {
    uint32_t Magic;
    uint32_t Version;
    uint32_t MessageType;
    uint32_t PayloadSize;
    uint64_t SequenceId;
} SWARM_HEADER;

// Tensor Chunk Meta
typedef struct _TENSOR_CHUNK_INFO {
    uint64_t TensorId;
    uint64_t Offset;
    uint64_t TotalSize;
    uint32_t ElementType; // 0=F32, 1=F16, 2=Q4_0, etc
    uint32_t Reserved;
} TENSOR_CHUNK_INFO;

// Node Discovery Payload
typedef struct _SWARM_DISCOVERY_PAYLOAD {
    uint64_t CapacityBytes;
    uint32_t NumCores;
    uint32_t Flags; // AVX512, CUDA, etc
} SWARM_DISCOVERY_PAYLOAD;

#pragma pack(pop)

// Static asserts to ensure layout consistency with MASM
static_assert(sizeof(SWARM_HEADER) == 24, "SWARM_HEADER size mismatch");
static_assert(sizeof(SWARM_NODE_ID) == 22, "SWARM_NODE_ID size mismatch");
static_assert(sizeof(TENSOR_CHUNK_INFO) == 32, "TENSOR_CHUNK_INFO size mismatch");

#endif // RAWRXD_SWARM_PROTOCOL_H
