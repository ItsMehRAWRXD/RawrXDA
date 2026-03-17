#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

class BufferModel {
public:
    BufferModel();
    explicit BufferModel(std::string_view initial);

    std::size_t size() const;
    void insert(std::size_t pos, std::string_view text);
    void erase(std::size_t pos, std::size_t len);
    std::string getText(std::size_t pos, std::size_t len) const;
    std::string snapshot() const;
    void set(std::string_view text);
    std::string getLine(std::size_t line) const;

private:
    void ensureGapCapacity(std::size_t needed);
    void moveGap(std::size_t pos);
    std::size_t logicalToPhysical(std::size_t logical) const;
    void rebuildLineIndex();
    void updateLineIndexOnInsert(std::size_t pos, std::string_view text);
    void updateLineIndexOnErase(std::size_t pos, std::size_t len);

    std::vector<char> m_data;
    std::size_t m_gapStart{0};
    std::size_t m_gapEnd{0};
    std::vector<std::size_t> m_lineOffsets;
};
