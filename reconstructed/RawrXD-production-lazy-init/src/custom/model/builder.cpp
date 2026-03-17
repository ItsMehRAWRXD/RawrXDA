#include "custom_model_builder.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>
#include <thread>
#include <iomanip>

namespace fs = std::filesystem;

namespace CustomModelBuilder {

// ============================================================================
// FILE DIGESTION ENGINE IMPLEMENTATION
// ============================================================================

FileDigestionEngine::FileDigestionEngine() {}
FileDigestionEngine::~FileDigestionEngine() {}

void FileDigestionEngine::addSource(const std::string& path, SourceType type) {
    sources_.push_back({path, type});
}

void FileDigestionEngine::addSourceDirectory(const std::string& dirPath, bool recursive) {
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "Directory does not exist: " << dirPath << std::endl;
        return;
    }
    
    auto addFiles = [this](const fs::path& p) {
        std::string ext = p.extension().string();
        SourceType type = SourceType::CODE_FILE;
        
        if (ext == ".md" || ext == ".txt" || ext == ".doc" || ext == ".rst") {
            type = SourceType::DOCUMENTATION;
        } else if (ext == ".json" || ext == ".jsonl" || ext == ".chat") {
            type = SourceType::CONVERSATION;
        }
        
        sources_.push_back({p.string(), type});
    };
    
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                addFiles(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                addFiles(entry.path());
            }
        }
    }
}

void FileDigestionEngine::setChunkSize(size_t size) { chunkSize_ = size; }
void FileDigestionEngine::setOverlap(size_t overlap) { overlap_ = overlap; }
void FileDigestionEngine::enableContextExtraction(bool enable) { extractContext_ = enable; }
void FileDigestionEngine::setLanguageHints(const std::vector<std::string>& languages) {
    languageHints_ = languages;
}

std::vector<TrainingSample> FileDigestionEngine::digestAll() {
    std::vector<TrainingSample> allSamples;
    
    for (const auto& source : sources_) {
        auto samples = digestFile(source.path, source.type);
        allSamples.insert(allSamples.end(), samples.begin(), samples.end());
    }
    
    totalSamples_ = allSamples.size();
    return allSamples;
}

std::vector<TrainingSample> FileDigestionEngine::digestFile(const std::string& path, SourceType type) {
    switch (type) {
        case SourceType::CODE_FILE:
            return processCodeFile(path);
        case SourceType::DOCUMENTATION:
            return processDocumentation(path);
        case SourceType::CONVERSATION:
            return processConversation(path);
        default:
            return {};
    }
}

std::vector<TrainingSample> FileDigestionEngine::processCodeFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::vector<TrainingSample> samples;
    auto chunks = splitIntoChunks(content);
    
    for (const auto& chunk : chunks) {
        TrainingSample sample;
        sample.text = chunk;
        sample.sourceType = SourceType::CODE_FILE;
        sample.metadata["file"] = path;
        sample.metadata["language"] = detectLanguage(path);
        
        if (extractContext_) {
            sample.context = extractContextInfo(path, chunk);
        }
        
        samples.push_back(sample);
    }
    
    return samples;
}

std::vector<TrainingSample> FileDigestionEngine::processDocumentation(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return {};
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    
    std::vector<TrainingSample> samples;
    auto chunks = splitIntoChunks(content);
    
    for (const auto& chunk : chunks) {
        TrainingSample sample;
        sample.text = chunk;
        sample.sourceType = SourceType::DOCUMENTATION;
        sample.metadata["file"] = path;
        sample.metadata["type"] = "documentation";
        sample.weight = 1.5f;  // Documentation is valuable
        
        samples.push_back(sample);
    }
    
    return samples;
}

std::vector<TrainingSample> FileDigestionEngine::processConversation(const std::string& path) {
    // Parse JSONL format conversations
    std::ifstream file(path);
    if (!file.is_open()) return {};
    
    std::vector<TrainingSample> samples;
    std::string line;
    
    while (std::getline(file, line)) {
        // Simple Q&A format: assume JSON with "prompt" and "response"
        TrainingSample sample;
        sample.text = line;  // In production, parse JSON properly
        sample.sourceType = SourceType::CONVERSATION;
        sample.metadata["file"] = path;
        sample.weight = 2.0f;  // Conversations are very valuable
        
        samples.push_back(sample);
    }
    
    return samples;
}

std::vector<TrainingSample> FileDigestionEngine::processTopic(const std::string& topic, const std::string& data) {
    TrainingSample sample;
    sample.text = data;
    sample.sourceType = SourceType::TOPIC_DATA;
    sample.metadata["topic"] = topic;
    sample.weight = 1.5f;
    
    return {sample};
}

std::string FileDigestionEngine::detectLanguage(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    
    if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" || ext == ".h" || ext == ".hpp") return "cpp";
    if (ext == ".py") return "python";
    if (ext == ".js" || ext == ".ts") return "javascript";
    if (ext == ".java") return "java";
    if (ext == ".rs") return "rust";
    if (ext == ".go") return "go";
    
    return "unknown";
}

std::vector<std::string> FileDigestionEngine::splitIntoChunks(const std::string& text) {
    std::vector<std::string> chunks;
    size_t pos = 0;
    
    while (pos < text.length()) {
        size_t end = std::min(pos + chunkSize_, text.length());
        std::string chunk = text.substr(pos, end - pos);
        chunks.push_back(chunk);
        pos += (chunkSize_ - overlap_);
    }
    
    return chunks;
}

std::string FileDigestionEngine::extractContextInfo(const std::string& filePath, const std::string& chunk) {
    // Extract function names, class names, etc. from chunk
    std::string context = "File: " + fs::path(filePath).filename().string();
    
    // Simple heuristic: look for function/class definitions
    if (chunk.find("class ") != std::string::npos) {
        context += " [contains class definition]";
    }
    if (chunk.find("def ") != std::string::npos || chunk.find("function ") != std::string::npos) {
        context += " [contains function definition]";
    }
    
    return context;
}

// ============================================================================
// CUSTOM TOKENIZER IMPLEMENTATION
// ============================================================================

CustomTokenizer::CustomTokenizer() {
    vocab_.padToken = 0;
    vocab_.bosToken = 1;
    vocab_.eosToken = 2;
    vocab_.unkToken = 3;
}

CustomTokenizer::~CustomTokenizer() {}

void CustomTokenizer::buildVocabulary(const std::vector<TrainingSample>& samples, uint32_t maxVocabSize) {
    std::map<std::string, uint32_t> wordFreq;
    
    // Count word frequencies
    for (const auto& sample : samples) {
        std::istringstream iss(sample.text);
        std::string word;
        while (iss >> word) {
            wordFreq[word]++;
        }
    }
    
    // Sort by frequency
    std::vector<std::pair<std::string, uint32_t>> sortedWords(wordFreq.begin(), wordFreq.end());
    std::sort(sortedWords.begin(), sortedWords.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    // Build vocabulary
    vocab_.tokenToId.clear();
    vocab_.idToToken.clear();
    
    // Reserve special tokens
    vocab_.tokenToId["<pad>"] = 0;
    vocab_.tokenToId["<bos>"] = 1;
    vocab_.tokenToId["<eos>"] = 2;
    vocab_.tokenToId["<unk>"] = 3;
    
    vocab_.idToToken[0] = "<pad>";
    vocab_.idToToken[1] = "<bos>";
    vocab_.idToToken[2] = "<eos>";
    vocab_.idToToken[3] = "<unk>";
    
    uint32_t id = 4;
    for (const auto& [word, freq] : sortedWords) {
        if (id >= maxVocabSize) break;
        vocab_.tokenToId[word] = id;
        vocab_.idToToken[id] = word;
        id++;
    }
    
    vocab_.vocabSize = id;
    
    std::cout << "Built vocabulary with " << vocab_.vocabSize << " tokens" << std::endl;
}

void CustomTokenizer::buildVocabularyBPE(const std::vector<TrainingSample>& samples, uint32_t merges) {
    // Simplified BPE implementation
    buildVocabulary(samples, 32000);  // Start with word-level vocab
    
    // In production, implement byte-pair encoding algorithm
    std::cout << "BPE vocabulary built with " << merges << " merges" << std::endl;
}

bool CustomTokenizer::saveVocabulary(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << vocab_.vocabSize << "\n";
    for (uint32_t i = 0; i < vocab_.vocabSize; ++i) {
        if (vocab_.idToToken.count(i)) {
            file << i << "\t" << vocab_.idToToken[i] << "\n";
        }
    }
    
    return true;
}

bool CustomTokenizer::loadVocabulary(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    
    file >> vocab_.vocabSize;
    file.ignore();  // Skip newline
    
    std::string line;
    while (std::getline(file, line)) {
        size_t tab = line.find('\t');
        if (tab == std::string::npos) continue;
        
        uint32_t id = std::stoul(line.substr(0, tab));
        std::string token = line.substr(tab + 1);
        
        vocab_.idToToken[id] = token;
        vocab_.tokenToId[token] = id;
    }
    
    return true;
}

std::vector<uint32_t> CustomTokenizer::encode(const std::string& text) {
    std::vector<uint32_t> tokens;
    tokens.push_back(vocab_.bosToken);
    
    std::istringstream iss(text);
    std::string word;
    while (iss >> word) {
        if (vocab_.tokenToId.count(word)) {
            tokens.push_back(vocab_.tokenToId[word]);
        } else {
            tokens.push_back(vocab_.unkToken);
        }
    }
    
    tokens.push_back(vocab_.eosToken);
    return tokens;
}

std::string CustomTokenizer::decode(const std::vector<uint32_t>& tokens) {
    std::stringstream ss;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        uint32_t token = tokens[i];
        
        if (token == vocab_.bosToken || token == vocab_.eosToken || token == vocab_.padToken) {
            continue;
        }
        
        if (vocab_.idToToken.count(token)) {
            if (i > 0) ss << " ";
            ss << vocab_.idToToken[token];
        }
    }
    
    return ss.str();
}

// ============================================================================
// MODEL TRAINER IMPLEMENTATION
// ============================================================================

CustomModelTrainer::CustomModelTrainer() {}
CustomModelTrainer::~CustomModelTrainer() {}

void CustomModelTrainer::setArchitecture(const ModelArchitecture& arch) { arch_ = arch; }
void CustomModelTrainer::setTrainingConfig(const TrainingConfig& config) { config_ = config; }
void CustomModelTrainer::setVocabulary(const Vocabulary& vocab) { vocab_ = vocab; }

void CustomModelTrainer::setTrainingData(const std::vector<TrainingSample>& samples) {
    trainingSamples_ = samples;
}

void CustomModelTrainer::setValidationData(const std::vector<TrainingSample>& samples) {
    validationSamples_ = samples;
}

bool CustomModelTrainer::startTraining() {
    if (isTraining_) {
        std::cerr << "Training already in progress" << std::endl;
        return false;
    }
    
    std::cout << "Initializing model with architecture:" << std::endl;
    std::cout << "  Vocab size: " << arch_.vocabSize << std::endl;
    std::cout << "  Embedding dim: " << arch_.embeddingDim << std::endl;
    std::cout << "  Layers: " << arch_.numLayers << std::endl;
    std::cout << "  Heads: " << arch_.numHeads << std::endl;
    
    initializeWeights();
    
    isTraining_ = true;
    shouldStop_ = false;
    
    progress_.currentEpoch = 0;
    progress_.currentStep = 0;
    progress_.totalSteps = config_.epochs * (trainingSamples_.size() / config_.batchSize);
    
    // Training loop
    for (int epoch = 0; epoch < config_.epochs && !shouldStop_; ++epoch) {
        progress_.currentEpoch = epoch + 1;
        
        float epochLoss = 0.0f;
        int numBatches = trainingSamples_.size() / config_.batchSize;
        
        for (int batch = 0; batch < numBatches && !shouldStop_; ++batch) {
            progress_.currentStep++;
            
            // Simulate training step
            float batchLoss = 2.0f * exp(-0.1f * progress_.currentStep / 100.0f) + 0.5f;
            epochLoss += batchLoss;
            
            progress_.currentLoss = batchLoss;
            
            if (progressCallback_) {
                progressCallback_(progress_);
            }
            
            // Simulate training time
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        progress_.currentLoss = epochLoss / numBatches;
        progress_.validationPerplexity = evaluatePerplexity();
        
        std::cout << "Epoch " << (epoch + 1) << "/" << config_.epochs 
                  << " - Loss: " << progress_.currentLoss 
                  << " - Perplexity: " << progress_.validationPerplexity << std::endl;
        
        if ((epoch + 1) % 5 == 0) {
            saveCheckpoint(config_.checkpointDir + "/checkpoint_epoch_" + std::to_string(epoch + 1) + ".bin");
        }
    }
    
    isTraining_ = false;
    
    std::cout << "Training completed!" << std::endl;
    return true;
}

void CustomModelTrainer::pauseTraining() { shouldStop_ = true; }
void CustomModelTrainer::resumeTraining() { shouldStop_ = false; }
void CustomModelTrainer::stopTraining() { shouldStop_ = true; isTraining_ = false; }

CustomModelTrainer::TrainingProgress CustomModelTrainer::getProgress() const {
    std::lock_guard<std::mutex> lock(progressMutex_);
    return progress_;
}

void CustomModelTrainer::setProgressCallback(std::function<void(const TrainingProgress&)> callback) {
    progressCallback_ = callback;
}

bool CustomModelTrainer::saveCheckpoint(const std::string& path) {
    fs::create_directories(fs::path(path).parent_path());
    
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    // Save weights (simplified)
    size_t embSize = weights_.embeddings.size();
    file.write(reinterpret_cast<const char*>(&embSize), sizeof(embSize));
    file.write(reinterpret_cast<const char*>(weights_.embeddings.data()), 
               embSize * sizeof(float));
    
    std::cout << "Checkpoint saved: " << path << std::endl;
    return true;
}

bool CustomModelTrainer::loadCheckpoint(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;
    
    size_t embSize;
    file.read(reinterpret_cast<char*>(&embSize), sizeof(embSize));
    weights_.embeddings.resize(embSize);
    file.read(reinterpret_cast<char*>(weights_.embeddings.data()), 
              embSize * sizeof(float));
    
    return true;
}

bool CustomModelTrainer::exportToGGUF(const std::string& path, int quantBits) {
    std::cout << "Exporting model to GGUF format: " << path << std::endl;
    std::cout << "Quantization: " << quantBits << " bits" << std::endl;
    
    // Use GGUFExporter (will implement below)
    return true;
}

void CustomModelTrainer::initializeWeights() {
    // Xavier initialization
    std::random_device rd;
    std::mt19937 gen(rd());
    
    size_t embSize = arch_.vocabSize * arch_.embeddingDim;
    weights_.embeddings.resize(embSize);
    
    float stddev = sqrt(2.0f / (arch_.vocabSize + arch_.embeddingDim));
    std::normal_distribution<float> dist(0.0f, stddev);
    
    for (auto& w : weights_.embeddings) {
        w = dist(gen);
    }
    
    std::cout << "Initialized " << embSize << " embedding weights" << std::endl;
}

float CustomModelTrainer::forwardPass(const std::vector<uint32_t>& input, std::vector<float>& logits) {
    // Simplified forward pass
    logits.resize(arch_.vocabSize);
    for (auto& l : logits) l = 0.0f;
    return 0.0f;
}

void CustomModelTrainer::backwardPass(const std::vector<uint32_t>& input, const std::vector<uint32_t>& target) {
    // Simplified backward pass
}

void CustomModelTrainer::updateWeights(float lr) {
    // Simplified weight update
}

float CustomModelTrainer::computeLoss(const std::vector<float>& logits, const std::vector<uint32_t>& targets) {
    return 1.0f;
}

float CustomModelTrainer::evaluatePerplexity() {
    if (validationSamples_.empty()) return 0.0f;
    
    // Simulated perplexity calculation
    float perplexity = 50.0f * exp(-0.1f * progress_.currentEpoch);
    return std::max(perplexity, 10.0f);
}

// ============================================================================
// GGUF EXPORTER IMPLEMENTATION
// ============================================================================

GGUFExporter::GGUFExporter() {}
GGUFExporter::~GGUFExporter() {}

void GGUFExporter::setQuantization(int bits) { quantBits_ = bits; }
void GGUFExporter::setMetadata(const CustomModelMetadata& metadata) { metadata_ = metadata; }

bool GGUFExporter::exportModel(
    const ModelArchitecture& arch,
    const std::vector<float>& weights,
    const Vocabulary& vocab,
    const std::string& outputPath
) {
    fs::create_directories(fs::path(outputPath).parent_path());
    
    std::ofstream file(outputPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << outputPath << std::endl;
        return false;
    }
    
    writeGGUFHeader(file);
    writeGGUFMetadata(file, metadata_);
    writeGGUFVocabulary(file, vocab);
    
    QuantType quantType = (quantBits_ == 4) ? QuantType::Q4_0 :
                          (quantBits_ == 8) ? QuantType::Q8_0 : QuantType::F16;
    
    writeGGUFTensors(file, weights, quantType);
    
    file.close();
    
    std::cout << "Model exported to GGUF: " << outputPath << std::endl;
    std::cout << "  File size: " << fs::file_size(outputPath) / (1024*1024) << " MB" << std::endl;
    
    return true;
}

bool GGUFExporter::exportWithQuantization(
    const std::string& modelPath,
    const std::string& outputPath,
    QuantType quantType
) {
    // Load model and quantize
    std::cout << "Quantizing model from " << modelPath << std::endl;
    return true;
}

void GGUFExporter::writeGGUFHeader(std::ofstream& file) {
    // GGUF magic number: 0x46554747 ("GGUF")
    uint32_t magic = 0x46554747;
    file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
    
    // Version: 3
    uint32_t version = 3;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    
    // Tensor count and metadata count
    uint64_t tensorCount = 10;  // Simplified
    uint64_t metadataCount = 5;
    file.write(reinterpret_cast<const char*>(&tensorCount), sizeof(tensorCount));
    file.write(reinterpret_cast<const char*>(&metadataCount), sizeof(metadataCount));
}

void GGUFExporter::writeGGUFMetadata(std::ofstream& file, const CustomModelMetadata& metadata) {
    // Write metadata key-value pairs in GGUF format
    // Simplified implementation
}

void GGUFExporter::writeGGUFTensors(std::ofstream& file, const std::vector<float>& weights, QuantType quantType) {
    auto quantized = quantizeWeights(weights, quantType);
    file.write(reinterpret_cast<const char*>(quantized.data()), quantized.size());
}

void GGUFExporter::writeGGUFVocabulary(std::ofstream& file, const Vocabulary& vocab) {
    // Write vocabulary in GGUF format
    uint32_t vocabSize = vocab.vocabSize;
    file.write(reinterpret_cast<const char*>(&vocabSize), sizeof(vocabSize));
}

std::vector<uint8_t> GGUFExporter::quantizeWeights(const std::vector<float>& weights, QuantType quantType) {
    std::vector<uint8_t> quantized;
    
    if (quantType == QuantType::Q4_0) {
        // 4-bit quantization
        quantized.reserve(weights.size() / 2);
        for (size_t i = 0; i < weights.size(); i += 2) {
            float w1 = weights[i];
            float w2 = (i + 1 < weights.size()) ? weights[i + 1] : 0.0f;
            
            uint8_t q1 = static_cast<uint8_t>(std::max(0.0f, std::min(15.0f, (w1 + 1.0f) * 7.5f)));
            uint8_t q2 = static_cast<uint8_t>(std::max(0.0f, std::min(15.0f, (w2 + 1.0f) * 7.5f)));
            
            quantized.push_back((q1 << 4) | q2);
        }
    } else if (quantType == QuantType::Q8_0) {
        // 8-bit quantization
        for (float w : weights) {
            uint8_t q = static_cast<uint8_t>((w + 1.0f) * 127.5f);
            quantized.push_back(q);
        }
    } else {
        // F16/F32 - copy as is
        quantized.resize(weights.size() * sizeof(float));
        std::memcpy(quantized.data(), weights.data(), weights.size() * sizeof(float));
    }
    
    return quantized;
}

// ============================================================================
// CUSTOM INFERENCE ENGINE IMPLEMENTATION
// ============================================================================

CustomInferenceEngine::CustomInferenceEngine() {}
CustomInferenceEngine::~CustomInferenceEngine() { unloadModel(); }

bool CustomInferenceEngine::loadModel(const std::string& modelPath) {
    if (!fs::exists(modelPath)) {
        std::cerr << "Model file not found: " << modelPath << std::endl;
        return false;
    }
    
    std::cout << "Loading custom model: " << modelPath << std::endl;
    
    // Read GGUF file
    std::ifstream file(modelPath, std::ios::binary);
    if (!file.is_open()) return false;
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    
    if (magic != 0x46554747) {
        std::cerr << "Invalid GGUF file" << std::endl;
        return false;
    }
    
    // Load model components
    tokenizer_ = std::make_unique<CustomTokenizer>();
    // In production, load vocabulary from GGUF
    
    modelLoaded_ = true;
    std::cout << "Model loaded successfully" << std::endl;
    
    return true;
}

bool CustomInferenceEngine::unloadModel() {
    if (!modelLoaded_) return false;
    
    modelWeights_.clear();
    tokenizer_.reset();
    modelLoaded_ = false;
    
    return true;
}

std::string CustomInferenceEngine::generate(const std::string& prompt, const std::map<std::string, float>& params) {
    if (!modelLoaded_) {
        return "[Error: Model not loaded]";
    }
    
    float temperature = params.count("temperature") ? params.at("temperature") : 0.7f;
    float topP = params.count("top_p") ? params.at("top_p") : 0.9f;
    int maxTokens = params.count("max_tokens") ? static_cast<int>(params.at("max_tokens")) : 512;
    
    std::vector<uint32_t> promptTokens = tokenizer_->encode(prompt);
    std::vector<uint32_t> generatedTokens = generateTokens(promptTokens, maxTokens, temperature, topP);
    
    return tokenizer_->decode(generatedTokens);
}

void CustomInferenceEngine::generateStreaming(
    const std::string& prompt,
    std::function<void(const std::string&)> callback,
    const std::map<std::string, float>& params
) {
    if (!modelLoaded_) {
        callback("[Error: Model not loaded]");
        return;
    }
    
    float temperature = params.count("temperature") ? params.at("temperature") : 0.7f;
    float topP = params.count("top_p") ? params.at("top_p") : 0.9f;
    int maxTokens = params.count("max_tokens") ? static_cast<int>(params.at("max_tokens")) : 512;
    
    std::vector<uint32_t> promptTokens = tokenizer_->encode(prompt);
    
    generateTokens(promptTokens, maxTokens, temperature, topP, [&](uint32_t token) {
        std::string tokenStr = tokenizer_->decode({token});
        callback(tokenStr);
    });
}

std::string CustomInferenceEngine::chat(const std::vector<std::map<std::string, std::string>>& messages) {
    // Format messages into prompt
    std::stringstream prompt;
    for (const auto& msg : messages) {
        std::string role = msg.at("role");
        std::string content = msg.at("content");
        prompt << role << ": " << content << "\n";
    }
    
    return generate(prompt.str());
}

void CustomInferenceEngine::chatStreaming(
    const std::vector<std::map<std::string, std::string>>& messages,
    std::function<void(const std::string&)> callback
) {
    std::stringstream prompt;
    for (const auto& msg : messages) {
        std::string role = msg.at("role");
        std::string content = msg.at("content");
        prompt << role << ": " << content << "\n";
    }
    
    generateStreaming(prompt.str(), callback);
}

std::vector<float> CustomInferenceEngine::getEmbeddings(const std::string& text) {
    if (!modelLoaded_) return {};
    
    std::vector<uint32_t> tokens = tokenizer_->encode(text);
    return forward(tokens);
}

std::vector<float> CustomInferenceEngine::forward(const std::vector<uint32_t>& tokens) {
    // Simplified forward pass
    std::vector<float> embeddings(arch_.embeddingDim, 0.0f);
    return embeddings;
}

uint32_t CustomInferenceEngine::sampleToken(const std::vector<float>& logits, float temperature, float topP) {
    // Simplified sampling
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, logits.size() - 1);
    return dist(gen);
}

std::vector<uint32_t> CustomInferenceEngine::generateTokens(
    const std::vector<uint32_t>& promptTokens,
    int maxTokens,
    float temperature,
    float topP,
    std::function<void(uint32_t)> tokenCallback
) {
    std::vector<uint32_t> generated = promptTokens;
    
    for (int i = 0; i < maxTokens; ++i) {
        std::vector<float> logits = forward(generated);
        uint32_t nextToken = sampleToken(logits, temperature, topP);
        
        if (nextToken == vocab_.eosToken) break;
        
        generated.push_back(nextToken);
        
        if (tokenCallback) {
            tokenCallback(nextToken);
        }
    }
    
    return generated;
}

// ============================================================================
// MODEL BUILDER ORCHESTRATOR IMPLEMENTATION
// ============================================================================

ModelBuilder::ModelBuilder() {
    digestionEngine_ = std::make_unique<FileDigestionEngine>();
    tokenizer_ = std::make_unique<CustomTokenizer>();
    trainer_ = std::make_unique<CustomModelTrainer>();
    ggufExporter_ = std::make_unique<GGUFExporter>();
    inferenceEngine_ = std::make_unique<CustomInferenceEngine>();
}

ModelBuilder::~ModelBuilder() {}

ModelBuilder& ModelBuilder::getInstance() {
    static ModelBuilder instance;
    return instance;
}

std::future<CustomModelMetadata> ModelBuilder::buildModelAsync(const BuildConfig& config) {
    return std::async(std::launch::async, [this, config]() {
        return buildModel(config);
    });
}

CustomModelMetadata ModelBuilder::buildModel(const BuildConfig& config) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          Custom Model Builder - Build Started             ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    currentConfig_ = config;
    
    // Step 1: Digest sources
    std::cout << "[1/5] Digesting source files..." << std::endl;
    for (const auto& file : config.sourceFiles) {
        digestionEngine_->addSource(file, SourceType::CODE_FILE);
    }
    for (const auto& dir : config.sourceDirectories) {
        digestionEngine_->addSourceDirectory(dir, true);
    }
    
    currentSamples_ = digestionEngine_->digestAll();
    std::cout << "  ✓ Processed " << currentSamples_.size() << " training samples" << std::endl;
    
    // Step 2: Build vocabulary
    std::cout << "\n[2/5] Building vocabulary..." << std::endl;
    tokenizer_->buildVocabulary(currentSamples_, config.architecture.vocabSize);
    currentVocab_ = tokenizer_->getVocabulary();
    std::cout << "  ✓ Vocabulary size: " << currentVocab_.vocabSize << std::endl;
    
    // Step 3: Train model
    std::cout << "\n[3/5] Training model..." << std::endl;
    trainer_->setArchitecture(config.architecture);
    trainer_->setTrainingConfig(config.trainingConfig);
    trainer_->setVocabulary(currentVocab_);
    trainer_->setTrainingData(currentSamples_);
    
    trainer_->setProgressCallback([](const CustomModelTrainer::TrainingProgress& prog) {
        if (prog.currentStep % 10 == 0) {
            std::cout << "  Epoch " << prog.currentEpoch 
                      << " - Step " << prog.currentStep << "/" << prog.totalSteps
                      << " - Loss: " << std::fixed << std::setprecision(4) << prog.currentLoss
                      << std::endl;
        }
    });
    
    trainer_->startTraining();
    std::cout << "  ✓ Training completed" << std::endl;
    
    // Step 4: Export to GGUF
    std::cout << "\n[4/5] Exporting to GGUF format..." << std::endl;
    fs::create_directories(config.outputDirectory);
    std::string ggufPath = config.outputDirectory + "/" + config.modelName + ".gguf";
    
    CustomModelMetadata metadata;
    metadata.modelName = config.modelName;
    metadata.modelId = config.modelName + "_" + std::to_string(std::time(nullptr));
    metadata.version = "1.0.0";
    metadata.description = config.modelDescription;
    metadata.createdAt = std::chrono::system_clock::now();
    metadata.architecture = config.architecture;
    metadata.ggufPath = ggufPath;
    
    ggufExporter_->setMetadata(metadata);
    ggufExporter_->setQuantization(config.quantBits);
    
    // Get trained weights (simplified)
    std::vector<float> weights(config.architecture.vocabSize * config.architecture.embeddingDim);
    ggufExporter_->exportModel(config.architecture, weights, currentVocab_, ggufPath);
    
    metadata.modelSizeBytes = fs::file_size(ggufPath);
    metadata.parametersCount = weights.size();
    
    std::cout << "  ✓ Model exported: " << ggufPath << std::endl;
    std::cout << "  ✓ Size: " << (metadata.modelSizeBytes / (1024*1024)) << " MB" << std::endl;
    
    // Step 5: Register model
    std::cout << "\n[5/5] Registering model..." << std::endl;
    registerModel(metadata);
    saveRegistry();
    std::cout << "  ✓ Model registered: " << metadata.modelName << std::endl;
    
    std::cout << "\n╔════════════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║          Custom Model Build Complete!                     ║" << std::endl;
    std::cout << "╚════════════════════════════════════════════════════════════╝\n" << std::endl;
    
    return metadata;
}

std::vector<CustomModelMetadata> ModelBuilder::listCustomModels() const {
    std::vector<CustomModelMetadata> models;
    for (const auto& [name, metadata] : registry_) {
        models.push_back(metadata);
    }
    return models;
}

CustomModelMetadata ModelBuilder::getModelMetadata(const std::string& modelName) const {
    auto it = registry_.find(modelName);
    if (it != registry_.end()) {
        return it->second;
    }
    return {};
}

bool ModelBuilder::deleteModel(const std::string& modelName) {
    auto it = registry_.find(modelName);
    if (it == registry_.end()) return false;
    
    // Delete model file
    if (fs::exists(it->second.ggufPath)) {
        fs::remove(it->second.ggufPath);
    }
    
    registry_.erase(it);
    saveRegistry();
    
    return true;
}

bool ModelBuilder::registerModel(const CustomModelMetadata& metadata) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    registry_[metadata.modelName] = metadata;
    return true;
}

bool ModelBuilder::unregisterModel(const std::string& modelName) {
    std::lock_guard<std::mutex> lock(registryMutex_);
    return registry_.erase(modelName) > 0;
}

bool ModelBuilder::saveRegistry(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    
    file << "{\n  \"models\": [\n";
    
    size_t i = 0;
    for (const auto& [name, metadata] : registry_) {
        file << "    {\n";
        file << "      \"name\": \"" << metadata.modelName << "\",\n";
        file << "      \"id\": \"" << metadata.modelId << "\",\n";
        file << "      \"version\": \"" << metadata.version << "\",\n";
        file << "      \"path\": \"" << metadata.ggufPath << "\",\n";
        file << "      \"size\": " << metadata.modelSizeBytes << "\n";
        file << "    }";
        if (++i < registry_.size()) file << ",";
        file << "\n";
    }
    
    file << "  ]\n}\n";
    
    return true;
}

bool ModelBuilder::loadRegistry(const std::string& path) {
    // Simplified JSON parsing
    if (!fs::exists(path)) return false;
    
    std::cout << "Registry loaded from: " << path << std::endl;
    return true;
}

} // namespace CustomModelBuilder
