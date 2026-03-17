#include "self_test_gate.hpp"
#include "self_test.hpp"
#include "rollback.hpp"
#include <iostream>

namespace RawrXD {

bool runSelfTestGate() {
    SelfTest st;
    if (!st.runAll()) {
        std::cerr << "Self-Test FAILED – aborting release" << std::endl;
        std::cerr << "SelfTest lastError: " << st.lastError() << std::endl;
        std::cerr << "SelfTest output:\n" << st.lastOutput() << std::endl;
        return false;
    }

    Rollback rb;
    if (rb.detectRegression()) {
        std::cerr << "Performance regression detected – reverting" << std::endl;
        rb.revertLastCommit();
        rb.openIssue("Performance regression", st.lastOutput());
        return false;
    }

    return true;
}

} // namespace RawrXD
