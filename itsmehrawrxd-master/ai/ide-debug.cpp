#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "model_layer.hpp"
#include "ide-vdb.hpp"
#include "language_sdk/api.hpp"

namespace IDE_AI {

// Debug state information
struct DebugState {
    std::string current_file;
    int current_line;
    std::string current_function;
    std::vector<std::string> call_stack;
    std::map<std::string, std::string> variables;
    std::string error_message;
    
    std::string call_stack_summary() const {
        std::string summary;
        for (const auto& frame : call_stack) {
            summary += frame + "\n";
        }
        return summary;
    }
};

// Low-level debugger core interface
class DebuggerCore {
public:
    virtual ~DebuggerCore() = default;
    virtual void start(const std::string& executable_path) = 0;
    virtual void set_breakpoint(const std::string& file, int line) = 0;
    virtual void continue_execution() = 0;
    virtual void step_over() = 0;
    virtual void step_into() = 0;
    virtual DebugState get_current_state() = 0;
    virtual std::string get_source_code(const std::string& file, int start_line, int num_lines) = 0;
    
    // Event callbacks
    std::function<void(const DebugState&)> on_breakpoint_hit;
    std::function<void(const DebugState&)> on_exception;
};

// AI-powered debugger that provides intelligent analysis
class AIDebugger {
public:
    AIDebugger(std::shared_ptr<DebuggerCore> core, 
               std::shared_ptr<CompletionModel> ai_model, 
               std::shared_ptr<VectorDatabase> vdb)
        : core_(core), ai_model_(ai_model), vdb_(vdb) {
        
        // Set up debugger event handlers
        core_->on_breakpoint_hit = [this](const DebugState& state) {
            this->handle_breakpoint_hit(state);
        };
        
        core_->on_exception = [this](const DebugState& state) {
            this->handle_exception(state);
        };
    }
    
    void start_debugging(const std::string& executable_path) {
        std::cout << "Starting AI-powered debugging session...\n";
        core_->start(executable_path);
    }
    
    void set_breakpoint(const std::string& file, int line) {
        core_->set_breakpoint(file, line);
        std::cout << "Breakpoint set at " << file << ":" << line << "\n";
    }
    
    void continue_execution() {
        core_->continue_execution();
    }
    
    void step_over() {
        core_->step_over();
    }
    
    void step_into() {
        core_->step_into();
    }
    
    void explain_current_state() {
        DebugState state = core_->get_current_state();
        explain_context(state);
    }
    
    void suggest_fixes() {
        DebugState state = core_->get_current_state();
        suggest_fixes(state);
    }

private:
    void handle_breakpoint_hit(const DebugState& state) {
        std::cout << "\n=== BREAKPOINT HIT ===\n";
        std::cout << "Location: " << state.current_file << ":" << state.current_line << "\n";
        std::cout << "Function: " << state.current_function << "\n";
        
        explain_context(state);
        suggest_fixes(state);
        
        std::cout << "\nVariables:\n";
        for (const auto& var : state.variables) {
            std::cout << "  " << var.first << " = " << var.second << "\n";
        }
    }
    
    void handle_exception(const DebugState& state) {
        std::cout << "\n=== EXCEPTION CAUGHT ===\n";
        std::cout << "Error: " << state.error_message << "\n";
        std::cout << "Location: " << state.current_file << ":" << state.current_line << "\n";
        
        explain_context(state);
        suggest_fixes(state);
    }
    
    void explain_context(const DebugState& state) {
        std::string current_code = core_->get_source_code(state.current_file, 
                                                         state.current_line - 5, 10);
        
        std::string prompt = "Explain the current state of the program at this point in the code:\n\n" 
                           + current_code + "\n\nCall stack:\n" + state.call_stack_summary();
        
        // Retrieve relevant context from the vector database
        std::string context_text = current_code + "\n" + state.call_stack_summary();
        Embedding query_embedding = ai_model_->generate_embedding(context_text);
        auto relevant_docs = vdb_->search(query_embedding, 3);
        
        // Augment prompt with relevant project context
        std::string augmented_prompt = "Relevant project context:\n";
        for (const auto& doc_id : relevant_docs) {
            augmented_prompt += vdb_->get_metadata(doc_id) + "\n";
        }
        augmented_prompt += "\n" + prompt;
        
        std::string explanation = ai_model_->generate_completion(augmented_prompt);
        
        std::cout << "\n--- AI EXPLANATION ---\n";
        std::cout << explanation << "\n";
        std::cout << "--- END EXPLANATION ---\n";
    }
    
    void suggest_fixes(const DebugState& state) {
        std::string current_code = core_->get_source_code(state.current_file, 
                                                         state.current_line - 3, 6);
        
        std::string prompt = "Analyze this code and suggest potential fixes for any issues:\n\n" 
                           + current_code + "\n\nCurrent variables:\n";
        
        for (const auto& var : state.variables) {
            prompt += "  " + var.first + " = " + var.second + "\n";
        }
        
        if (!state.error_message.empty()) {
            prompt += "\nError message: " + state.error_message + "\n";
        }
        
        // Get relevant debugging examples from the vector database
        std::string debug_context = current_code + " " + state.error_message;
        Embedding query_embedding = ai_model_->generate_embedding(debug_context);
        auto relevant_docs = vdb_->search(query_embedding, 2);
        
        std::string augmented_prompt = "Similar debugging scenarios from project:\n";
        for (const auto& doc_id : relevant_docs) {
            augmented_prompt += vdb_->get_metadata(doc_id) + "\n";
        }
        augmented_prompt += "\n" + prompt;
        
        std::string suggestions = ai_model_->generate_completion(augmented_prompt);
        
        std::cout << "\n--- AI SUGGESTIONS ---\n";
        std::cout << suggestions << "\n";
        std::cout << "--- END SUGGESTIONS ---\n";
    }
    
    std::shared_ptr<DebuggerCore> core_;
    std::shared_ptr<CompletionModel> ai_model_;
    std::shared_ptr<VectorDatabase> vdb_;
};

} // namespace IDE_AI
