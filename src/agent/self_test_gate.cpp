#include "self_test_gate.hpp"
#include "self_test.hpp"
#include "rollback.hpp"

bool runSelfTestGate() {
    SelfTest st;
    if (!st.runAll()) {
        return false;
    }

    Rollback rb;
    if (rb.detectRegression()) {
        rb.revertLastCommit();
        rb.openIssue("Performance regression", st.lastOutput());
        return false;
    }

    return true;
}

