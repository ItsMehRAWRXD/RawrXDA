#include "planner.hpp"
#include <algorithm>
#include <regex>

json Planner::plan(const std::string& humanWish) {
    std::string wish = humanWish;
    std::transform(wish.begin(), wish.end(), wish.begin(), [](unsigned char c){ return std::tolower(c); });
    
    if (wish.find("yourself") != std::string::npos || wish.find("itself") != std::string::npos || 
        wish.find("clone") != std::string::npos || wish.find("replicate") != std::string::npos || 
        wish.find("copy of you") != std::string::npos || wish.find("same thing") != std::string::npos || 
        wish.find("another you") != std::string::npos || wish.find("duplicate") != std::string::npos || 
        wish.find("second version") != std::string::npos) {
        return planSelfReplication(humanWish);
    }
    
    if (wish.find("faster") != std::string::npos || wish.find("optimize") != std::string::npos || 
        wish.find("speed up") != std::string::npos || wish.find("q8_k") != std::string::npos || 
        wish.find("q6_k") != std::string::npos || wish.find("quant") != std::string::npos) {
        return planQuantKernel(humanWish);
    }
    
    if (wish.find("release") != std::string::npos || wish.find("ship") != std::string::npos || 
        wish.find("publish") != std::string::npos || wish.find("share") != std::string::npos || 
        wish.find("deploy") != std::string::npos) {
        return planRelease(humanWish);
    }
    
    if (wish.find("website") != std::string::npos || wish.find("web app") != std::string::npos || 
        wish.find("dashboard") != std::string::npos || wish.find("admin panel") != std::string::npos || 
        wish.find("user interface") != std::string::npos || wish.find("react") != std::string::npos || 
        wish.find("vue") != std::string::npos || wish.find("angular") != std::string::npos || 
        wish.find("frontend") != std::string::npos || wish.find("ui") != std::string::npos) {
        return planWebProject(humanWish);
    }
    
    if (wish.find("api") != std::string::npos || wish.find("backend") != std::string::npos || 
        wish.find("server") != std::string::npos || wish.find("endpoint") != std::string::npos || 
        wish.find("rest") != std::string::npos || wish.find("graphql") != std::string::npos || 
        wish.find("express") != std::string::npos || wish.find("fastapi") != std::string::npos || 
        wish.find("flask") != std::string::npos) {
        return planWebProject(humanWish);
    }
    
    return planGeneric(humanWish);
}

json Planner::planQuantKernel(const std::string& wish) {
    json tasks = json::array();
    
    std::regex re(R"((Q\d+_[KM]|F16|F32))", std::regex_constants::icase);
    std::smatch m;
    std::string quantType = "Q8_K";
    if (std::regex_search(wish, m, re)) {
        quantType = m.str(1);
        std::transform(quantType.begin(), quantType.end(), quantType.begin(), ::toupper);
    }
    
    tasks.push_back({{"type", "add_kernel"}, {"target", quantType}, {"lang", "comp"}, {"template", "quant_vulkan.comp"}});
    tasks.push_back({{"type", "add_cpp"}, {"target", "quant_" + quantType + "_wrapper"}, {"deps", {quantType + ".comp"}}});
    tasks.push_back({{"type", "add_menu"}, {"target", quantType}, {"menu", "AI"}});
    tasks.push_back({{"type", "bench"}, {"target", quantType}, {"metric", "tokens/sec"}, {"threshold", 0.95}});
    tasks.push_back({{"type", "self_test"}, {"target", quantType}, {"cases", 50}});
    tasks.push_back({{"type", "hot_reload"}});
    tasks.push_back({{"type", "meta_learn"}});
    
    return tasks;
}

json Planner::planRelease(const std::string& wish) {
    json tasks = json::array();
    tasks.push_back({{"type", "self_code"}, {"target", "release_agent"}});
    tasks.push_back({{"type", "sign_binary"}, {"target", "RawrXD.exe"}});
    tasks.push_back({{"type", "auto_update"}, {"target", "push"}});
    return tasks;
}

json Planner::planWebProject(const std::string& wish) {
    json tasks = json::array();
    tasks.push_back({{"type", "invoke_command"}, {"cmd", "npx create-next-app@latest"}});
    tasks.push_back({{"type", "file_edit"}, {"target", "next.config.js"}});
    return tasks;
}

json Planner::planSelfReplication(const std::string& wish) {
    json tasks = json::array();
    tasks.push_back({{"type", "self_code"}, {"target", "clone"}});
    tasks.push_back({{"type", "self_patch"}, {"target", "recursive_evolution"}});
    return tasks;
}

json Planner::planGeneric(const std::string& wish) {
    json tasks = json::array();
    tasks.push_back({{"type", "search_files"}, {"pattern", wish}});
    return tasks;
}
