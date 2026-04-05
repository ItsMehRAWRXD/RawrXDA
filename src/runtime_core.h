#pragma once
#include <string>
#include <functional>

void init_runtime();
std::string process_prompt(const std::string& input);
std::string process_prompt_stream(const std::string& input,
								  const std::function<bool(const std::string&)>& on_chunk);

void set_mode(const std::string& mode_str);
void set_engine(const std::string& engine_name);

void set_deep_thinking(bool v);
void set_deep_research(bool v);
void set_no_refusal(bool v);
void set_context(size_t tokens);
void runtime_load_model(const std::string& path);
std::string get_active_engine_name();

// Getters for React Server
std::string get_mode();
std::string get_active_engine();
bool get_deep_thinking();
bool get_deep_research();
size_t get_context();
