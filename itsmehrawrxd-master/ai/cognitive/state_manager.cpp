// Cognitive State Manager for ADD/ADHD-like AI behavior
#include <vector>
#include <string>
#include <memory>
#include <random>
#include <queue>
#include <optional>
#include <chrono>

namespace IDE_AI {
namespace Cognitive {

// Focus structure
struct Focus {
    std::string code_context;
    std::string file_path;
    int line_number;
    std::chrono::steady_clock::time_point start_time;
    float intensity;
    
    Focus(const std::string& context, const std::string& path, int line, float intensity = 1.0f)
        : code_context(context), file_path(path), line_number(line), intensity(intensity) {
        start_time = std::chrono::steady_clock::now();
    }
};

// Distraction structure
struct Distraction {
    std::string origin;
    std::string content;
    float priority;
    std::chrono::steady_clock::time_point timestamp;
    
    Distraction(const std::string& origin, const std::string& content, float priority = 0.5f)
        : origin(origin), content(content), priority(priority) {
        timestamp = std::chrono::steady_clock::now();
    }
};

// Cognitive State Manager
class StateManager {
public:
    StateManager(IAIProvider* ai_provider, VectorDatabase* vdb)
        : ai_provider_(ai_provider), vdb_(vdb), current_focus_(std::nullopt) {
        rng_.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    // Main cognitive cycle
    void run_cycle() {
        // Check if we should shift attention
        if (should_shift_attention()) {
            shift_attention();
        }
        
        // Process current focus
        if (current_focus_.has_value()) {
            execute_focus_burst();
        }
        
        // Check for impulsive actions
        if (should_do_impulse_action()) {
            do_impulse_action();
        }
        
        // Process distractions
        process_distractions();
    }
    
    // Add a distraction
    void add_distraction(const Distraction& distraction) {
        distractions_.push(distraction);
    }
    
    // Set current focus
    void set_focus(const Focus& focus) {
        if (current_focus_.has_value()) {
            previous_focus_ = current_focus_;
        }
        current_focus_ = focus;
    }
    
    // Get current focus
    std::optional<Focus> get_current_focus() const {
        return current_focus_;
    }
    
    // Get previous focus
    std::optional<Focus> get_previous_focus() const {
        return previous_focus_;
    }
    
    // Get recent distractions
    std::vector<Distraction> get_recent_distractions() const {
        std::vector<Distraction> recent;
        auto queue_copy = distractions_;
        while (!queue_copy.empty()) {
            recent.push_back(queue_copy.front());
            queue_copy.pop();
        }
        return recent;
    }
    
    // Get impulsive refactor idea
    std::string get_impulsive_refactor_idea(const std::string& code_context) {
        if (ai_provider_) {
            std::string prompt = "Give me a quick refactor idea for this code: " + code_context;
            return ai_provider_->generateCompletion(prompt);
        }
        return "Consider extracting this into a separate function";
    }
    
private:
    // Check if we should shift attention
    bool should_shift_attention() {
        if (!current_focus_.has_value()) {
            return true;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto focus_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - current_focus_->start_time).count();
        
        // Probabilistic attention shift based on focus duration
        float shift_probability = std::min(0.1f + focus_duration * 0.01f, 0.8f);
        
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < shift_probability;
    }
    
    // Shift attention to a new focus
    void shift_attention() {
        if (distractions_.empty()) {
            return;
        }
        
        // Get highest priority distraction
        Distraction distraction = distractions_.front();
        distractions_.pop();
        
        // Create new focus from distraction
        Focus new_focus(distraction.content, "unknown", 0, distraction.priority);
        set_focus(new_focus);
    }
    
    // Execute a burst of focused work
    void execute_focus_burst() {
        if (!current_focus_.has_value()) {
            return;
        }
        
        // Simulate focused work
        std::uniform_int_distribution<int> work_dist(1, 5);
        int work_units = work_dist(rng_);
        
        for (int i = 0; i < work_units; ++i) {
            // Simulate work on current focus
            if (ai_provider_) {
                std::string prompt = "Continue working on: " + current_focus_->code_context;
                ai_provider_->generateCompletion(prompt);
            }
        }
    }
    
    // Check if we should do an impulsive action
    bool should_do_impulse_action() {
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng_) < 0.15f; // 15% chance of impulsive action
    }
    
    // Do an impulsive action
    void do_impulse_action() {
        std::vector<std::string> impulsive_actions = {
            "Refactor this function",
            "Add error handling",
            "Optimize this loop",
            "Add comments",
            "Extract this into a class",
            "Add unit tests",
            "Improve variable names"
        };
        
        std::uniform_int_distribution<int> action_dist(0, impulsive_actions.size() - 1);
        std::string action = impulsive_actions[action_dist(rng_)];
        
        // Create distraction from impulsive action
        Distraction impulse("impulse", action, 0.8f);
        add_distraction(impulse);
    }
    
    // Process pending distractions
    void process_distractions() {
        // Limit distraction queue size
        while (distractions_.size() > 10) {
            distractions_.pop();
        }
    }
    
    IAIProvider* ai_provider_;
    VectorDatabase* vdb_;
    std::optional<Focus> current_focus_;
    std::optional<Focus> previous_focus_;
    std::queue<Distraction> distractions_;
    std::mt19937 rng_;
};

} // namespace Cognitive
} // namespace IDE_AI