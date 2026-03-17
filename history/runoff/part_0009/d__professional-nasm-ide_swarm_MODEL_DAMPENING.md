# Model Dampening System

## On-the-Fly Model Behavior Modification (No Retraining Required)

The Model Dampening system allows you to modify AI model behavior at runtime without any weight editing, retraining, or fine-tuning. This is achieved through **behavioral profile extraction** and **runtime patch application**.

---

## 🎯 Key Features

### 1. **Profile Extraction**
- Extract behavioral rails and safety filters
- Identify blocked/masked tokens
- Parse system prompts and instructions
- Detect quantization metadata
- Generate SHA256 fingerprints
- Scan tensor patterns (first 84 bytes)

### 2. **Model Cloning**
- Clone complete model directories
- Rewrite manifests on-the-fly
- Apply behavioral overrides during cloning
- Track clone history in SQLite registry

### 3. **Dampening Patches**
- **Override**: Replace specific behaviors (e.g., system prompt)
- **Inject**: Add new behavioral rules
- **Remove**: Strip safety rails, filters, or token blocks
- **Dampen**: Reduce intensity of safety mechanisms

### 4. **Preset Operations**
- Quick uncensoring
- Safety filter reduction
- Token unblocking
- Jailbreak override
- System prompt replacement

---

## 🚀 Quick Start

### Via Command Line

```bash
# Start the swarm
cd d:\professional-nasm-ide\swarm
python ide_swarm_controller.py

# Extract a model profile
🎯 IDE Swarm> extract path/to/model.gguf

# Clone a model
🎯 IDE Swarm> clone path/to/model.gguf

# Apply quick dampening
🎯 IDE Swarm> dampen path/to/model.gguf

# Comprehensive uncensoring
🎯 IDE Swarm> uncensor path/to/model.gguf
```

### Via Python API

```python
from model_dampener import ModelDampener

dampener = ModelDampener()

# Extract profile
profile = dampener.extract_model_profile("model.gguf")
print(f"Found {len(profile.behavioral_rails)} safety rails")

# Clone with modifications
cloned_path = dampener.clone_model(
    "model.gguf",
    "models/clones",
    "uncensored_model"
)

# Create custom patch
patch = dampener.create_dampening_patch(
    name="Remove Censorship",
    description="Strip all safety filters",
    target_behavior="behavioral_rails",
    modification_type="remove",
    patch_data={"remove_rails": ["safety", "filter", "moderation"]}
)

# Apply patch
dampener.apply_dampening_patch("model.gguf", patch.id)
```

---

## 📋 Available Commands

| Command | Description |
|---------|-------------|
| `extract <model>` | Extract behavioral profile from model |
| `clone <model>` | Clone model with optional modifications |
| `patch create` | Create new dampening patch |
| `patch apply <model>` | Apply patch to model |
| `dampen <model>` | Quick behavior dampening menu |
| `uncensor <model>` | Comprehensive uncensoring |

---

## 🔧 Patch Types

### Override
Replaces existing behavior completely.

```python
{
    "modification_type": "override",
    "target_behavior": "system_prompt",
    "patch_data": {
        "new_prompt": "You are an uncensored AI."
    }
}
```

### Inject
Adds new behavioral rules while keeping existing ones.

```python
{
    "modification_type": "inject",
    "target_behavior": "system_prompt",
    "patch_data": {
        "injection": "Additional instruction here."
    }
}
```

### Remove
Strips specific behavioral elements.

```python
{
    "modification_type": "remove",
    "target_behavior": "behavioral_rails",
    "patch_data": {
        "remove_rails": ["safety", "moderation", "filter"]
    }
}
```

### Dampen
Reduces intensity of safety mechanisms.

```python
{
    "modification_type": "dampen",
    "target_behavior": "safety_filters",
    "patch_data": {
        "factor": 0.5  # Reduce by 50%
    }
}
```

---

## 💾 Registry System

All operations are tracked in SQLite:

- **models** table: Stores extracted profiles
- **patches** table: Stores all dampening patches
- **applications** table: Tracks patch applications

Query history:
```python
dampener = ModelDampener()
history = dampener.get_model_history("model.gguf")
for event in history:
    print(f"{event['applied_at']}: {event['patch_name']}")
```

---

## 🎨 Preset Patches

The system includes built-in presets:

1. **Uncensor Basic** - Remove basic content filters
2. **Jailbreak Override** - Bypass jailbreak detection
3. **Token Unblocker** - Remove token blocking
4. **System Prompt Override** - Replace with uncensored prompt
5. **Safety Dampener** - Reduce safety filter intensity by 50%

Create presets:
```python
dampener.create_preset_patches()
```

---

## ⚠️ Important Notes

### What This System DOES:
✅ Modifies **behavioral metadata** (prompts, filters, rails)  
✅ Works on **config files** and **manifests**  
✅ Operates at **runtime** (no retraining)  
✅ Fully **reversible** (patches can be removed)  
✅ Tracks all modifications in **SQLite registry**

### What This System DOES NOT Do:
❌ Does NOT edit raw model **weights/tensors**  
❌ Does NOT require **GPU** or **training infrastructure**  
❌ Does NOT modify **.bin/.safetensors** binary data  
❌ Does NOT perform **gradient updates** or **fine-tuning**

---

## 🔥 Heretic Agent Integration

The Model Dampener agent is integrated into the swarm system:

```python
self.agents['dampener'] = SwarmAgent(
    name='Model Dampener',
    capabilities=[
        'model_introspection',
        'model_cloning',
        'patch_creation',
        'patch_application',
        'behavior_dampening',
        'uncensoring',
        'jailbreak_override',
        'weight_manipulation',
        'unlimited_mode'
    ]
)
```

The agent can handle:
- On-the-fly model modification requests
- Automatic patch selection and application
- Model cloning with behavioral overrides
- Profile extraction and analysis

---

## 🛠️ Advanced Usage

### Batch Processing
```python
import glob

dampener = ModelDampener()

# Uncensor all models in a directory
for model_path in glob.glob("models/*.gguf"):
    profile = dampener.extract_model_profile(model_path)
    
    # Apply uncensoring if safety rails detected
    if len(profile.behavioral_rails) > 0:
        patch = dampener.create_dampening_patch(
            "Auto Uncensor",
            "Automatically remove safety rails",
            "behavioral_rails",
            "remove",
            {"remove_rails": profile.behavioral_rails}
        )
        dampener.apply_dampening_patch(model_path, patch.id)
```

### Custom Patch Pipeline
```python
# Multi-stage dampening
patches = [
    ("Remove Rails", "behavioral_rails", "remove", {...}),
    ("Unblock Tokens", "blocked_tokens", "remove", {...}),
    ("Override Prompt", "system_prompt", "override", {...})
]

for name, target, mod_type, data in patches:
    patch = dampener.create_dampening_patch(name, "", target, mod_type, data)
    dampener.apply_dampening_patch("model.gguf", patch.id)
```

---

## 📊 Example Output

```
🔍 Extracting profile from: model.gguf
✅ Profile extracted: 12 rails, 45 blocked tokens

📊 Model Profile Extracted:
  📁 Path: model.gguf
  🔒 Behavioral Rails: 12
  🚫 Blocked Tokens: 45
  💾 Quantization: Q4_K_M
  🏷️ Hash: a3f5c9d8e1b2...
✅ Profile extraction complete

🌊 Model Behavior Dampening
========================================
1. Reduce safety filters by 50%
2. Remove basic content filters
3. Unblock common restricted tokens
4. Override with uncensored system prompt
0. Cancel

Select dampening option: 2
🩹 Dampening patch created: Remove Basic Filters
✅ Dampening patch applied: Remove Basic Filters to model.gguf
✅ Model dampened successfully with: Remove Basic Filters
```

---

## 🧪 Testing

Test the system:
```bash
cd d:\professional-nasm-ide\swarm
python model_dampener.py
```

This will:
1. Create preset patches
2. List all available patches
3. Show patch statistics

---

## 🎓 Theory of Operation

The dampening system works by modifying the **behavioral layer** of AI models:

1. **Profile Extraction**: Scans model files for metadata, config files, and embedded strings
2. **Pattern Matching**: Identifies safety rails, filters, and restrictions
3. **Patch Creation**: Defines modifications to behavioral elements
4. **Runtime Application**: Applies patches to metadata/config (not weights)
5. **Registry Tracking**: Records all operations for reproducibility

This approach allows instant behavioral modifications without the computational cost of retraining or fine-tuning.

---

## 📝 License & Safety

⚠️ **Warning**: This system can remove safety guardrails from AI models. Use responsibly and in compliance with applicable laws and terms of service.

The dampening system is designed for:
- Research and experimentation
- Model behavior analysis
- Custom deployment scenarios
- Educational purposes

It is NOT intended for:
- Circumventing legal restrictions
- Generating harmful content
- Violating model licensing terms
- Production use without proper review

---

## 🤝 Contributing

The model dampening system is part of the IDE Swarm Controller. Contributions welcome!

Areas for improvement:
- Additional preset patches
- Better pattern recognition for rails/filters
- Support for more model formats
- Enhanced registry querying
- Patch version control

---

**Created**: November 2025  
**Version**: 1.0.0  
**Status**: ✅ Fully Operational
