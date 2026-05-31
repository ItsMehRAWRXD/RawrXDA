#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <intrin.h>

struct RawrXDStreamPacket {
    uint64_t seq;
    uint32_t len;
    char payload[512];
};

class RawrXDStreamBuffer {
public:
    static const long kCapacity = 256;
    static const long kMask = kCapacity - 1;

    RawrXDStreamBuffer() {
        Reset();
    }

    void Reset() {
        m_head = 0;
        m_tail = 0;
        m_dropped = 0;
        for (long i = 0; i < kCapacity; ++i) {
            m_slots[i].seq = 0;
            m_slots[i].len = 0;
            m_slots[i].payload[0] = 0;
        }
    }

    int Push(const char* text, int len, uint64_t seq) {
        if (!text || len <= 0) {
            return 0;
        }

        long head = m_head;
        long next = (head + 1) & kMask;
        if (next == m_tail) {
            _InterlockedIncrement(&m_dropped);
            return 0;
        }

        RawrXDStreamPacket& slot = m_slots[head];
        int copy_len = len;
        if (copy_len > (int)(sizeof(slot.payload) - 1)) {
            copy_len = (int)(sizeof(slot.payload) - 1);
        }

        for (int i = 0; i < copy_len; ++i) {
            slot.payload[i] = text[i];
        }
        slot.payload[copy_len] = 0;
        slot.len = (uint32_t)copy_len;
        slot.seq = seq;

        _ReadWriteBarrier();
        m_head = next;
        return 1;
    }

    int Pop(char* out, uint32_t out_capacity, uint32_t* out_len, uint64_t* out_seq) {
        if (!out || out_capacity == 0 || !out_len || !out_seq) {
            return 0;
        }

        long tail = m_tail;
        if (tail == m_head) {
            return 0;
        }

        const RawrXDStreamPacket& slot = m_slots[tail];
        uint32_t copy_len = slot.len;
        if (copy_len >= out_capacity) {
            copy_len = out_capacity - 1;
        }

        for (uint32_t i = 0; i < copy_len; ++i) {
            out[i] = slot.payload[i];
        }
        out[copy_len] = 0;
        *out_len = copy_len;
        *out_seq = slot.seq;

        _ReadWriteBarrier();
        m_tail = (tail + 1) & kMask;
        return 1;
    }

    uint32_t Count() const {
        long head = m_head;
        long tail = m_tail;
        if (head >= tail) {
            return (uint32_t)(head - tail);
        }
        return (uint32_t)(kCapacity - tail + head);
    }

    uint32_t Dropped() const {
        return (uint32_t)m_dropped;
    }

private:
    volatile long m_head;
    volatile long m_tail;
    volatile long m_dropped;
    RawrXDStreamPacket m_slots[kCapacity];
};
