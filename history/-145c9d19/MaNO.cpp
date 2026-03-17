#include "baseline_profile.h"
#include "gui.h"
#include <iostream>
#include <filesystem>

int TestBaseline() {
    AppState st;
    st.baseline_detected_mhz = 4800;
    st.baseline_stable_offset_mhz = 50;
    std::string tmp = "settings/test_oc_baseline.json";
    std::filesystem::create_directories(std::filesystem::path(tmp).parent_path());
    if (!baseline_profile::Save(st, tmp)) { std::cerr << "Save failed" << std::endl; return 1; }
    AppState st2;
    if (!baseline_profile::Load(st2, tmp)) { std::cerr << "Load failed" << std::endl; return 2; }
    if (st2.baseline_detected_mhz != st.baseline_detected_mhz) { std::cerr << "Mismatch detected" << std::endl; return 3; }
    if (st2.baseline_stable_offset_mhz != st.baseline_stable_offset_mhz) { std::cerr << "Offset mismatch" << std::endl; return 4; }
    std::cout << "Baseline roundtrip OK" << std::endl;
    // Clean up
    std::filesystem::remove(tmp);
    return 0;
}
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
