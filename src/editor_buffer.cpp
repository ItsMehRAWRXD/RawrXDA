#include "../include/editor_buffer.h"
#include <algorithm>
#include <stdexcept>

BufferModel::BufferModel() {
    m_data.reserve(1024);
    m_gapStart = 0;
    m_gapEnd = m_data.capacity();
    m_data.resize(m_gapEnd, '\0');
    rebuildLineIndex();
}

BufferModel::BufferModel(std::string_view initial) : BufferModel() {
    set(initial);
}

size_t BufferModel::size() const {
    return m_data.size() - (m_gapEnd - m_gapStart);
}

void BufferModel::ensureGapCapacity(size_t needed) {
    size_t gapSize = m_gapEnd - m_gapStart;
    if (gapSize >= needed) return;
    // Grow: allocate new vector doubling size + needed
    size_t logicalSize = size();
    size_t newCapacity = std::max(m_data.size() * 2, logicalSize + needed + 64);
    std::vector<char> newData(newCapacity, '\0');

    // Copy before gap
    std::copy(m_data.begin(), m_data.begin() + m_gapStart, newData.begin());
    size_t newGapStart = m_gapStart;
    size_t newGapEnd = newGapStart + (newCapacity - logicalSize);
    // Copy after gap
    size_t tailCount = m_data.size() - m_gapEnd;
    std::copy(m_data.begin() + m_gapEnd, m_data.end(), newData.begin() + newGapEnd);

    m_data.swap(newData);
    m_gapStart = newGapStart;
    m_gapEnd = newGapEnd;
}

void BufferModel::moveGap(size_t pos) {
    if (pos > size()) pos = size();
    if (pos == m_gapStart) return;
    size_t logicalSize = size();
    // Move gap by shifting characters
    if (pos < m_gapStart) {
        // Move gap left: shift block [pos, m_gapStart) to end of gap
        size_t delta = m_gapStart - pos;
        for (size_t i = 0; i < delta; ++i) {
            m_data[m_gapEnd - 1 - i] = m_data[m_gapStart - 1 - i];
        }
        m_gapStart = pos;
        m_gapEnd -= delta;
    } else { // pos > m_gapStart
        size_t delta = pos - m_gapStart;
        for (size_t i = 0; i < delta; ++i) {
            m_data[m_gapStart + i] = m_data[m_gapEnd + i];
        }
        m_gapStart += delta;
        m_gapEnd += delta;
    }
}

size_t BufferModel::logicalToPhysical(size_t logical) const {
    if (logical < m_gapStart) return logical;
    return logical + (m_gapEnd - m_gapStart);
}

void BufferModel::insert(size_t pos, std::string_view text) {
    if (pos > size()) pos = size();
    moveGap(pos);
    ensureGapCapacity(text.size());
    for (char c : text) {
        m_data[m_gapStart++] = c;
    }
    updateLineIndexOnInsert(pos, text);
}

void BufferModel::erase(size_t pos, size_t len) {
    if (pos >= size() || len == 0) return;
    if (pos + len > size()) len = size() - pos;
    moveGap(pos + len);
    // Expand gap backwards
    m_gapStart = pos;
    updateLineIndexOnErase(pos, len);
}

std::string BufferModel::getText(size_t pos, size_t len) const {
    if (pos >= size()) return {};
    if (pos + len > size()) len = size() - pos;
    std::string out; out.reserve(len);
    size_t end = pos + len;
    size_t gapSize = m_gapEnd - m_gapStart;
    if (end <= m_gapStart) {
        // Entirely before gap
        out.assign(m_data.begin() + pos, m_data.begin() + end);
        return out;
    }
    if (pos >= m_gapStart) {
        // Entirely after gap
        size_t physStart = pos + gapSize;
        size_t physEnd = end + gapSize;
        out.assign(m_data.begin() + physStart, m_data.begin() + physEnd);
        return out;
    }
    // Spans gap
    out.assign(m_data.begin() + pos, m_data.begin() + m_gapStart);
    size_t physStart = m_gapEnd;
    size_t physEnd = m_gapEnd + (end - m_gapStart);
    out.append(m_data.begin() + physStart, m_data.begin() + physEnd);
    return out;
}

std::string BufferModel::snapshot() const {
    return getText(0, size());
}

void BufferModel::set(std::string_view text) {
    m_data.assign(text.begin(), text.end());
    // Create a gap at end with small reserve
    size_t extra = 256;
    m_data.resize(m_data.size() + extra, '\0');
    m_gapStart = text.size();
    m_gapEnd = m_data.size();
    rebuildLineIndex();
}

void BufferModel::rebuildLineIndex() {
    m_lineOffsets.clear();
    m_lineOffsets.push_back(0);
    std::string all = snapshot();
    for (size_t i = 0; i < all.size(); ++i) {
        if (all[i] == '\n') m_lineOffsets.push_back(i + 1);
    }
}

void BufferModel::updateLineIndexOnInsert(size_t pos, std::string_view text) {
    // Simplified: rebuild for correctness first; optimize later.
    (void)pos; (void)text;
    rebuildLineIndex();
}

void BufferModel::updateLineIndexOnErase(size_t pos, size_t len) {
    (void)pos; (void)len;
    rebuildLineIndex();
}

std::string BufferModel::getLine(size_t line) const {
    if (line >= m_lineOffsets.size()) return {};
    size_t start = m_lineOffsets[line];
    size_t end = (line + 1 < m_lineOffsets.size()) ? m_lineOffsets[line + 1] : size();
    // Trim trailing newline
    if (end > start && getText(end - 1, 1) == "\n") --end;
    return getText(start, end - start);
}
