#pragma once

#include <cstdint>

namespace RawrXD::Compression
{

// Byte-Oriented Command (BOC) opcodes.
constexpr uint8_t kOpcodeEndOfBlock = 0x00;
constexpr uint8_t kOpcodeLiteralRun = 0x01;
constexpr uint8_t kOpcodeZeroRun = 0x02;
constexpr uint8_t kOpcodeDeltaChunk = 0x03;

using CodecFn = uint32_t (*)(const uint8_t* predicted, const uint8_t* input, uint32_t inputBytes, uint8_t* output,
                             uint32_t outputCapacity);

extern "C" uint32_t ExpandBitstream_Ref(const uint8_t* predicted, const uint8_t* inStream, uint32_t inBytes,
                                         uint8_t* outActual, uint32_t outCapacity);
extern "C" uint32_t SqueezeBitstream_Ref(const uint8_t* predicted, const uint8_t* inActual, uint32_t inBytes,
                                          uint8_t* outStream, uint32_t outCapacity);

extern "C" uint32_t ExpandBitstream(const uint8_t* predicted, const uint8_t* inStream, uint32_t inBytes,
                                     uint8_t* outActual, uint32_t outCapacity);
extern "C" uint32_t SqueezeBitstream(const uint8_t* predicted, const uint8_t* inActual, uint32_t inBytes,
                                      uint8_t* outStream, uint32_t outCapacity);

CodecFn SelectExpandDecoder();
CodecFn SelectSqueezeEncoder();

}  // namespace RawrXD::Compression
