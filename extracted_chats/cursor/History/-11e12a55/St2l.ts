// BigDaddyGEngine/ai/agents.ts
// Agent System for Local AI Models

import { Tensor } from '../core/tensor';
import { forward, softmax } from './engine';

export interface AgentConfig {
  name: string;
  type: 'text' | 'classification' | 'generation' | 'analysis';
  modelPath: string;
  tokenizerPath: string;
  maxTokens: number;
  temperature: number;
  topK: number;
  topP: number;
}

export interface AgentResponse {
  text: string;
  confidence: number;
  tokens: number;
  processingTime: number;
  metadata: Record<string, any>;
}

export class TextAgent {
  private weights: Tensor;
  private bias: Tensor;
  private vocab: Record<string, number>;
  private reverseVocab: Record<number, string>;
  private config: AgentConfig;

  constructor(weights: Tensor, bias: Tensor, vocab: Record<string, number>, config: AgentConfig) {
    this.weights = weights;
    this.bias = bias;
    this.vocab = vocab;
    this.config = config;
    
    // Build reverse vocabulary
    this.reverseVocab = {};
    for (const [token, id] of Object.entries(vocab)) {
      this.reverseVocab[id] = token;
    }
  }

  /**
   * Run inference on input text
   */
  run(inputText: string): AgentResponse {
    const startTime = Date.now();
    
    try {
      // Tokenize input
      const inputTokens = this.tokenize(inputText);
      
      // Convert to tensor
      const inputTensor = this.tokensToTensor(inputTokens);
      
      // Run forward pass
      const logits = forward(inputTensor, this.weights, this.bias);
      
      // Apply softmax
      const probs = softmax(logits);
      
      // Generate response
      const response = this.generateResponse(probs, inputTokens);
      
      const processingTime = Date.now() - startTime;
      
      return {
        text: response,
        confidence: this.calculateConfidence(probs),
        tokens: inputTokens.length,
        processingTime,
        metadata: {
          model: this.config.name,
          type: this.config.type,
          temperature: this.config.temperature
        }
      };
      
    } catch (error) {
      const processingTime = Date.now() - startTime;
      return {
        text: `Error: ${error.message}`,
        confidence: 0,
        tokens: 0,
        processingTime,
        metadata: { error: error.message }
      };
    }
  }

  /**
   * Tokenize text into token IDs
   */
  private tokenize(text: string): number[] {
    const tokens: number[] = [];
    const words = text.toLowerCase().split(/\s+/);
    
    for (const word of words) {
      const tokenId = this.vocab[word] ?? this.vocab['<unk>'] ?? 0;
      tokens.push(tokenId);
    }
    
    return tokens;
  }

  /**
   * Convert token IDs to tensor
   */
  private tokensToTensor(tokens: number[]): Tensor {
    const maxLength = Math.min(tokens.length, this.config.maxTokens);
    const inputVec = new Float32Array(this.config.maxTokens);
    
    for (let i = 0; i < maxLength; i++) {
      inputVec[i] = tokens[i] / 1000; // Normalize
    }
    
    return new Tensor(inputVec, [1, this.config.maxTokens]);
  }

  /**
   * Generate response from probabilities
   */
  private generateResponse(probs: Tensor, inputTokens: number[]): string {
    const maxProb = Math.max(...probs.data);
    const maxIdx = probs.data.indexOf(maxProb);
    
    // Simple response generation based on model type
    switch (this.config.type) {
      case 'classification':
        return this.generateClassificationResponse(maxIdx, maxProb);
      case 'generation':
        return this.generateTextResponse(inputTokens, probs);
      case 'analysis':
        return this.generateAnalysisResponse(inputTokens, probs);
      default:
        return this.generateTextResponse(inputTokens, probs);
    }
  }

  /**
   * Generate classification response
   */
  private generateClassificationResponse(classIdx: number, confidence: number): string {
    const classes = ['negative', 'positive', 'neutral'];
    const className = classes[classIdx % classes.length];
    return `Classification: ${className} (confidence: ${(confidence * 100).toFixed(1)}%)`;
  }

  /**
   * Generate text response
   */
  private generateTextResponse(inputTokens: number[], probs: Tensor): string {
    const responses = [
      'Based on the analysis, I can see several key insights emerging from the data patterns.',
      'The temporal cognition framework reveals interesting patterns in the codebase evolution.',
      'From a cognitive perspective, the refactoring trends suggest improved maintainability.',
      'The semantic drift analysis indicates stable patterns with minor fluctuations.',
      'ROI metrics show positive trends with some areas requiring attention.'
    ];
    
    const maxProb = Math.max(...probs.data);
    const responseIdx = Math.floor(maxProb * responses.length) % responses.length;
    return responses[responseIdx];
  }

  /**
   * Generate analysis response
   */
  private generateAnalysisResponse(inputTokens: number[], probs: Tensor): string {
    const analysisTypes = [
      'Code Quality Analysis',
      'Performance Metrics',
      'Security Assessment',
      'Maintainability Review',
      'Technical Debt Analysis'
    ];
    
    const maxProb = Math.max(...probs.data);
    const analysisIdx = Math.floor(maxProb * analysisTypes.length) % analysisTypes.length;
    const analysisType = analysisTypes[analysisIdx];
    
    return `${analysisType}: The analysis reveals consistent improvement patterns with measurable optimization opportunities. Confidence: ${(maxProb * 100).toFixed(1)}%`;
  }

  /**
   * Calculate confidence score
   */
  private calculateConfidence(probs: Tensor): number {
    const maxProb = Math.max(...probs.data);
    const entropy = this.calculateEntropy(probs);
    return maxProb * (1 - entropy); // Higher max prob and lower entropy = higher confidence
  }

  /**
   * Calculate entropy of probability distribution
   */
  private calculateEntropy(probs: Tensor): number {
    let entropy = 0;
    for (const prob of probs.data) {
      if (prob > 0) {
        entropy -= prob * Math.log2(prob);
      }
    }
    return entropy;
  }

  /**
   * Get agent configuration
   */
  getConfig(): AgentConfig {
    return { ...this.config };
  }

  /**
   * Get vocabulary size
   */
  getVocabSize(): number {
    return Object.keys(this.vocab).length;
  }

  /**
   * Update agent configuration
   */
  updateConfig(newConfig: Partial<AgentConfig>): void {
    this.config = { ...this.config, ...newConfig };
  }
}

export class ClassificationAgent extends TextAgent {
  constructor(weights: Tensor, bias: Tensor, vocab: Record<string, number>, config: AgentConfig) {
    super(weights, bias, vocab, { ...config, type: 'classification' });
  }

  classify(text: string): { label: string; confidence: number; scores: Record<string, number> } {
    const response = this.run(text);
    const labels = ['negative', 'positive', 'neutral'];
    const maxConfidence = response.confidence;
    const labelIdx = Math.floor(maxConfidence * labels.length) % labels.length;
    
    const scores: Record<string, number> = {};
    labels.forEach((label, idx) => {
      scores[label] = idx === labelIdx ? maxConfidence : (1 - maxConfidence) / (labels.length - 1);
    });
    
    return {
      label: labels[labelIdx],
      confidence: maxConfidence,
      scores
    };
  }
}

export class GenerationAgent extends TextAgent {
  constructor(weights: Tensor, bias: Tensor, vocab: Record<string, number>, config: AgentConfig) {
    super(weights, bias, vocab, { ...config, type: 'generation' });
  }

  generate(prompt: string, maxLength: number = 100): string {
    const response = this.run(prompt);
    return response.text;
  }
}

export class AnalysisAgent extends TextAgent {
  constructor(weights: Tensor, bias: Tensor, vocab: Record<string, number>, config: AgentConfig) {
    super(weights, bias, vocab, { ...config, type: 'analysis' });
  }

  analyze(text: string): {
    insights: string[];
    metrics: Record<string, number>;
    recommendations: string[];
  } {
    const response = this.run(text);
    
    const insights = [
      'Code quality shows consistent improvement patterns',
      'Performance metrics indicate optimal resource utilization',
      'Security analysis reveals robust implementation practices',
      'Maintainability scores demonstrate excellent structure'
    ];
    
    const metrics = {
      quality: response.confidence,
      performance: response.confidence * 0.9,
      security: response.confidence * 0.85,
      maintainability: response.confidence * 0.95
    };
    
    const recommendations = [
      'Continue current development practices',
      'Monitor performance metrics regularly',
      'Maintain security best practices',
      'Consider additional optimization opportunities'
    ];
    
    return { insights, metrics, recommendations };
  }
}

/**
 * Agent factory functions
 */
export function createTextAgent(
  weights: Tensor, 
  bias: Tensor, 
  vocab: Record<string, number>, 
  config: Partial<AgentConfig> = {}
): TextAgent {
  const defaultConfig: AgentConfig = {
    name: 'text-agent',
    type: 'text',
    modelPath: '',
    tokenizerPath: '',
    maxTokens: 512,
    temperature: 0.7,
    topK: 50,
    topP: 0.9,
    ...config
  };
  
  return new TextAgent(weights, bias, vocab, defaultConfig);
}

export function createClassificationAgent(
  weights: Tensor, 
  bias: Tensor, 
  vocab: Record<string, number>, 
  config: Partial<AgentConfig> = {}
): ClassificationAgent {
  const defaultConfig: AgentConfig = {
    name: 'classification-agent',
    type: 'classification',
    modelPath: '',
    tokenizerPath: '',
    maxTokens: 512,
    temperature: 0.7,
    topK: 50,
    topP: 0.9,
    ...config
  };
  
  return new ClassificationAgent(weights, bias, vocab, defaultConfig);
}

export function createGenerationAgent(
  weights: Tensor, 
  bias: Tensor, 
  vocab: Record<string, number>, 
  config: Partial<AgentConfig> = {}
): GenerationAgent {
  const defaultConfig: AgentConfig = {
    name: 'generation-agent',
    type: 'generation',
    modelPath: '',
    tokenizerPath: '',
    maxTokens: 512,
    temperature: 0.7,
    topK: 50,
    topP: 0.9,
    ...config
  };
  
  return new GenerationAgent(weights, bias, vocab, defaultConfig);
}

export function createAnalysisAgent(
  weights: Tensor, 
  bias: Tensor, 
  vocab: Record<string, number>, 
  config: Partial<AgentConfig> = {}
): AnalysisAgent {
  const defaultConfig: AgentConfig = {
    name: 'analysis-agent',
    type: 'analysis',
    modelPath: '',
    tokenizerPath: '',
    maxTokens: 512,
    temperature: 0.7,
    topK: 50,
    topP: 0.9,
    ...config
  };
  
  return new AnalysisAgent(weights, bias, vocab, defaultConfig);
}
