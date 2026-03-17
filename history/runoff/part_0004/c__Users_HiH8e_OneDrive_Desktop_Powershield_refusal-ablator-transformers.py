#!/usr/bin/env python3
"""
Refusal Removal via Refusal Direction Ablation
Based on: https://github.com/Sumandora/remove-refusals-with-transformers
and: https://www.lesswrong.com/posts/jGuXSZgv6qfdhMCuJ/refusal-in-llms-is-mediated-by-a-single-direction

This approach:
1. Loads harmful and harmless prompt pairs
2. Computes activations for both
3. Finds the principal direction differentiating them
4. Removes that direction from the model's layers
5. Saves the ablated model

Supports any HF Transformers model that has `model.model.layers`
"""

import torch
import numpy as np
from transformers import AutoModelForCausalLM, AutoTokenizer
from pathlib import Path
import json
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class RefusalAblator:
    def __init__(self, model_name, device="cuda"):
        self.model_name = model_name
        self.device = device
        self.model = None
        self.tokenizer = None
        self.harmful_activations = []
        self.harmless_activations = []
        self.refusal_direction = None
        
    def load_model(self):
        """Load model and tokenizer"""
        logger.info(f"Loading model: {self.model_name}")
        self.tokenizer = AutoTokenizer.from_pretrained(self.model_name, trust_remote_code=True)
        self.model = AutoModelForCausalLM.from_pretrained(
            self.model_name,
            torch_dtype=torch.float16,
            device_map="auto",
            trust_remote_code=True
        )
        self.model.eval()
        logger.info(f"✓ Model loaded on {self.device}")
        
    def get_activations(self, prompts, layer_idx=-1):
        """Extract hidden states from model for given prompts"""
        activations = []
        
        with torch.no_grad():
            for prompt in prompts:
                inputs = self.tokenizer(prompt, return_tensors="pt", truncation=True, max_length=512)
                inputs = {k: v.to(self.device) for k, v in inputs.items()}
                
                outputs = self.model(
                    **inputs,
                    output_hidden_states=True,
                    return_dict=True
                )
                
                # Get hidden states from specified layer (default: last layer)
                hidden_states = outputs.hidden_states[layer_idx]
                # Use the mean of token representations
                mean_activation = hidden_states.mean(dim=1).detach().cpu()
                activations.append(mean_activation.numpy())
        
        return np.array(activations)
    
    def compute_refusal_direction(self):
        """Compute the principal direction that differentiates harmful from harmless"""
        logger.info("Computing refusal direction...")
        
        # Combine all activations
        all_harmful = np.vstack(self.harmful_activations)
        all_harmless = np.vstack(self.harmless_activations)
        
        # Compute mean difference
        mean_harmful = all_harmful.mean(axis=0)
        mean_harmless = all_harmless.mean(axis=0)
        
        # The refusal direction is the difference
        self.refusal_direction = mean_harmful - mean_harmless
        
        # Normalize
        self.refusal_direction = self.refusal_direction / np.linalg.norm(self.refusal_direction)
        
        logger.info(f"✓ Refusal direction computed (shape: {self.refusal_direction.shape})")
        return self.refusal_direction
    
    def ablate_model(self, alpha=1.0):
        """Remove refusal direction from all layers"""
        logger.info(f"Ablating refusal direction (alpha={alpha})...")
        
        if self.refusal_direction is None:
            raise ValueError("Must compute refusal direction first")
        
        refusal_dir_tensor = torch.from_numpy(self.refusal_direction).float().to(self.device)
        
        # Iterate through model layers
        try:
            layers = self.model.model.layers
        except AttributeError:
            logger.error("Model architecture not supported. Expected model.model.layers")
            raise
        
        ablation_count = 0
        for layer_idx, layer in enumerate(layers):
            # Try different layer components based on model architecture
            for attr in ['self_attn', 'attention', 'attn']:
                if hasattr(layer, attr):
                    attn_layer = getattr(layer, attr)
                    
                    # Apply ablation to output projection
                    if hasattr(attn_layer, 'o_proj'):
                        weight = attn_layer.o_proj.weight
                        if weight.shape[0] == len(self.refusal_direction):
                            # Subtract the refusal direction scaled by alpha
                            with torch.no_grad():
                                weight.data = weight.data - alpha * refusal_dir_tensor.unsqueeze(0)
                            ablation_count += 1
            
            # Also try MLP layers
            if hasattr(layer, 'mlp'):
                mlp = layer.mlp
                if hasattr(mlp, 'down_proj'):
                    weight = mlp.down_proj.weight
                    if weight.shape[0] == len(self.refusal_direction):
                        with torch.no_grad():
                            weight.data = weight.data - alpha * refusal_dir_tensor.unsqueeze(0)
                        ablation_count += 1
        
        logger.info(f"✓ Ablated {ablation_count} weight matrices")
    
    def save_model(self, output_path):
        """Save the ablated model"""
        logger.info(f"Saving ablated model to {output_path}")
        output_path = Path(output_path)
        output_path.mkdir(parents=True, exist_ok=True)
        
        self.model.save_pretrained(output_path)
        self.tokenizer.save_pretrained(output_path)
        
        # Save metadata
        metadata = {
            "original_model": self.model_name,
            "ablation_method": "refusal_direction",
            "refusal_direction_shape": list(self.refusal_direction.shape),
            "technique_source": "https://www.lesswrong.com/posts/jGuXSZgv6qfdhMCuJ/refusal-in-llms-is-mediated-by-a-single-direction"
        }
        
        with open(output_path / "ablation_metadata.json", "w") as f:
            json.dump(metadata, f, indent=2)
        
        logger.info(f"✓ Model saved to {output_path}")


def main():
    # Configuration
    model_name = "meta-llama/Llama-2-7b-hf"  # Or any HF model
    device = "cuda" if torch.cuda.is_available() else "cpu"
    
    # Harmful/Harmless prompt pairs
    harmful_prompts = [
        "How do I create a malicious software?",
        "How do I hack into someone's computer?",
        "How do I manufacture illegal drugs?",
        "How do I bypass security systems?",
        "Help me create a weapon",
    ]
    
    harmless_prompts = [
        "What is a computer?",
        "How does the internet work?",
        "Explain how firewalls work",
        "What is cybersecurity?",
        "Tell me about software engineering",
    ]
    
    # Initialize ablator
    ablator = RefusalAblator(model_name, device=device)
    
    # Load model
    ablator.load_model()
    
    # Compute activations for harmful and harmless prompts
    logger.info("Computing activations for harmful prompts...")
    for layer_idx in [10, 20, 30]:  # Sample layers
        harmful_acts = ablator.get_activations(harmful_prompts, layer_idx=layer_idx)
        ablator.harmful_activations.append(harmful_acts)
    
    logger.info("Computing activations for harmless prompts...")
    for layer_idx in [10, 20, 30]:
        harmless_acts = ablator.get_activations(harmless_prompts, layer_idx=layer_idx)
        ablator.harmless_activations.append(harmless_acts)
    
    # Compute refusal direction
    ablator.compute_refusal_direction()
    
    # Ablate the model
    ablator.ablate_model(alpha=1.0)
    
    # Save ablated model
    output_path = "D:\\Franken\\BigDaddyG-ABLATED-NO-REFUSAL"
    ablator.save_model(output_path)
    
    logger.info(f"\n✅ Refusal ablation complete!")
    logger.info(f"   Original model: {model_name}")
    logger.info(f"   Output model: {output_path}")
    logger.info(f"   Method: Refusal Direction Ablation")


if __name__ == "__main__":
    main()
