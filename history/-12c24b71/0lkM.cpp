#include "overclock_governor.h"
#include "gui.h"
#include <iostream>
#include <cassert>

int main() {
    AppState st;
    st.boost_step_mhz = 25;
    // CPU PID output mapping
    if (OverclockGovernor::ComputeCpuDesiredDelta(6.0f, st) != 25) { std::cerr << "CPU mapping 6->25 failed" << std::endl; return 1; }
    if (OverclockGovernor::ComputeCpuDesiredDelta(2.0f, st) != 12) { std::cerr << "CPU mapping 2->12 failed" << std::endl; return 2; }
    if (OverclockGovernor::ComputeCpuDesiredDelta(-6.0f, st) != -25) { std::cerr << "CPU mapping -6->-25 failed" << std::endl; return 3; }
    if (OverclockGovernor::ComputeCpuDesiredDelta(-2.0f, st) != -12) { std::cerr << "CPU mapping -2->-12 failed" << std::endl; return 4; }

    if (OverclockGovernor::ComputeGpuDesiredDelta(6.0f, st) != 25) { std::cerr << "GPU mapping 6->25 failed" << std::endl; return 5; }
    if (OverclockGovernor::ComputeGpuDesiredDelta(-6.0f, st) != -25) { std::cerr << "GPU mapping -6->-25 failed" << std::endl; return 6; }

    std::cout << "test_pid_main: OK" << std::endl;
    return 0;
}
