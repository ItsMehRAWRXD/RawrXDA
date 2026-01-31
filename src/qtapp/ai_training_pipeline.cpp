#include "ai_training_pipeline.hpp"
AITrainingPipeline::AITrainingPipeline()
    
    , m_backend(TrainingBackend::LlamaCpp)
    , m_isTraining(false)
    , m_isPaused(false)
    , m_progress(0.0)
    , m_status("Ready")
    , m_progressTimer(nullptr)
    , m_checkpointTimer(nullptr)
    , m_networkManager(nullptr)
    , m_currentEpoch(0)
    , m_totalEpochs(0)
    , m_currentStep(0)
    , m_totalSteps(0)
    , m_currentLoss(0.0)
    , m_currentAccuracy(0.0)
    , m_bestLoss(std::numeric_limits<double>::max())
{
    // Initialize network manager
    m_networkManager = new void*(this);
    
    // Initialize progress timer
    m_progressTimer = new // Timer(this);
    m_progressTimer->setInterval(1000); // Update every second  // Signal connection removed\n// Initialize checkpoint timer
    m_checkpointTimer = new // Timer(this);
    m_checkpointTimer->setInterval(60000); // Checkpoint every minute  // Signal connection removed\n// Initialize training components
    m_llamaTrainer = std::make_unique<LlamaTrainer>(this);
    m_quantizer = std::make_unique<ModelQuantizer>(this);
    m_validator = std::make_unique<TrainingValidator>(this);
    
    // Connect signals
    // Connect removed, &LlamaTrainer::trainingProgress, 
            this, [this](double progress) {
                m_progress = progress;
                updateTrainingMetrics();
            });
    
    // Connect removed, &LlamaTrainer::trainingCompleted,
            this, [this](const std::string& modelPath) {
                m_modelOutputPath = modelPath;
                trainingCompleted(modelPath);
                m_isTraining = false;
            });  // Signal connection removed\nm_isTraining = false;
            });
    
}

AITrainingPipeline::~AITrainingPipeline() {
    stopTraining();
    cleanupTraining();
}

void AITrainingPipeline::setBackend(TrainingBackend backend) {
    m_backend = backend;
}

TrainingBackend AITrainingPipeline::getBackend() const {
    return m_backend;
}

void AITrainingPipeline::setArchitecture(const AITrainingArchitecture& arch) {
    m_architecture = arch;
}

AITrainingArchitecture AITrainingPipeline::getArchitecture() const {
    return m_architecture;
}

void AITrainingPipeline::setHyperparameters(const TrainingHyperparameters& params) {
    m_hyperparameters = params;
}

TrainingHyperparameters AITrainingPipeline::getHyperparameters() const {
    return m_hyperparameters;
}

bool AITrainingPipeline::prepareTraining(const AIDigestionDataset& dataset, const DigestionConfig& config) {
    if (m_isTraining) {
        return false;
    }
    
    m_dataset = dataset;
    m_config = config;
    m_status = "Preparing training environment...";
    
    // Create working directory
    m_workingDir = std::make_unique<QTemporaryDir>();
    if (!m_workingDir->isValid()) {
        return false;
    }
    
    // Setup training environment
    if (!setupTrainingEnvironment()) {
        return false;
    }
    
    // Prepare training data
    if (!prepareTrainingData(dataset)) {
        return false;
    }
    
    // Generate training configuration
    if (!createConfigFiles()) {
        return false;
    }
    
    // Generate training script
    if (!generateTrainingScript()) {
        return false;
    }
    
    m_status = "Ready for training";
    return true;
}

void AITrainingPipeline::startTraining() {
    if (m_isTraining) {
        return;
    }
    
    m_isTraining = true;
    m_isPaused = false;
    m_progress = 0.0;
    m_currentEpoch = 0;
    m_totalEpochs = m_config.epochs;
    m_currentStep = 0;
    m_currentLoss = 0.0;
    m_currentAccuracy = 0.0;
    m_bestLoss = std::numeric_limits<double>::max();
    m_trainingStartTime = // DateTime::currentDateTime();
    
    m_status = "Starting training...";
    
    // Start backend-specific training
    bool success = false;
    switch (m_backend) {
        case TrainingBackend::LlamaCpp:
            success = setupLlamaCppTraining();
            break;
        case TrainingBackend::Transformers:
            success = setupTransformersTraining();
            break;
        case TrainingBackend::Custom:
            success = setupCustomTraining();
            break;
        case TrainingBackend::Ollama:
            success = setupOllamaTraining();
            break;
    }
    
    if (!success) {
        m_isTraining = false;
        trainingFailed("Failed to start training backend");
        return;
    }
    
    // Start timers
    m_progressTimer->start();
    m_checkpointTimer->start();
    
    trainingStarted();
    trainingProgress(m_progress, m_metrics);
}

void AITrainingPipeline::stopTraining() {
    if (!m_isTraining) return;
    
    m_isTraining = false;
    m_isPaused = false;
    
    // Stop timers
    if (m_progressTimer) m_progressTimer->stop();
    if (m_checkpointTimer) m_checkpointTimer->stop();
    
    // Stop training process
    if (m_trainingProcess && m_trainingProcess->state() == void*::Running) {
        m_trainingProcess->terminate();
        if (!m_trainingProcess->waitForFinished(5000)) {
            m_trainingProcess->kill();
        }
    }
    
    m_status = "Training stopped";
}

void AITrainingPipeline::pauseTraining() {
    if (m_isTraining && !m_isPaused) {
        m_isPaused = true;
        m_status = "Training paused";
        trainingPaused();
    }
}

void AITrainingPipeline::resumeTraining() {
    if (m_isTraining && m_isPaused) {
        m_isPaused = false;
        m_status = "Training resumed";
        trainingResumed();
    }
}

bool AITrainingPipeline::isTraining() const {
    return m_isTraining;
}

bool AITrainingPipeline::isPaused() const {
    return m_isPaused;
}

double AITrainingPipeline::getTrainingProgress() const {
    return m_progress;
}

std::string AITrainingPipeline::getTrainingStatus() const {
    return m_status;
}

nlohmann::json AITrainingPipeline::getTrainingMetrics() const {
    return m_metrics;
}

std::string AITrainingPipeline::getModelOutputPath() const {
    return m_modelOutputPath;
}

bool AITrainingPipeline::quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization) {
    if (!m_quantizer) {
        return false;
    }
    
    return m_quantizer->quantizeModel(inputPath, outputPath, quantization);
}

bool AITrainingPipeline::validateModel(const std::string& modelPath) {
    if (!m_validator) {
        return false;
    }
    
    return m_validator->validateModel(modelPath);
}

bool AITrainingPipeline::testModel(const std::string& modelPath, const std::stringList& testPrompts) {
    if (!m_validator) {
        return false;
    }
    
    return m_validator->testModel(modelPath, testPrompts);
}

void AITrainingPipeline::onTrainingTimeout() {
    if (!m_isTraining) return;
    
    // Update training metrics
    updateTrainingMetrics();
    
    // Parse training logs if available
    parseTrainingLogs();
    
    trainingProgress(m_progress, m_metrics);
}

void AITrainingPipeline::onModelCheckpoint() {
    if (!m_isTraining || m_isPaused) return;
    
    // Save training state
    saveTrainingState();
    
    // Save model checkpoint if loss improved
    if (m_currentLoss < m_bestLoss) {
        m_bestLoss = m_currentLoss;
        saveModelCheckpoints();
    }
}

bool AITrainingPipeline::setupTrainingEnvironment() {
    // Create necessary directories
    // workDir(m_workingDir->path());
    workDir.mkpath("data");
    workDir.mkpath("models");
    workDir.mkpath("logs");
    workDir.mkpath("checkpoints");
    
    // Set paths
    m_datasetPath = workDir.filePath("data/training_data.jsonl");
    m_configPath = workDir.filePath("config.json");
    m_scriptPath = workDir.filePath("train.py");
    m_modelOutputPath = // (m_config.outputDirectory).filePath(m_config.modelName + ".gguf");
    
    // Check GPU availability
    bool hasGPU = checkGPUAvailability();
    m_metrics["has_gpu"] = hasGPU;
    
    // Install dependencies if needed
    if (!installDependencies()) {
        return false;
    }
    
    return true;
}

bool AITrainingPipeline::prepareTrainingData(const AIDigestionDataset& dataset) {
    // File operation removed;
    if (!dataFile.open(std::iostream::WriteOnly | std::iostream::Text)) {
        return false;
    }
    
    std::stringstream stream(&dataFile);
    
    // Convert knowledge representations to training format
    for (const auto& knowledge : dataset.samples) {
        // Create training examples from knowledge
        nlohmann::json trainingExample;
        
        // Generate instruction-response pairs
        if (!knowledge.functions.empty()) {
            trainingExample["instruction"] = std::string("Explain the function %1 from %2")
                                             )
                                             .baseName());
            trainingExample["input"] = "";
            trainingExample["output"] = std::string("Function %1: %2")
                                        )
                                        );
        } else if (!knowledge.classes.empty()) {
            trainingExample["instruction"] = std::string("Describe the class %1 from %2")
                                             )
                                             .baseName());
            trainingExample["input"] = "";
            trainingExample["output"] = std::string("Class %1: %2")
                                        )
                                        );
        } else {
            trainingExample["instruction"] = std::string("What can you tell me about %1?")
                                             .baseName());
            trainingExample["input"] = "";
            trainingExample["output"] = knowledge.content.left(1000);
        }
        
        // Add metadata
        trainingExample["source_file"] = knowledge.originalFile;
        trainingExample["file_type"] = static_cast<int>(knowledge.fileType);
        trainingExample["keywords"] = nlohmann::json::fromStringList(knowledge.keywords);
        
        // Write as JSONL
        nlohmann::json doc(trainingExample);
        stream << doc.toJson(nlohmann::json::Compact) << "\n";
    }
    
    dataFile.close();
    
    // Calculate total training steps
    int batchSize = m_hyperparameters.batchSize * m_hyperparameters.gradientAccumulationSteps;
    m_totalSteps = (dataset.totalSamples / batchSize) * m_config.epochs;
    
    
    return true;
}

bool AITrainingPipeline::createConfigFiles() {
    // Create training configuration
    nlohmann::json config;
    config["model_name"] = m_config.modelName;
    config["output_dir"] = m_config.outputDirectory;
    config["dataset_path"] = m_datasetPath;
    
    // Architecture configuration
    nlohmann::json archConfig;
    archConfig["base_model"] = m_architecture.baseModel;
    archConfig["hidden_size"] = m_architecture.hiddenSize;
    archConfig["num_layers"] = m_architecture.numLayers;
    archConfig["num_attention_heads"] = m_architecture.numAttentionHeads;
    archConfig["intermediate_size"] = m_architecture.intermediateSize;
    archConfig["max_position_embeddings"] = m_architecture.maxPositionEmbeddings;
    archConfig["activation_function"] = m_architecture.activationFunction;
    archConfig["dropout_rate"] = m_architecture.dropoutRate;
    config["architecture"] = archConfig;
    
    // Hyperparameters
    nlohmann::json hyperConfig;
    hyperConfig["learning_rate"] = m_hyperparameters.learningRate;
    hyperConfig["weight_decay"] = m_hyperparameters.weightDecay;
    hyperConfig["beta1"] = m_hyperparameters.beta1;
    hyperConfig["beta2"] = m_hyperparameters.beta2;
    hyperConfig["epsilon"] = m_hyperparameters.epsilon;
    hyperConfig["batch_size"] = m_hyperparameters.batchSize;
    hyperConfig["gradient_accumulation_steps"] = m_hyperparameters.gradientAccumulationSteps;
    hyperConfig["max_gradient_norm"] = m_hyperparameters.maxGradientNorm;
    hyperConfig["warmup_ratio"] = m_hyperparameters.warmupRatio;
    hyperConfig["scheduler"] = m_hyperparameters.scheduler;
    hyperConfig["save_steps"] = m_hyperparameters.saveSteps;
    hyperConfig["eval_steps"] = m_hyperparameters.evaluationSteps;
    hyperConfig["logging_steps"] = m_hyperparameters.loggingSteps;
    hyperConfig["use_gradient_checkpointing"] = m_hyperparameters.useGradientCheckpointing;
    hyperConfig["use_fp16"] = m_hyperparameters.useFp16;
    hyperConfig["use_bf16"] = m_hyperparameters.useBf16;
    config["hyperparameters"] = hyperConfig;
    
    config["epochs"] = m_config.epochs;
    config["max_tokens"] = m_config.maxTokens;
    config["quantization"] = m_config.quantization;
    
    // Save config file
    // File operation removed;
    if (!configFile.open(std::iostream::WriteOnly)) {
        return false;
    }
    
    nlohmann::json doc(config);
    configFile.write(doc.toJson());
    configFile.close();
    
    return true;
}

bool AITrainingPipeline::generateTrainingScript() {
    std::string script;
    
    switch (m_backend) {
        case TrainingBackend::LlamaCpp:
            script = R"(#!/usr/bin/env python3
import os
import sys
import json
import torch
from transformers import (
    AutoTokenizer, AutoModelForCausalLM, 
    TrainingArguments, Trainer, DataCollatorForLanguageModeling
)
from datasets import Dataset
import logging

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def load_config(config_path):
    with open(config_path, 'r') as f:
        return json.load(f)

def load_dataset(dataset_path):
    data = []
    with open(dataset_path, 'r') as f:
        for line in f:
            data.append(json.loads(line))
    return Dataset.from_list(data)

def preprocess_data(examples, tokenizer, max_length=2048):
    # Format as instruction-following
    texts = []
    for instruction, input_text, output in zip(
        examples['instruction'], examples['input'], examples['output']
    ):
        if input_text:
            text = f"### Instruction:\n{instruction}\n\n### Input:\n{input_text}\n\n### Response:\n{output}"
        else:
            text = f"### Instruction:\n{instruction}\n\n### Response:\n{output}"
        texts.append(text)
    
    tokenized = tokenizer(
        texts, 
        truncation=True, 
        padding=False, 
        max_length=max_length,
        return_overflowing_tokens=False
    )
    
    # Set labels for language modeling
    tokenized["labels"] = tokenized["input_ids"].copy()
    
    return tokenized

def main():
    config_path = sys.argv[1] if len(sys.argv) > 1 else "config.json"
    config = load_config(config_path)
    
    # Load model and tokenizer
    model_name = config['architecture'].get('base_model', 'microsoft/DialoGPT-medium')
    
    logger.info(f"Loading model: {model_name}")
    tokenizer = AutoTokenizer.from_pretrained(model_name)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token
    
    model = AutoModelForCausalLM.from_pretrained(
        model_name,
        torch_dtype=torch.float16 if config['hyperparameters'].get('use_fp16') else torch.float32,
        device_map="auto" if torch.cuda.is_available() else None
    )
    
    # Load and preprocess dataset
    logger.info(f"Loading dataset: {config['dataset_path']}")
    dataset = load_dataset(config['dataset_path'])
    
    def tokenize_function(examples):
        return preprocess_data(examples, tokenizer, config.get('max_tokens', 2048))
    
    tokenized_dataset = dataset.map(
        tokenize_function, 
        batched=True,
        remove_columns=dataset.column_names
    )
    
    # Split dataset
    split_dataset = tokenized_dataset.train_test_split(test_size=0.1)
    train_dataset = split_dataset['train']
    eval_dataset = split_dataset['test']
    
    # Setup training arguments
    training_args = TrainingArguments(
        output_dir=config['output_dir'],
        overwrite_output_dir=True,
        num_train_epochs=config.get('epochs', 3),
        per_device_train_batch_size=config['hyperparameters']['batch_size'],
        per_device_eval_batch_size=config['hyperparameters']['batch_size'],
        gradient_accumulation_steps=config['hyperparameters']['gradient_accumulation_steps'],
        learning_rate=config['hyperparameters']['learning_rate'],
        weight_decay=config['hyperparameters']['weight_decay'],
        adam_beta1=config['hyperparameters']['beta1'],
        adam_beta2=config['hyperparameters']['beta2'],
        adam_epsilon=config['hyperparameters']['epsilon'],
        max_grad_norm=config['hyperparameters']['max_gradient_norm'],
        warmup_ratio=config['hyperparameters']['warmup_ratio'],
        lr_scheduler_type=config['hyperparameters']['scheduler'],
        logging_steps=config['hyperparameters']['logging_steps'],
        save_steps=config['hyperparameters']['save_steps'],
        eval_steps=config['hyperparameters']['eval_steps'],
        evaluation_strategy="steps",
        save_strategy="steps",
        load_best_model_at_end=True,
        metric_for_best_model="eval_loss",
        greater_is_better=False,
        fp16=config['hyperparameters'].get('use_fp16', False),
        bf16=config['hyperparameters'].get('use_bf16', False),
        gradient_checkpointing=config['hyperparameters'].get('use_gradient_checkpointing', True),
        dataloader_pin_memory=False,
        remove_unused_columns=False,
        report_to=[],  # Disable wandb/tensorboard
    )
    
    # Data collator
    data_collator = DataCollatorForLanguageModeling(
        tokenizer=tokenizer,
        mlm=False,  # Causal language modeling
        pad_to_multiple_of=8
    )
    
    # Initialize trainer
    trainer = Trainer(
        model=model,
        args=training_args,
        train_dataset=train_dataset,
        eval_dataset=eval_dataset,
        data_collator=data_collator,
        tokenizer=tokenizer,
    )
    
    # Train the model
    logger.info("Starting training...")
    trainer.train()
    
    # Save final model
    final_model_path = os.path.join(config['output_dir'], "final_model")
    trainer.save_model(final_model_path)
    tokenizer.save_pretrained(final_model_path)
    
    logger.info(f"Training completed. Model saved to: {final_model_path}")

if __name__ == "__main__":
    main()
)";
            break;
            
        default:
            return false;
    }
    
    // Save script to file
    // File operation removed;
    if (!scriptFile.open(std::iostream::WriteOnly | std::iostream::Text)) {
        return false;
    }
    
    std::stringstream stream(&scriptFile);
    stream << script;
    scriptFile.close();
    
    return true;
}

bool AITrainingPipeline::setupLlamaCppTraining() {
    // Use the LlamaTrainer component
    if (!m_llamaTrainer->prepareTraining(m_datasetPath, m_architecture)) {
        return false;
    }
    
    return m_llamaTrainer->startTraining(m_hyperparameters);
}

bool AITrainingPipeline::setupTransformersTraining() {
    // Setup Python transformers training
    m_trainingProcess = std::make_unique<void*>(this);  // Signal connection removed\n  // Signal connection removed\n  // Signal connection removed\n// Start training process
    std::string python = "python";
    std::stringList args;
    args << m_scriptPath << m_configPath;
    
    m_trainingProcess->start(python, args);
    
    return m_trainingProcess->waitForStarted();
}

bool AITrainingPipeline::setupCustomTraining() {
    // Custom training implementation
    return false;
}

bool AITrainingPipeline::setupOllamaTraining() {
    // Ollama training implementation
    return false;
}

void AITrainingPipeline::handleTrainingOutput() {
    if (!m_trainingProcess) return;
    
    std::vector<uint8_t> data = m_trainingProcess->readAllStandardOutput();
    std::string output = std::string::fromUtf8(data);
    
    
    // Parse training progress from output
    parseTrainingLogs();
}

void AITrainingPipeline::handleTrainingError() {
    if (!m_trainingProcess) return;
    
    std::vector<uint8_t> data = m_trainingProcess->readAllStandardError();
    std::string error = std::string::fromUtf8(data);
    
}

void AITrainingPipeline::handleTrainingFinished() {
    m_isTraining = false;
    
    if (m_trainingProcess) {
        int exitCode = m_trainingProcess->exitCode();
        if (exitCode == 0) {
            trainingCompleted(m_modelOutputPath);
        } else {
            trainingFailed(std::string("Training process exited with code %1"));
        }
    }
}

void AITrainingPipeline::parseTrainingLogs() {
    // Parse training logs to extract metrics
    // This would parse actual log files in a real implementation
    
    // Simulate progress update
    if (m_totalSteps > 0) {
        m_currentStep = qMin(m_currentStep + 1, m_totalSteps);
        m_progress = static_cast<double>(m_currentStep) / m_totalSteps;
    }
    
    // Simulate loss decrease
    auto randomGen = QRandomGenerator::global();
    m_currentLoss = 4.0 - (m_progress * 2.0) + (randomGen->bounded(100)) / 1000.0;
    m_currentAccuracy = m_progress * 0.8 + (randomGen->bounded(20)) / 100.0;
}

void AITrainingPipeline::updateTrainingMetrics() {
    m_metrics = nlohmann::json();
    m_metrics["progress"] = m_progress;
    m_metrics["current_epoch"] = m_currentEpoch;
    m_metrics["total_epochs"] = m_totalEpochs;
    m_metrics["current_step"] = m_currentStep;
    m_metrics["total_steps"] = m_totalSteps;
    m_metrics["current_loss"] = m_currentLoss;
    m_metrics["current_accuracy"] = m_currentAccuracy;
    m_metrics["best_loss"] = m_bestLoss;
    m_metrics["elapsed_time"] = m_trainingStartTime.secsTo(// DateTime::currentDateTime());
}

void AITrainingPipeline::saveTrainingState() {
    // Save current training state for resuming
    nlohmann::json state;
    state["current_epoch"] = m_currentEpoch;
    state["current_step"] = m_currentStep;
    state["best_loss"] = m_bestLoss;
    state["metrics"] = m_metrics;
    
    std::string statePath = // (m_workingDir->path()).filePath("training_state.json");
    // File operation removed;
    if (stateFile.open(std::iostream::WriteOnly)) {
        nlohmann::json doc(state);
        stateFile.write(doc.toJson());
    }
}

bool AITrainingPipeline::saveModelCheckpoints() {
    // Save model checkpoints
    std::string checkpointDir = // (m_workingDir->path()).filePath("checkpoints");
    std::filesystem::create_directories(checkpointDir);
    
    std::string checkpointPath = // (checkpointDir).filePath(std::string("checkpoint-%1"));
    
    // This would save actual model checkpoints in a real implementation
    
    modelSaved(checkpointPath);
    return true;
}

bool AITrainingPipeline::checkGPUAvailability() {
    // Check if CUDA/GPU is available
    // Process removed
    process.start("nvidia-smi");
    process.waitForFinished(3000);
    
    return process.exitCode() == 0;
}

bool AITrainingPipeline::installDependencies() {
    // Install required Python packages
    std::stringList packages = {
        "torch", "transformers", "datasets", "accelerate", "peft", "bitsandbytes"
    };
    
    for (const std::string& package : packages) {
        // Process removed
        process.start("pip", std::stringList() << "install" << package);
        if (!process.waitForFinished(60000) || process.exitCode() != 0) {
            // Continue anyway - might already be installed
        }
    }
    
    return true;
}

void AITrainingPipeline::cleanupTraining() {
    if (m_progressTimer) {
        m_progressTimer->stop();
    }
    
    if (m_checkpointTimer) {
        m_checkpointTimer->stop();
    }
    
    if (m_trainingProcess) {
        if (m_trainingProcess->state() == void*::Running) {
            m_trainingProcess->terminate();
            m_trainingProcess->waitForFinished(3000);
        }
        m_trainingProcess.reset();
    }
}

// LlamaTrainer implementation
LlamaTrainer::LlamaTrainer()  {
}

bool LlamaTrainer::prepareTraining(const std::string& datasetPath, const AITrainingArchitecture& arch) {
    // Prepare llama.cpp training
    // This would download and build llama.cpp if needed
    return true;
}

bool LlamaTrainer::startTraining(const TrainingHyperparameters& params) {
    // Start llama.cpp training
    // This would execute the actual training
    
    // Simulate training progress
    // Timer timer = new // Timer(this);
    // Connect removed {
        static double progress = 0.0;
        progress += 0.01;
        trainingProgress(progress);
        
        if (progress >= 1.0) {
            timer->stop();
            timer->deleteLater();
            trainingCompleted("model.gguf");
        }
    });
    timer->start(100);
    
    return true;
}

bool LlamaTrainer::quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization) {
    // Quantize model using llama.cpp
    return true;
}

// ModelQuantizer implementation
ModelQuantizer::ModelQuantizer()  {
}

bool ModelQuantizer::quantizeModel(const std::string& inputPath, const std::string& outputPath, const std::string& quantization) {
    // Implement actual quantization
    return true;
}

std::stringList ModelQuantizer::getSupportedQuantizations() const {
    return {"Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q8_0", "F16", "F32"};
}

double ModelQuantizer::estimateQuantizedSize(const std::string& modelPath, const std::string& quantization) {
    // Estimate quantized model size
    return 0.0;
}

// TrainingValidator implementation
TrainingValidator::TrainingValidator()  {
}

bool TrainingValidator::validateModel(const std::string& modelPath) {
    // Validate model
    return true;
}

bool TrainingValidator::testModel(const std::string& modelPath, const std::stringList& testPrompts) {
    // Test model with prompts
    return true;
}

nlohmann::json TrainingValidator::getValidationResults() const {
    return m_validationResults;
}

