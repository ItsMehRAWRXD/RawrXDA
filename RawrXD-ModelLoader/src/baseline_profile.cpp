#include "baseline_profile.h"
#include "gui.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace baseline_profile {

bool Load(AppState& state, const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    // Very naive parsing
    auto findVal = [&](const char* key)->int{
        std::string k = std::string("\"") + key + "\":";
        auto pos = content.find(k);
        if (pos==std::string::npos) return 0;
        pos += k.size();
        size_t end = content.find_first_of(",}\n", pos);
        std::string num = content.substr(pos, end-pos);
        try { return std::stoi(num); } catch(...) { return 0; }
    };
    state.baseline_detected_mhz = (uint32_t)findVal("baseline_detected_mhz");
    state.baseline_stable_offset_mhz = findVal("baseline_stable_offset_mhz");
    state.baseline_loaded = state.baseline_detected_mhz > 0;
    return state.baseline_loaded;
}

bool Save(const AppState& state, const std::string& path) {
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    std::ofstream ofs(path, std::ios::trunc);
    if (!ofs.is_open()) return false;
    ofs << "{\n";
    ofs << "  \"baseline_detected_mhz\": " << state.baseline_detected_mhz << ",\n";
    ofs << "  \"baseline_stable_offset_mhz\": " << state.baseline_stable_offset_mhz << "\n";
    ofs << "}\n";
    return true;
}

} // namespace baseline_profile
