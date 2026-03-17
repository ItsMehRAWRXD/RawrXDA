#pragma once

#include &lt;string&gt;
#include &lt;vector&gt;
#include &lt;memory&gt;

namespace RawrXD {
namespace Modules {

class CodexUltimate {
public:
    CodexUltimate();
    ~CodexUltimate();

    bool initialize();
    std::string processCode(const std::string&amp; code);
    void shutdown();

private:
    bool initialized_;
};

} // namespace Modules
} // namespace RawrXD