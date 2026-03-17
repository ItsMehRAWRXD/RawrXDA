/**
 * OpenAI Models Management Module
 * Provides integration with OpenAI's available models list from December 2025
 * Supports dynamic model selection and filtering for the Win32 agentic IDE
 */

export interface OpenAIModel {
    id: string;
    object: string;
    created: number;
    owned_by: string;
}

export class OpenAIModelsManager {
    /**
     * Complete list of available models from OpenAI API (as of December 2025)
     */
    private static readonly OPENAI_MODELS: OpenAIModel[] = [
        // Vision and Multimodal Models
        { id: "gpt-5.2", object: "model", created: 1765313051, owned_by: "system" },
        { id: "gpt-5.2-pro", object: "model", created: 1765343983, owned_by: "system" },
        { id: "gpt-5.1", object: "model", created: 1762800673, owned_by: "system" },
        { id: "gpt-5", object: "model", created: 1754425777, owned_by: "system" },
        { id: "gpt-4o", object: "model", created: 1715367049, owned_by: "system" },
        { id: "gpt-4o-2024-11-20", object: "model", created: 1739331543, owned_by: "system" },
        
        // Audio and Transcription
        { id: "gpt-audio", object: "model", created: 1756339249, owned_by: "system" },
        { id: "gpt-audio-mini", object: "model", created: 1759512027, owned_by: "system" },
        { id: "gpt-4o-mini-transcribe", object: "model", created: 1742068596, owned_by: "system" },
        { id: "gpt-4o-transcribe-diarize", object: "model", created: 1750798887, owned_by: "system" },
        
        // Text-to-Speech
        { id: "tts-1", object: "model", created: 1681940951, owned_by: "openai-internal" },
        { id: "tts-1-hd", object: "model", created: 1699046015, owned_by: "system" },
        
        // Speech Recognition
        { id: "whisper-1", object: "model", created: 1677532384, owned_by: "openai-internal" },
        
        // Embeddings
        { id: "text-embedding-3-large", object: "model", created: 1705953180, owned_by: "system" },
        { id: "text-embedding-3-small", object: "model", created: 1705948997, owned_by: "system" },
        { id: "text-embedding-ada-002", object: "model", created: 1671217299, owned_by: "openai-internal" },
        
        // Image Generation
        { id: "dall-e-3", object: "model", created: 1698785189, owned_by: "system" },
        { id: "dall-e-2", object: "model", created: 1698798177, owned_by: "system" },
        
        // Video Generation
        { id: "sora-2", object: "model", created: 1759708615, owned_by: "system" },
        { id: "sora-2-pro", object: "model", created: 1759708663, owned_by: "system" },
        
        // Legacy and Specialized Models
        { id: "gpt-3.5-turbo", object: "model", created: 1677610602, owned_by: "openai" },
        { id: "gpt-3.5-turbo-1106", object: "model", created: 1698959748, owned_by: "system" },
        { id: "davinci-002", object: "model", created: 1692634301, owned_by: "system" },
        { id: "babbage-002", object: "model", created: 1692634615, owned_by: "system" },
        
        // Reasoning Models (O-series)
        { id: "o1", object: "model", created: 1734375816, owned_by: "system" },
        { id: "o3-mini", object: "model", created: 1737146383, owned_by: "system" },
        { id: "o3", object: "model", created: 1744225308, owned_by: "system" },
        
        // Moderation
        { id: "omni-moderation-latest", object: "model", created: 1731689265, owned_by: "system" },
    ];

    /**
     * Get recommended models for different use cases
     */
    public static getRecommendedModels(): OpenAIModel[] {
        return [
            this.OPENAI_MODELS.find(m => m.id === "gpt-5.2")!,
            this.OPENAI_MODELS.find(m => m.id === "gpt-5.2-pro")!,
            this.OPENAI_MODELS.find(m => m.id === "gpt-4o")!,
            this.OPENAI_MODELS.find(m => m.id === "gpt-3.5-turbo")!,
        ];
    }

    /**
     * Get all available chat/completion models
     */
    public static getChatModels(): OpenAIModel[] {
        const chatModelIds = [
            "gpt-5.2", "gpt-5.2-pro", "gpt-5.1", "gpt-5",
            "gpt-4o", "gpt-4o-2024-11-20",
            "gpt-3.5-turbo", "gpt-3.5-turbo-1106",
            "o1", "o3-mini", "o3",
            "davinci-002", "babbage-002"
        ];
        return this.OPENAI_MODELS.filter(m => chatModelIds.includes(m.id));
    }

    /**
     * Get all available audio models
     */
    public static getAudioModels(): OpenAIModel[] {
        const audioModelIds = [
            "gpt-audio", "gpt-audio-mini",
            "gpt-4o-mini-transcribe", "gpt-4o-transcribe-diarize",
            "tts-1", "tts-1-hd",
            "whisper-1"
        ];
        return this.OPENAI_MODELS.filter(m => audioModelIds.includes(m.id));
    }

    /**
     * Get all available image/vision models
     */
    public static getVisionModels(): OpenAIModel[] {
        const visionModelIds = [
            "gpt-5.2", "gpt-5.2-pro", "gpt-5.1", "gpt-5",
            "gpt-4o", "gpt-4o-2024-11-20",
            "dall-e-3", "dall-e-2"
        ];
        return this.OPENAI_MODELS.filter(m => visionModelIds.includes(m.id));
    }

    /**
     * Get all available embedding models
     */
    public static getEmbeddingModels(): OpenAIModel[] {
        const embeddingModelIds = [
            "text-embedding-3-large", "text-embedding-3-small", "text-embedding-ada-002"
        ];
        return this.OPENAI_MODELS.filter(m => embeddingModelIds.includes(m.id));
    }

    /**
     * Get model by ID
     */
    public static getModelById(id: string): OpenAIModel | undefined {
        return this.OPENAI_MODELS.find(m => m.id === id);
    }

    /**
     * Get all models
     */
    public static getAllModels(): OpenAIModel[] {
        return [...this.OPENAI_MODELS];
    }

    /**
     * Search models by keyword
     */
    public static searchModels(keyword: string): OpenAIModel[] {
        const lowerKeyword = keyword.toLowerCase();
        return this.OPENAI_MODELS.filter(m => 
            m.id.toLowerCase().includes(lowerKeyword) ||
            m.owned_by.toLowerCase().includes(lowerKeyword)
        );
    }

    /**
     * Get model capabilities
     */
    public static getModelCapabilities(modelId: string): string[] {
        const capabilities: { [key: string]: string[] } = {
            "gpt-5.2": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-5.2-pro": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-5.1": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-5": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-4o": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-4o-2024-11-20": ["chat", "vision", "function-calling", "json-mode"],
            "gpt-3.5-turbo": ["chat", "function-calling", "json-mode"],
            "gpt-3.5-turbo-1106": ["chat", "function-calling", "json-mode"],
            "gpt-audio": ["chat", "audio", "speech-recognition"],
            "gpt-audio-mini": ["chat", "audio", "speech-recognition"],
            "tts-1": ["text-to-speech", "audio"],
            "tts-1-hd": ["text-to-speech", "audio"],
            "whisper-1": ["speech-recognition", "audio"],
            "dall-e-3": ["image-generation"],
            "dall-e-2": ["image-generation"],
            "sora-2": ["video-generation"],
            "sora-2-pro": ["video-generation"],
            "text-embedding-3-large": ["embeddings"],
            "text-embedding-3-small": ["embeddings"],
            "text-embedding-ada-002": ["embeddings"],
            "o1": ["chat", "reasoning"],
            "o3-mini": ["chat", "reasoning"],
            "o3": ["chat", "reasoning"],
            "omni-moderation-latest": ["moderation"]
        };
        return capabilities[modelId] || ["unknown"];
    }

    /**
     * Get recommended max tokens for a model
     */
    public static getRecommendedMaxTokens(modelId: string): number {
        if (modelId.includes("gpt-5") || modelId.includes("gpt-4")) {
            return 4096;
        }
        if (modelId.includes("gpt-3.5")) {
            return 2048;
        }
        return 1024;
    }

    /**
     * Format model for display in UI
     */
    public static formatModelForDisplay(model: OpenAIModel): string {
        const capabilities = this.getModelCapabilities(model.id);
        return `${model.id} (${capabilities.join(", ")})`;
    }
}
