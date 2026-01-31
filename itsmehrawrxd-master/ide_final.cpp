// The main application integrating all new UI and NLP systems
#include <iostream>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include "core/in_memory_fs.cpp"
#include "ai/in_memory_db.cpp"
#include "ai/providers/keyless_provider.cpp"
#include "ai/cognitive/state_manager.cpp"
#include "ui/visual_flow.cpp"
#include "ui/context_ui.cpp"
#include "nlp/nl_to_distraction.cpp"

// Assume model data is embedded at compile-time
extern const std::vector<unsigned char> EMBEDDED_MODEL_DATA;

int main() {
    std::cout << "Starting the elegant AI IDE with enhanced UI/UX..." << std::endl;
    
    // 1. Set up the fileless environment
    InMemoryFileSystem fs;
    IDE_AI::InMemoryVectorDatabase vdb(512);
    IDE_AI::InMemoryProjectKnowledgeGraph pkg;
    
    // 2. Initialize the keyless AI provider with embedded data
    auto keyless_provider = std::make_unique<IDE_AI::KeylessProvider>(EMBEDDED_MODEL_DATA);
    
    // 3. Initialize the ADD/ADHD cognitive state manager
    IDE_AI::Cognitive::StateManager state_manager(
        keyless_provider.get(),
        &vdb
    );

    // 4. Initialize UI components
    IDE_UI::Framework gui_framework;
    IDE_UI::VisualFlow visual_flow(&gui_framework, &state_manager);
    IDE_UI::ContextUI context_ui(&gui_framework, &state_manager);
    
    // 5. Initialize NLP component for natural language input
    NLP::NaturalLanguageToDistraction nl_processor(&state_manager, keyless_provider.get());

    std::cout << "Elegant AI IDE is running." << std::endl;
    
    // 6. Populate with initial code
    fs.write_file("src/main.cpp", R"(
#include <iostream>
#include "utils.h"

int main() {
    std::cout << "Hello, World!" << std::endl;
    
    int result = Utils::add(5, 3);
    std::cout << "5 + 3 = " << result << std::endl;
    
    return 0;
}
)");
    
    fs.write_file("src/utils.h", R"(
#pragma once

namespace Utils {
    int add(int a, int b);
    int multiply(int a, int b);
}
)");
    
    fs.write_file("src/utils.cpp", R"(
#include "utils.h"

namespace Utils {
    int add(int a, int b) {
        return a + b;
    }
    
    int multiply(int a, int b) {
        return a * b;
    }
}
)");
    
    // Add files to knowledge base
    for (const auto& file_path : fs.get_all_files()) {
        std::string content = fs.read_file(file_path);
        vdb.addDocument(file_path, std::vector<float>(512, 0.1f), content);
        
        pkg.addEntity(file_path, "file", {
            {"type", "source_code"},
            {"language", "cpp"},
            {"size", std::to_string(content.size())}
        });
    }
    
    std::cout << "Loaded " << fs.get_file_count() << " files into memory" << std::endl;
    
    // 7. Main event loop
    gui_framework.add_timer(100, [&]() {
        // Run AI cognitive cycle
        state_manager.run_cycle();
        
        // Update visual state and contextual UI
        visual_flow.update();
        context_ui.update();
    });
    
    // 8. Simulate user interactions
    std::cout << "\nSimulating user interactions..." << std::endl;
    
    // Simulate natural language commands
    std::vector<std::string> user_commands = {
        "Can you refactor the add function to be more efficient?",
        "Explain how the main function works",
        "Add error handling to the multiply function",
        "Create unit tests for the Utils namespace",
        "Optimize the code for better performance"
    };
    
    for (const auto& command : user_commands) {
        std::cout << "\nUser: " << command << std::endl;
        nl_processor.process_user_input(command);
        
        // Run a few cognitive cycles to process the command
        for (int i = 0; i < 5; ++i) {
            state_manager.run_cycle();
            visual_flow.update();
            context_ui.update();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    // 9. Demonstrate AI's cognitive state
    std::cout << "\nAI Cognitive State Summary:" << std::endl;
    auto current_focus = state_manager.get_current_focus();
    if (current_focus.has_value()) {
        std::cout << "Current focus: " << current_focus->code_context << std::endl;
    }
    
    auto previous_focus = state_manager.get_previous_focus();
    if (previous_focus.has_value()) {
        std::cout << "Previous focus: " << previous_focus->code_context << std::endl;
    }
    
    auto recent_distractions = state_manager.get_recent_distractions();
    std::cout << "Recent distractions: " << recent_distractions.size() << std::endl;
    for (const auto& distraction : recent_distractions) {
        std::cout << "  - " << distraction.origin << ": " << distraction.content << std::endl;
    }
    
    // 10. Demonstrate knowledge base operations
    std::cout << "\nKnowledge Base Operations:" << std::endl;
    std::vector<float> query_embedding(512, 0.1f);
    auto search_results = vdb.search(query_embedding, 3);
    
    std::cout << "Search results:" << std::endl;
    for (const auto& result : search_results) {
        std::cout << "  " << result.id << " (similarity: " << result.similarity << ")" << std::endl;
    }
    
    std::cout << "\nKnowledge graph entities:" << std::endl;
    auto entities = pkg.getEntitiesByType("file");
    for (const auto& entity : entities) {
        auto properties = pkg.getEntityProperties(entity);
        std::cout << "  " << entity << " (" << properties["language"] << ")" << std::endl;
    }
    
    std::cout << "\nElegant AI IDE demonstration completed successfully!" << std::endl;
    std::cout << "This system demonstrates:" << std::endl;
    std::cout << "- Fileless and keyless operation" << std::endl;
    std::cout << "- ADD/ADHD-like AI cognitive behavior" << std::endl;
    std::cout << "- Natural language interaction" << std::endl;
    std::cout << "- Context-aware UI suggestions" << std::endl;
    std::cout << "- In-memory knowledge management" << std::endl;
    
    return 0;
}