#include "self_test_gate.hpp"
#include "self_test.hpp"
#include "rollback.hpp"

bool runSelfTestGate() {
    SelfTest st;
    Rollback rb;

    // 1. Functional Tests
    if (!st.runAll()) {
        std::cerr << "[!] Self-test failed. Reverting..." << std::endl;
        rb.revertLastCommit();
        rb.openIssue("Functional regression (Self-Test Failed)", st.lastOutput());
        return false;
    }

    // 2. Performance Regression
    if (rb.detectRegression()) {
        std::cerr << "[!] Performance regression detected. Reverting..." << std::endl;
        rb.revertLastCommit();
        rb.openIssue("Performance regression", st.lastOutput());
        return false;
    }

    return true;
}

