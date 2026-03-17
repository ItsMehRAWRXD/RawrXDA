#include "baseline_profile.h"
#include "gui.h"
#include <cassert>
#include <filesystem>
#include <iostream>

int main() {
    AppState st;
    st.baseline_detected_mhz = 4200;
    st.baseline_stable_offset_mhz = 55;
    std::filesystem::path p = "settings/test_oc_baseline.json";
    std::filesystem::create_directories(p.parent_path());
    bool ok = baseline_profile::Save(st, p.string());
    assert(ok && "Save should succeed");

    // Clear and reload
    AppState st2;
    ok = baseline_profile::Load(st2, p.string());
    assert(ok && "Load should succeed");
    assert(st2.baseline_detected_mhz == 4200);
    assert(st2.baseline_stable_offset_mhz == 55);
    std::cout << "test_baseline: OK" << std::endl;
    return 0;
}
