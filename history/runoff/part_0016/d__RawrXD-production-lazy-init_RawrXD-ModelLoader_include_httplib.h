#pragma once
#include <string>
#include <functional>
#include <map>

// Minimal stub of cpp-httplib for CLI builds; not production-ready.
namespace httplib {
struct Request {
    std::string body;
};

struct Response {
    void set_content(const std::string& data, const char* content_type) {
        (void)data; (void)content_type; // no-op
    }
};

class Server {
public:
    template<typename F>
    void Post(const std::string& /*path*/, F&& /*handler*/) {}
    template<typename F>
    void Get(const std::string& /*path*/, F&& /*handler*/) {}
    void listen(const std::string& /*host*/, int /*port*/) {}
};

} // namespace httplib
