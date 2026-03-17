#pragma once
#include <string>

void init_runtime();
std::string process_prompt(const std::string& input);

void set_mode(const std::string& mode_str);
void set_engine(const std::string& engine_name);

void set_deep_thinking(bool v);
void set_deep_research(bool v);
void set_no_refusal(bool v);
void set_context(size_t tokens);

// Getters for React Server
std::string get_mode();
std::string get_active_engine();
bool get_deep_thinking();
bool get_deep_research();
size_t get_context();
