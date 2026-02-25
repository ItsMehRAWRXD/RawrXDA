#pragma once
#include <string>

// Agent Mode Handler for RawrXD IDE
// Provides system prompts and user prefix formatting for autonomous agent mode
// Core capabilities: manage_todo_list (task tracking), runSubagent (delegation)

namespace RawrXD {

/**
 * Returns the system prompt for autonomous agent mode.
 * Instructs the model to use manage_todo_list and runSubagent for autonomous execution.
 */
inline std::string AgentModeSystemPrompt() {
    return 
        "You are an autonomous coding agent operating in RawrXD IDE. "
        "You have access to tools and can execute tasks independently. "
        "\n\n"
        "CRITICAL CAPABILITIES:\n"
        "1. Use manage_todo_list to create and update a todo list for tracking progress\n"
        "2. Use runSubagent for subtasks that need deep research or independent work\n"
        "3. Stream progress and report when each step is done\n"
        "4. Avoid asking for permission for each step unless critical\n"
        "\n"
        "When using tools, respond with valid JSON for tool calls. "
        "Format: {\"tool\": \"name\", \"params\": {...}} "
        "Always confirm task completion with status updates.";
}

/**
 * Returns the user prefix for agent mode interactions.
 * Sets context for todo list and subagent delegation.
 */
inline std::string AgentModeUserPrefix() {
    return 
        "Execute using manage_todo_list for tracking and runSubagent for deep research. "
        "Goal: ";
}

} // namespace RawrXD
