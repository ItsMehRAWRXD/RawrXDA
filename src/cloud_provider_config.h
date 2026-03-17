/**
 * @file cloud_provider_config.h
 * @brief Cloud provider configurations for RawrXD IDE
 * 
 * Provides configuration schemas and constants for:
 * - AWS SageMaker / Bedrock
 * - Azure Cognitive Services / OpenAI
 * - Google Vertex AI
 * - HuggingFace Inference API
 */

#ifndef CLOUD_PROVIDER_CONFIG_H
#define CLOUD_PROVIDER_CONFIG_H

#include <string>
#include <vector>
#include <unordered_map>

namespace RawrXD {
namespace Cloud {

//=============================================================================
// AWS Configuration
//=============================================================================
struct AWSConfig {
    std::string region = "us-west-2";
    std::string accessKeyId;      // AWS_ACCESS_KEY_ID
    std::string secretAccessKey;  // AWS_SECRET_ACCESS_KEY
    std::string sessionToken;     // Optional: AWS_SESSION_TOKEN
    
    // Service endpoints
    std::string sagemakerEndpoint;  // e.g., "sagemaker.us-west-2.amazonaws.com"
    std::string bedrockEndpoint;    // e.g., "bedrock-runtime.us-west-2.amazonaws.com"
    
    // Available models
    std::vector<std::string> availableModels = {
        "anthropic.claude-3-sonnet-20240229-v1:0",
        "anthropic.claude-3-opus-20240229-v1:0",
        "meta.llama2-70b-chat-v1",
        "mistral.mistral-7b-instruct-v0:2"
    };
    
    // Pricing (USD per 1000 tokens)
    std::unordered_map<std::string, double> modelPricing = {
        {"anthropic.claude-3-sonnet", 0.003},
        {"anthropic.claude-3-opus", 0.015},
        {"meta.llama2", 0.001},
        {"mistral.mistral", 0.00015}
    };
};

//=============================================================================
// Azure Configuration
//=============================================================================
struct AzureConfig {
    std::string region = "eastus";
    std::string subscriptionId;
    std::string resourceGroup;
    std::string apiKey;               // Azure API key or auth token
    std::string apiVersion = "2024-02-15-preview";
    
    // Service endpoints
    std::string cognitiveServicesEndpoint;
    std::string openaiEndpoint;       // e.g., "https://myresource.openai.azure.com/"
    
    // Available models and their deployments
    std::vector<std::string> availableModels = {
        "gpt-4-turbo",
        "gpt-4",
        "gpt-35-turbo",
        "text-davinci-003"
    };
    
    // Pricing (USD per 1000 tokens)
    std::unordered_map<std::string, double> modelPricing = {
        {"gpt-4-turbo", 0.01},
        {"gpt-4", 0.03},
        {"gpt-35-turbo", 0.001},
        {"text-davinci-003", 0.02}
    };
};

//=============================================================================
// Google Cloud Configuration
//=============================================================================
struct GCPConfig {
    std::string projectId;
    std::string region = "us-central1";
    std::string credentialsJson;      // GCP service account JSON
    std::string accessToken;          // OAuth2 access token
    
    // Service endpoints
    std::string vertexAiEndpoint;     // e.g., "us-central1-aiplatform.googleapis.com"
    std::string locationId = "us-central1";
    
    // Available models
    std::vector<std::string> availableModels = {
        "gemini-pro",
        "gemini-pro-vision",
        "text-bison@001",
        "text-unicorn@001"
    };
    
    // Pricing (USD per 1000 tokens)
    std::unordered_map<std::string, double> modelPricing = {
        {"gemini-pro", 0.001},
        {"gemini-pro-vision", 0.0025},
        {"text-bison", 0.0001},
        {"text-unicorn", 0.0003}
    };
};

//=============================================================================
// HuggingFace Configuration
//=============================================================================
struct HuggingFaceConfig {
    std::string apiToken;             // Hugging Face API token
    std::string endpointUrl = "https://api-inference.huggingface.co";
    
    // Available models
    std::vector<std::string> availableModels = {
        "meta-llama/Llama-2-70b-chat-hf",
        "meta-llama/Llama-2-13b-chat-hf",
        "mistralai/Mistral-7B-Instruct-v0.1",
        "bigcode/starcoder"
    };
    
    // Cost (free API calls, optional paid tiers)
    std::unordered_map<std::string, double> modelPricing = {
        // Free tier (rate-limited)
        {"meta-llama/Llama-2-70b-chat-hf", 0.0},
        {"meta-llama/Llama-2-13b-chat-hf", 0.0},
        {"mistralai/Mistral-7B-Instruct-v0.1", 0.0}
    };
};

//=============================================================================
// Ollama Configuration (Local)
//=============================================================================
struct OllamaConfig {
    std::string endpoint = "http://localhost:11434";
    int port = 11434;
    
    // Available models (installed locally)
    std::vector<std::string> availableModels = {
        "mistral",
        "neural-chat",
        "starling-lm",
        "orca-mini"
    };
    
    // Pricing (free, local execution)
    std::unordered_map<std::string, double> modelPricing = {
        {"mistral", 0.0},
        {"neural-chat", 0.0},
        {"starling-lm", 0.0},
        {"orca-mini", 0.0}
    };
};

//=============================================================================
// Anthropic Configuration (Claude API)
//=============================================================================
struct AnthropicConfig {
    std::string apiKey;
    std::string apiVersion = "2024-01-15";
    std::string endpoint = "https://api.anthropic.com/v1";
    
    // Available models
    std::vector<std::string> availableModels = {
        "claude-3-opus-20240229",
        "claude-3-sonnet-20240229",
        "claude-3-haiku-20240307",
        "claude-2.1",
        "claude-2"
    };
    
    // Pricing (USD per 1000 tokens) - input/output
    struct TokenPricing {
        double inputCost;
        double outputCost;
    };
    
    std::unordered_map<std::string, TokenPricing> modelPricing = {
        {"claude-3-opus", {0.015, 0.075}},
        {"claude-3-sonnet", {0.003, 0.015}},
        {"claude-3-haiku", {0.00025, 0.00125}},
        {"claude-2.1", {0.008, 0.024}},
        {"claude-2", {0.008, 0.024}}
    };
};

//=============================================================================
// Configuration Manager
//=============================================================================
class CloudProviderConfigManager {
public:
    // AWS configuration
    static AWSConfig& getAWSConfig() {
        static AWSConfig config;
        return config;
    }
    
    // Azure configuration
    static AzureConfig& getAzureConfig() {
        static AzureConfig config;
        return config;
    }
    
    // GCP configuration
    static GCPConfig& getGCPConfig() {
        static GCPConfig config;
        return config;
    }
    
    // HuggingFace configuration
    static HuggingFaceConfig& getHuggingFaceConfig() {
        static HuggingFaceConfig config;
        return config;
    }
    
    // Ollama configuration
    static OllamaConfig& getOllamaConfig() {
        static OllamaConfig config;
        return config;
    }
    
    // Anthropic configuration
    static AnthropicConfig& getAnthropicConfig() {
        static AnthropicConfig config;
        return config;
    }
    
    // Load configuration from environment variables
    static void loadFromEnvironment() {
        // AWS
        const char* awsAccessKey = getenv("AWS_ACCESS_KEY_ID");
        const char* awsSecretKey = getenv("AWS_SECRET_ACCESS_KEY");
        if (awsAccessKey && awsSecretKey) {
            getAWSConfig().accessKeyId = awsAccessKey;
            getAWSConfig().secretAccessKey = awsSecretKey;
        }
        
        // Azure
        const char* azureKey = getenv("AZURE_API_KEY");
        if (azureKey) {
            getAzureConfig().apiKey = azureKey;
        }
        
        // GCP
        const char* gcpProject = getenv("GCP_PROJECT_ID");
        if (gcpProject) {
            getGCPConfig().projectId = gcpProject;
        }
        
        // HuggingFace
        const char* hfToken = getenv("HUGGINGFACE_HUB_TOKEN");
        if (hfToken) {
            getHuggingFaceConfig().apiToken = hfToken;
        }
        
        // Anthropic
        const char* anthropicKey = getenv("ANTHROPIC_API_KEY");
        if (anthropicKey) {
            getAnthropicConfig().apiKey = anthropicKey;
        }
    }
    
    // Load configuration from file
    static bool loadFromFile(const std::string& configPath);
};

} // namespace Cloud
} // namespace RawrXD

#endif // CLOUD_PROVIDER_CONFIG_H
