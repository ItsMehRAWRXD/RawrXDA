#pragma once
#include "AppState.h"
#include <fstream>
#include <sstream>
#include <string>

namespace baseline_profile {

    inline const char* ProfilePath()
    {
        return "baseline_profile.cfg";
    }

    inline void Load(AppState& s)
    {
        std::ifstream in(ProfilePath(), std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            s.baseline_loaded = false;
            return;
        }

        std::string line;
        while (std::getline(in, line))
        {
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos)
            {
                continue;
            }

            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);

            if (key == "model_path") s.model_path = value;
            else if (key == "temperature") s.temperature = std::strtof(value.c_str(), nullptr);
            else if (key == "top_p") s.top_p = std::strtof(value.c_str(), nullptr);
            else if (key == "is_gpu_enabled") s.is_gpu_enabled = true; // GPU is mandatory; on-disk false is ignored
            else if (key == "thread_count") s.thread_count = std::atoi(value.c_str());
            else if (key == "vram_limit_mb") s.vram_limit_mb = std::atoi(value.c_str());
            else if (key == "target_all_core_mhz") s.target_all_core_mhz = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            else if (key == "baseline_detected_mhz") s.baseline_detected_mhz = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            else if (key == "baseline_stable_offset_mhz") s.baseline_stable_offset_mhz = std::atoi(value.c_str());
            else if (key == "max_cpu_temp_c") s.max_cpu_temp_c = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            else if (key == "max_gpu_hotspot_c") s.max_gpu_hotspot_c = static_cast<uint32_t>(std::strtoul(value.c_str(), nullptr, 10));
            else if (key == "enable_max_mode") s.enable_max_mode = (value == "1");
            else if (key == "enable_deep_thinking") s.enable_deep_thinking = (value == "1");
            else if (key == "enable_deep_research") s.enable_deep_research = (value == "1");
            else if (key == "enable_no_refusal") s.enable_no_refusal = (value == "1");
            else if (key == "enable_autocorrect") s.enable_autocorrect = (value == "1");
        }

        s.baseline_loaded = true;
    }

    inline void Save(AppState& s)
    {
        std::ofstream out(ProfilePath(), std::ios::out | std::ios::binary | std::ios::trunc);
        if (!out.is_open())
        {
            return;
        }

        out << "model_path=" << s.model_path << '\n';
        out << "temperature=" << s.temperature << '\n';
        out << "top_p=" << s.top_p << '\n';
        out << "is_gpu_enabled=" << (s.is_gpu_enabled ? 1 : 0) << '\n';
        out << "thread_count=" << s.thread_count << '\n';
        out << "vram_limit_mb=" << s.vram_limit_mb << '\n';
        out << "target_all_core_mhz=" << s.target_all_core_mhz << '\n';
        out << "baseline_detected_mhz=" << s.baseline_detected_mhz << '\n';
        out << "baseline_stable_offset_mhz=" << s.baseline_stable_offset_mhz << '\n';
        out << "max_cpu_temp_c=" << s.max_cpu_temp_c << '\n';
        out << "max_gpu_hotspot_c=" << s.max_gpu_hotspot_c << '\n';
        out << "enable_max_mode=" << (s.enable_max_mode ? 1 : 0) << '\n';
        out << "enable_deep_thinking=" << (s.enable_deep_thinking ? 1 : 0) << '\n';
        out << "enable_deep_research=" << (s.enable_deep_research ? 1 : 0) << '\n';
        out << "enable_no_refusal=" << (s.enable_no_refusal ? 1 : 0) << '\n';
        out << "enable_autocorrect=" << (s.enable_autocorrect ? 1 : 0) << '\n';
        s.baseline_loaded = true;
    }
}
