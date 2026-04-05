extern "C" void start_scheduler();
extern "C" void push_token_ollama(const char*, size_t);
extern "C" void push_token_local(const char*, size_t);

void StartInference(const std::string& prompt) {
    start_scheduler();
}

void push_token_ollama(const char* p, size_t len) {
    if (winner == 1) PushToken(p, len);
}

void push_token_local(const char* p, size_t len) {
    if (winner == 2) PushToken(p, len);
}