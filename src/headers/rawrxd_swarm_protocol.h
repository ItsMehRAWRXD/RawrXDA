#ifndef RAWRXD_SWARM_PROTOCOL_H
#define RAWRXD_SWARM_PROTOCOL_H

#include <stdint.h>
#include <assert.h>

#pragma pack(push, 1)

/**
 * @version 1.2.0
 * @protocol RAWRXD_SWARM_INFRASTRUCTURE
 * @desc 800B Distributed Swarm Tensor Sharding Protocol.
 */

#define SW_PROTOCOL_VERSION 0x01020000

typedef enum {
    SW_MSG_HANDSHAKE    = 0x01,
    SW_MSG_SHARD_ANNOUNCE = 0x02,
    SW_MSG_TENSOR_SYNC  = 0x03,
    SW_MSG_HEARTBEAT    = 0x04,
    SW_MSG_GRADIENT_PUSH = 0x05,
    SW_MSG_BARRIER      = 0x06
} SwarmMessageType;

typedef struct {
    uint32_t Magic;         // 'SWRM'
    uint32_t Version;       // 0x01020000
    uint64_t Sequence;
    uint8_t  MessageType;
    uint8_t  Pad[7];        // Manual padding
} SwarmHeader;

typedef struct {
    SwarmHeader Header;
    uint64_t NodeID;
    uint64_t TotalNodes;
    uint64_t Capabilities;
} SwarmHandshake;

typedef struct {
    SwarmHeader Header;
    uint64_t TensorID;
    uint64_t Offset;
    uint64_t Size;
    uint64_t Checksum;
} SwarmTensorShard;

#pragma pack(pop)

// Static asserts for v1.2 protocol stability
static_assert(sizeof(SwarmHeader) == 24, "SwarmHeader size mismatch");
static_assert(sizeof(SwarmHandshake) == 48, "SwarmHandshake size mismatch");
static_assert(sizeof(SwarmTensorShard) == 56, "SwarmTensorShard size mismatch");

#endif // RAWRXD_SWARM_PROTOCOL_H
