// RawrXD_Planner.hpp - Autonomous Planning Engine
// Pure C++20 - No Qt Dependencies
// Ported from: planner.cpp

#pragma once

#include "RawrXD_JSON.hpp"
#include <string>
#include <vector>
#include <regex>
#include <algorithm>

namespace RawrXD {

class Planner {
public:
    static JSONArray Plan(const std::string& wish) {
        std::string lowerWish = wish;
        std::transform(lowerWish.begin(), lowerWish.end(), lowerWish.begin(), ::tolower);

        if (containsAny(lowerWish, {"yourself", "itself", "clone", "replicate", "copy of you"})) {
            return PlanSelfReplication(wish);
        }
        
        if (containsAny(lowerWish, {"faster", "optimize", "speed up", "q8_k", "q6_k", "quant"})) {
            return PlanQuantKernel(wish);
        }
        
        if (containsAny(lowerWish, {"release", "ship", "publish", "share", "deploy"})) {
            return PlanRelease(wish);
        }
        
        if (containsAny(lowerWish, {"website", "web app", "dashboard", "ui", "frontend"})) {
            return PlanWebProject(wish);
        }

        return PlanGeneric(wish);
    }

private:
    static bool containsAny(const std::string& str, const std::vector<std::string>& keywords) {
        for (const auto& k : keywords) {
            if (str.find(k) != std::string::npos) return true;
        }
        return false;
    }

    static JSONArray PlanQuantKernel(const std::string& wish) {
        JSONArray tasks;
        std::regex re("(Q\\d+_[KM]|F16|F32)", std::regex_constants::icase);
        std::smatch m;
        std::string quantType = "Q8_K";
        if (std::regex_search(wish, m, re)) quantType = m.str(1);

        auto createTask = [&](const std::string& type, const JSONObject& params) {
            JSONObject task = params;
            task["type"] = JSONValue(type);
            tasks.push_back(JSONValue(task));
        };

        createTask("add_kernel", {{"target", quantType}, {"template", "quant_vulkan.comp"}});
        createTask("add_cpp", {{"target", "quant_" + quantType + "_wrapper"}});
        createTask("bench", {{"target", quantType}, {"metric", "tokens/sec"}});
        createTask("self_test", {{"target", quantType}, {"cases", 50}});
        createTask("hot_reload", {});
        
        return tasks;
    }

    static JSONArray PlanRelease(const std::string& wish) {
        JSONArray tasks;
        std::string part = "patch";
        if (wish.find("major") != std::string::npos) part = "major";
        else if (wish.find("minor") != std::string::npos) part = "minor";

        auto createTask = [&](const std::string& type, const JSONObject& params) {
            JSONObject task = params;
            task["type"] = JSONValue(type);
            tasks.push_back(JSONValue(task));
        };

        createTask("bump_version", {{"part", part}});
        createTask("build", {{"target", "RawrXD_IDE"}});
        createTask("bench_all", {{"metric", "tokens/sec"}});
        createTask("self_test", {{"cases", 100}});
        createTask("tag", {});
        createTask("tweet", {{"text", "🚀 New release shipped fully autonomously from RawrXD IDE!"}});

        return tasks;
    }

    static JSONArray PlanSelfReplication(const std::string& wish) {
        JSONArray tasks;
        std::string cloneName = "RawrXD-Clone";
        
        std::regex nameRe("call(?:ed)? ([\\w-]+)", std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(wish, m, nameRe)) cloneName = m.str(1);

        auto createTask = [&](const std::string& type, const JSONObject& params) {
            JSONObject task = params;
            task["type"] = JSONValue(type);
            tasks.push_back(JSONValue(task));
        };

        createTask("create_directory", {{"path", cloneName}});
        createTask("clone_source", {{"source", "."}, {"target", cloneName}});
        createTask("build", {{"path", cloneName}});
        createTask("self_test", {{"path", cloneName}});

        return tasks;
    }

    static JSONArray PlanWebProject(const std::string& wish) {
        JSONArray tasks;
        auto createTask = [&](const std::string& type, const JSONObject& params) {
            JSONObject task = params;
            task["type"] = JSONValue(type);
            tasks.push_back(JSONValue(task));
        };

        createTask("create_web_boilerplate", {{"framework", "react"}});
        createTask("add_ai_endpoint", {{"route", "/api/chat"}});
        createTask("deploy_local", {});
        
        return tasks;
    }

    static JSONArray PlanGeneric(const std::string& wish) {
        JSONArray tasks;
        JSONObject task;
        task["type"] = JSONValue("analyze_and_execute");
        task["wish"] = JSONValue(wish);
        tasks.push_back(JSONValue(task));
        return tasks;
    }
};

} // namespace RawrXD
