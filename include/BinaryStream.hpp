// BinaryStream.hpp — Win32 + STL only. Little-endian binary stream and file reader.
#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <istream>
#include <string>
#include <vector>

namespace RawrXD
{

enum class StreamStatus
{
    Ok = 0,
    ReadPastEnd,
    ReadCorruptData
};

class BinaryStream
{
  public:
    explicit BinaryStream(std::istream& in) : m_in(in), m_status(StreamStatus::Ok) {}

    void setByteOrderLittleEndian() {}
    StreamStatus status() const { return m_status; }
    void setStatus(StreamStatus s) { m_status = s; }

    BinaryStream& operator>>(uint8_t& v)
    {
        read(&v, 1);
        return *this;
    }
    BinaryStream& operator>>(int8_t& v)
    {
        read(&v, 1);
        return *this;
    }
    BinaryStream& operator>>(uint16_t& v)
    {
        read(&v, 2);
        return *this;
    }
    BinaryStream& operator>>(int16_t& v)
    {
        read(&v, 2);
        return *this;
    }
    BinaryStream& operator>>(uint32_t& v)
    {
        read(&v, 4);
        return *this;
    }
    BinaryStream& operator>>(int32_t& v)
    {
        read(&v, 4);
        return *this;
    }
    BinaryStream& operator>>(uint64_t& v)
    {
        read(&v, 8);
        return *this;
    }
    BinaryStream& operator>>(int64_t& v)
    {
        read(&v, 8);
        return *this;
    }
    BinaryStream& operator>>(float& v)
    {
        read(&v, 4);
        return *this;
    }
    BinaryStream& operator>>(bool& v)
    {
        uint8_t u;
        *this >> u;
        v = (u != 0);
        return *this;
    }

    int readRawData(char* buf, int len)
    {
        if (!m_in.read(buf, len))
        {
            m_status = StreamStatus::ReadPastEnd;
            return static_cast<int>(m_in.gcount());
        }
        return len;
    }
    int skipRawData(int len)
    {
        if (!m_in.seekg(len, std::ios::cur))
        {
            m_status = StreamStatus::ReadPastEnd;
            return 0;
        }
        return len;
    }

  private:
    std::istream& m_in;
    StreamStatus m_status;

    void read(void* p, size_t n)
    {
        if (!m_in.read(reinterpret_cast<char*>(p), n))
            m_status = StreamStatus::ReadPastEnd;
    }
};

class NativeFile
{
  public:
    NativeFile() = default;
    explicit NativeFile(const std::string& path) : m_path(path) {}

    bool open(const std::string& path)
    {
        m_path = path;
        m_file.open(path, std::ios::binary | std::ios::in);
        return m_file.is_open();
    }
    bool open() { return open(m_path); }
    bool isOpen() const { return m_file.is_open(); }
    bool exists() const
    {
        if (m_path.empty())
            return false;
        return std::filesystem::exists(m_path);
    }
    int64_t size()
    {
        if (!m_file.is_open())
            return 0;
        auto pos = m_file.tellg();
        m_file.seekg(0, std::ios::end);
        int64_t sz = static_cast<int64_t>(m_file.tellg());
        m_file.seekg(pos);
        return sz;
    }
    bool seek(size_t pos) { return m_file.seekg(static_cast<std::streamoff>(pos)).good(); }
    std::vector<uint8_t> read(int n)
    {
        std::vector<uint8_t> out(static_cast<size_t>(n), 0);
        if (n <= 0)
            return out;
        m_file.read(reinterpret_cast<char*>(out.data()), n);
        out.resize(static_cast<size_t>(m_file.gcount()));
        return out;
    }
    int read(char* buf, int n)
    {
        if (n <= 0)
            return 0;
        m_file.read(buf, n);
        return static_cast<int>(m_file.gcount());
    }
    std::string readAll()
    {
        if (!m_file.is_open())
            return {};
        m_file.seekg(0, std::ios::end);
        auto sz = static_cast<size_t>(m_file.tellg());
        m_file.seekg(0);
        std::string out(sz, '\0');
        if (sz > 0)
            m_file.read(&out[0], static_cast<std::streamsize>(sz));
        return out;
    }
    std::istream* stream() { return m_file.is_open() ? &m_file : nullptr; }
    std::istream& getStream() { return m_file; }

  private:
    std::string m_path;
    std::ifstream m_file;
};

}  // namespace RawrXD
