#pragma once

/**
 * RawrXD Enhanced Features Integration
 * 
 * Unified entry point for all new features:
 * - Slash Command Parser
 * - Custom Instructions (Scoped)
 * - Multi-File Edit Planning
 * - Incremental Repository Indexing
 * 
 * NO EXTERNAL DEPENDENCIES
 */

// Slash Commands
#include "agentic/slash_command_parser.hpp"

// Scoped Instructions
#include "core/scoped_instructions_provider.hpp"

// Multi-File Edits
#include "agentic/multi_file_edit_plan.hpp"

// Incremental Indexing
#include "indexing/incremental_indexer.hpp"

namespace RawrXD {

/**
 * Enhanced Features Suite
 * 
 * Usage:
 *   - Parse slash commands: SlashCommandParser::Parse("/edit file1 file2")
 *   - Get instructions for file: ScopedInstructionsProvider::getForFile("src/main.cpp")
 *   - Plan multi-file edits: MultiFileEditPlanBuilder().withEditFile(...).build()
 *   - Monitor repo changes: IncrementalRepositoryIndexer::startMonitoring(...)
 */

class EnhancedFeaturesSuite {
public:
    /**
     * Initialize all features for a project
     */
    static void initialize(const std::string& projectRoot) {
        Agentic::SlashCommandParser::AvailableCommands();  // Verify
        Core::ScopedInstructionsProvider::instance().setProjectRoot(projectRoot);
        Indexing::IncrementalRepositoryIndexer::instance().initialize(projectRoot);
    }
    
    /**
     * Start monitoring repository for changes
     */
    static void startRepositoryMonitoring(
        std::function<void(const std::vector<Indexing::FileChange>&)> onChanges) {
        Indexing::IncrementalRepositoryIndexer::instance().startMonitoring(onChanges);
    }
    
    /**
     * Stop monitoring
     */
    static void stopRepositoryMonitoring() {
        Indexing::IncrementalRepositoryIndexer::instance().stopMonitoring();
    }
    
    /**
     * Parse user input (could be slash command or natural language)
     */
    static std::string processUserInput(const std::string& input) {
        if (Agentic::SlashCommandParser::IsSlashCommand(input)) {
            auto cmd = Agentic::SlashCommandParser::Parse(input);
            if (!cmd.valid) {
                return "Error: " + cmd.error + "\n" + 
                       Agentic::SlashCommandParser::GetHelp();
            }
            // Convert to tool call JSON for executor
            auto toolCall = cmd.ToToolCall();
            return toolCall.dump(2);
        }
        
        // Otherwise, treat as natural language query
        return input;
    }
};

} // namespace RawrXD
