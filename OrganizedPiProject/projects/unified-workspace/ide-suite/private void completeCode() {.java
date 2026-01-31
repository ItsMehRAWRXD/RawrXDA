private void completeCode() {
    String text = editor.getSelectedText();
    if (text == null) text = editor.getText();
    // Remove emojis before sending to Copilot
    text = EmojiRemover.removeEmojisAdvanced(text);
    copilot.complete(text).whenComplete((result, error) -> {
        SwingUtilities.invokeLater(() -> {
            if (error == null) {
                // Remove emojis from Copilot's completion before displaying
                String cleaned = EmojiRemover.removeEmojisAdvanced(result);
                editor.append("\n" + cleaned);
            } else {
                JOptionPane.showMessageDialog(null, "Error: " + error.getMessage());
            }
        });
    });
}

// Amazon Q Provider
private class AmazonQProvider implements AIProvider {
    @Override
    public String process(String prompt, RequestType type) throws Exception {
        // Remove emojis and ensure non-null
        String cleanPrompt = EmojiRemover.removeEmojisAdvanced(prompt);
        if (cleanPrompt == null) cleanPrompt = "";
        Thread.sleep(500); // Simulate network call
        String result = "Amazon Q: " + getSystemPrompt(type) + "\n\nFor: " +
            cleanPrompt.substring(0, Math.min(50, cleanPrompt.length()));
        // Remove emojis from result and ensure non-null
        String cleanedResult = EmojiRemover.removeEmojisAdvanced(result);
        return cleanedResult == null ? "" : cleanedResult;
    }
    @Override
    public boolean isAvailable() { return true; }
    @Override
    public String getStatus() { return "Mock Available"; }
}

// Local AI Provider
private class LocalAIProvider implements AIProvider {
    @Override
    public String process(String prompt, RequestType type) throws Exception {
        // Remove emojis and ensure non-null
        String cleanPrompt = EmojiRemover.removeEmojisAdvanced(prompt);
        if (cleanPrompt == null) cleanPrompt = "";
        Thread.sleep(300);
        String result = "Local AI: Processing " + type + " for prompt length " + cleanPrompt.length();
        // Remove emojis from result and ensure non-null
        String cleanedResult = EmojiRemover.removeEmojisAdvanced(result);
        return cleanedResult == null ? "" : cleanedResult;
    }
    @Override
    public boolean isAvailable() { return true; }
    @Override
    public String getStatus() { return "Local Engine Ready"; }
}

// Utility methods
private static String safeRemoveEmojis(String input) {
    String cleaned = EmojiRemover.removeEmojisAdvanced(input);
    return cleaned == null ? "" : cleaned;
}

private static String safeSubstring(String input, int maxLen) {
    if (input == null) return "";
    return input.substring(0, Math.min(maxLen, input.length()));
}