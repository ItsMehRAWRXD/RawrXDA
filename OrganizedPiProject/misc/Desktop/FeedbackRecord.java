// FeedbackRecord.java - Data model for human feedback collection
package com.aicli.feedback;

import java.time.Instant;
import java.util.Map;
import java.util.HashMap;

/**
 * Comprehensive data structure to capture human feedback on AI interactions.
 * This forms the foundation for continuous learning and model improvement.
 */
public class FeedbackRecord {
    public enum InteractionType {
        ACCEPT,      // User accepted the AI response without changes
        REJECT,      // User rejected the AI response entirely
        EDIT,        // User modified the AI response
        RATE         // User provided a rating/score
    }

    private final String userId;
    private final String sessionId;
    private final String conversationId;
    private final String originalPrompt;
    private final String originalResponse;
    private final InteractionType interaction;
    private final Instant timestamp;
    private final Map<String, Object> metadata;
    
    // For EDIT interaction - stores the user's modifications
    private String editedResponse;
    
    // For RATE interaction - stores numerical rating
    private Double rating;
    
    // Additional context for feedback
    private String context;
    private String reasoning;

    public FeedbackRecord(String userId, String sessionId, String conversationId, 
                         String originalPrompt, String originalResponse, 
                         InteractionType interaction, Map<String, Object> metadata) {
        this.userId = userId;
        this.sessionId = sessionId;
        this.conversationId = conversationId;
        this.originalPrompt = originalPrompt;
        this.originalResponse = originalResponse;
        this.interaction = interaction;
        this.timestamp = Instant.now();
        this.metadata = metadata != null ? new HashMap<>(metadata) : new HashMap<>();
    }

    // Builder pattern for complex feedback records
    public static class Builder {
        private String userId;
        private String sessionId;
        private String conversationId;
        private String originalPrompt;
        private String originalResponse;
        private InteractionType interaction;
        private Map<String, Object> metadata = new HashMap<>();
        private String editedResponse;
        private Double rating;
        private String context;
        private String reasoning;

        public Builder userId(String userId) {
            this.userId = userId;
            return this;
        }

        public Builder sessionId(String sessionId) {
            this.sessionId = sessionId;
            return this;
        }

        public Builder conversationId(String conversationId) {
            this.conversationId = conversationId;
            return this;
        }

        public Builder originalPrompt(String originalPrompt) {
            this.originalPrompt = originalPrompt;
            return this;
        }

        public Builder originalResponse(String originalResponse) {
            this.originalResponse = originalResponse;
            return this;
        }

        public Builder interaction(InteractionType interaction) {
            this.interaction = interaction;
            return this;
        }

        public Builder metadata(Map<String, Object> metadata) {
            this.metadata = metadata;
            return this;
        }

        public Builder addMetadata(String key, Object value) {
            this.metadata.put(key, value);
            return this;
        }

        public Builder editedResponse(String editedResponse) {
            this.editedResponse = editedResponse;
            return this;
        }

        public Builder rating(Double rating) {
            this.rating = rating;
            return this;
        }

        public Builder context(String context) {
            this.context = context;
            return this;
        }

        public Builder reasoning(String reasoning) {
            this.reasoning = reasoning;
            return this;
        }

        public FeedbackRecord build() {
            FeedbackRecord record = new FeedbackRecord(userId, sessionId, conversationId, 
                    originalPrompt, originalResponse, interaction, metadata);
            record.editedResponse = editedResponse;
            record.rating = rating;
            record.context = context;
            record.reasoning = reasoning;
            return record;
        }
    }

    // Getters
    public String getUserId() { return userId; }
    public String getSessionId() { return sessionId; }
    public String getConversationId() { return conversationId; }
    public String getOriginalPrompt() { return originalPrompt; }
    public String getOriginalResponse() { return originalResponse; }
    public InteractionType getInteraction() { return interaction; }
    public Instant getTimestamp() { return timestamp; }
    public Map<String, Object> getMetadata() { return new HashMap<>(metadata); }
    public String getEditedResponse() { return editedResponse; }
    public Double getRating() { return rating; }
    public String getContext() { return context; }
    public String getReasoning() { return reasoning; }

    // Setters for mutable fields
    public void setEditedResponse(String editedResponse) {
        this.editedResponse = editedResponse;
    }

    public void setRating(Double rating) {
        this.rating = rating;
    }

    public void setContext(String context) {
        this.context = context;
    }

    public void setReasoning(String reasoning) {
        this.reasoning = reasoning;
    }

    /**
     * Creates a training example from this feedback record.
     * For RLHF, we need to convert feedback into preference pairs.
     */
    public TrainingExample toTrainingExample() {
        String preferredResponse;
        String rejectedResponse;
        
        switch (interaction) {
            case ACCEPT:
                preferredResponse = originalResponse;
                rejectedResponse = null; // No rejection to compare against
                break;
            case REJECT:
                preferredResponse = null; // No preferred response
                rejectedResponse = originalResponse;
                break;
            case EDIT:
                preferredResponse = editedResponse;
                rejectedResponse = originalResponse;
                break;
            case RATE:
                // For ratings, we might need to compare against a baseline
                preferredResponse = rating > 3.0 ? originalResponse : null;
                rejectedResponse = rating <= 3.0 ? originalResponse : null;
                break;
            default:
                preferredResponse = null;
                rejectedResponse = null;
        }
        
        return new TrainingExample(originalPrompt, preferredResponse, rejectedResponse, rating);
    }

    /**
     * Represents a training example for fine-tuning
     */
    public static class TrainingExample {
        private final String prompt;
        private final String preferredResponse;
        private final String rejectedResponse;
        private final Double rating;

        public TrainingExample(String prompt, String preferredResponse, String rejectedResponse, Double rating) {
            this.prompt = prompt;
            this.preferredResponse = preferredResponse;
            this.rejectedResponse = rejectedResponse;
            this.rating = rating;
        }

        public String getPrompt() { return prompt; }
        public String getPreferredResponse() { return preferredResponse; }
        public String getRejectedResponse() { return rejectedResponse; }
        public Double getRating() { return rating; }
        
        public boolean hasPreference() {
            return preferredResponse != null || rejectedResponse != null;
        }
        
        public boolean hasRating() {
            return rating != null;
        }
    }

    @Override
    public String toString() {
        return String.format("FeedbackRecord{userId='%s', interaction=%s, timestamp=%s}", 
                userId, interaction, timestamp);
    }
}
