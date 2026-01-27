#include "self_test_gate.hpp"
#include "self_test.hpp"
#include "rollback.hpp"
#include <QDebug>

bool runSelfTestGate() {
    SelfTest st;
    if (!st.runAll()) {
        qWarning() << "Self-Test FAILED – aborting release";
        qWarning() << "SelfTest lastError:" << st.lastError();
        qWarning() << "SelfTest output:\n" << st.lastOutput();
        return false;
    }

    Rollback rb;
    if (rb.detectRegression()) {
        qWarning() << "Performance regression detected – reverting";
        rb.revertLastCommit();
        rb.openIssue(QStringLiteral("Performance regression"), st.lastOutput());
        return false;
    }

    return true;
}
