#ifndef GDB_MI_H
#define GDB_MI_H

// C++20, no Qt. Minimal GDB/MI parser. Returns JSON as std::string.

#include <string>

class GdbMI
{
public:
    static std::string parseOutputRecord(const std::string& record);
    static std::string parseResultRecord(const std::string& record);
    static std::string parseAsyncRecord(const std::string& record);

private:
    static std::string parseCString(const std::string& str, size_t& index);
    static std::string parseTuple(const std::string& str, size_t& index);
    static std::string parseList(const std::string& str, size_t& index);
    static std::string parseResult(const std::string& str, size_t& index);
};

#endif // GDB_MI_H
