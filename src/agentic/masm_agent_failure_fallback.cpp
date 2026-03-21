namespace {
static unsigned g_masmAgentFailureDetectCalls = 0;
}

extern "C" void masm_agent_failure_detect_simd(void) {
    g_masmAgentFailureDetectCalls += 1;
}
