#include "overclock_governor.h"
#include "gui.h"
#include <cassert>
#include <iostream>

int RunPidTest() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    assert(OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) == 12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) == -25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) == -12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(0.5f, st) == 0);

    // GPU PID mapping (uses same step logic)
    assert(OverclockGovernor::ComputeGpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeGpuDesiredDelta(-6.0f, st) == -25);

    std::cout << "test_pid: OK" << std::endl;
    return 0;
}
#include "overclock_governor.h"
#include "gui.h"
#include <cassert>
#include <iostream>

int RunPidTest() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    assert(OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) == 12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) == -25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) == -12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(0.5f, st) == 0);

    // GPU PID mapping (uses same step logic)
    assert(OverclockGovernor::ComputeGpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeGpuDesiredDelta(-6.0f, st) == -25);

    std::cout << "test_pid: OK" << std::endl;
    return 0;
}
#include "overclock_governor.h"
#include "gui.h"
#include <cassert>
#include <iostream>

int RunPidTest() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    assert(OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) == 12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) == -25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) == -12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(0.5f, st) == 0);

    // GPU PID mapping (uses same step logic)
    assert(OverclockGovernor::ComputeGpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeGpuDesiredDelta(-6.0f, st) == -25);

    std::cout << "test_pid: OK" << std::endl;
    return 0;
}
#include "overclock_governor.h"
#include "gui.h"
#include <cassert>
#include <iostream>

int RunPidTest() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    assert(OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) == 12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) == -25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) == -12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(0.5f, st) == 0);

    // GPU PID mapping (uses same step logic)
    assert(OverclockGovernor::ComputeGpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeGpuDesiredDelta(-6.0f, st) == -25);

    std::cout << "test_pid: OK" << std::endl;
    return 0;
}
#include "overclock_governor.h"
#include "gui.h"
#include <iostream>

int TestPid() {
    // Quick mapping checks
    int d1 = OverclockGovernor::ComputePidDelta(10.0f, 40);
    if (d1 != 40) { std::cerr << "Expected 40 for 10.0 pidOutput, got " << d1 << std::endl; return 1; }
    int d2 = OverclockGovernor::ComputePidDelta(2.0f, 40);
    if (d2 != 20) { std::cerr << "Expected 20 for 2.0 pidOutput, got " << d2 << std::endl; return 2; }
    int d3 = OverclockGovernor::ComputePidDelta(-10.0f, 40);
    if (d3 != -40) { std::cerr << "Expected -40 for -10.0 pidOutput, got " << d3 << std::endl; return 3; }
    int d4 = OverclockGovernor::ComputePidDelta(-2.0f, 40);
    if (d4 != -20) { std::cerr << "Expected -20 for -2.0 pidOutput, got " << d4 << std::endl; return 4; }
    int d5 = OverclockGovernor::ComputePidDelta(0.0f, 40);
    if (d5 != 0) { std::cerr << "Expected 0 for 0.0 pidOutput, got " << d5 << std::endl; return 5; }
    std::cout << "PID mapping tests OK" << std::endl;
    return 0;
}
#include "overclock_governor.h"
#include "gui.h"
#include <cassert>
#include <iostream>
int RunPidTest() {
int main() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    assert(OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) == 25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) == 12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) == -25);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) == -12);
    assert(OverclockGovernor::ComputeCpuDesiredDelta(0.5f, st) == 0);

    // GPU PID mapping (uses same step logic)
    std::cout << "PID mapping tests OK" << std::endl;
    return 0;

    std::cout << "test_pid: OK" << std::endl;
    return 0;
}
