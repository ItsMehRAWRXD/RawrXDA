// Places AI-generated suggestions, explanations, or questions directly in the code
#include <iostream>
#include <string>
#include <memory>
#include <vector>

namespace IDE_UI {

class ContextUI {
public:
    ContextUI(Framework* gui_framework, IDE_AI::Cognitive::StateManager* state_manager)
        : gui_(gui_framework), state_manager_(state_manager) {}

    void update() {
        // Example: If the AI's focus was previously a specific function,
        // it might suggest a refactor right next to that function.
        auto past_focus = state_manager_->get_previous_focus();
        if (past_focus.has_value()) {
            std::string impulsive_idea = state_manager_->get_impulsive_refactor_idea(past_focus->code_context);
            if (!impulsive_idea.empty()) {
                gui_->insert_inline_suggestion(impulsive_idea, past_focus->code_context);
            }
        }
        
        // Example: If the AI was distracted by a new file being added,
        // it might put a "Did you mean to include this?" comment at the top of the nearest relevant file.
        for (const auto& distraction : state_manager_->get_recent_distractions()) {
            if (distraction.origin == "fs_event") {
                gui_->insert_comment_at_top("AI: I noticed a new file was added. Related to this section?");
            }
        }
        
        // Add contextual suggestions based on current focus
        auto current_focus = state_manager_->get_current_focus();
        if (current_focus.has_value()) {
            addContextualSuggestions(current_focus.value());
        }
    }
    
private:
    void addContextualSuggestions(const IDE_AI::Cognitive::Focus& focus) {
        // Add suggestions based on the current focus
        if (focus.code_context.find("function") != std::string::npos) {
            gui_->insert_inline_suggestion("Consider adding error handling", focus.code_context);
        } else if (focus.code_context.find("class") != std::string::npos) {
            gui_->insert_inline_suggestion("Consider adding a destructor", focus.code_context);
        } else if (focus.code_context.find("loop") != std::string::npos) {
            gui_->insert_inline_suggestion("Consider optimizing this loop", focus.code_context);
        }
    }

    Framework* gui_;
    IDE_AI::Cognitive::StateManager* state_manager_;
};

} // namespace IDE_UI