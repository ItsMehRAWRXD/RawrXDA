public class SampleLLMPlugin implements PluginSandbox.LLMPlugin {
    private boolean initialized = false;
    private RealLLMClient llmClient;

    @Override
    public String getName() {
        return "Sample LLM";
    }

    @Override
    public void initialize() throws Exception {
        // Initialize the real LLM client (replace with actual setup)
        llmClient = new RealLLMClient(/* config or API key */);
        llmClient.connect();
        initialized = true;
    }

    @Override
    public String process(String input) throws Exception {
        if (!initialized) throw new Exception("Plugin not initialized");
        // Remove emojis before sending to LLM
        String cleanInput = EmojiRemover.removeEmojisAdvanced(input);
        // Send to real LLM and get response
        String response = llmClient.generateCompletion(cleanInput);
        // Optionally remove emojis from response
        return EmojiRemover.removeEmojisAdvanced(response);
    }

    @Override
    public void cleanup() {
        if (llmClient != null) llmClient.disconnect();
        initialized = false;
    }
}