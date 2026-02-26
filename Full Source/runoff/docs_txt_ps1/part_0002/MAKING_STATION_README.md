# 🏭 Model/Agent Making Station

**Professional AI Factory & Laboratory**

The Model/Agent Making Station is your comprehensive toolkit for creating, configuring, and deploying AI models and autonomous agents in the RawrXD ecosystem.

---

## 🚀 Quick Start

### Launch the Making Station

```powershell
# Interactive Dashboard
.\Launch-Making-Station.ps1

# Or directly
.\scripts\model_agent_making_station.ps1
```

### Quick Create an Agent

```powershell
# Command line quick create
.\scripts\model_agent_making_station.ps1 -QuickCreate -ModelName "MyAgent" -BaseModel "Quantum"
```

---

## 🎯 Key Features

### 🔧 Creation & Configuration

1. **Create Model from Template**
   - Pre-configured architectures (1B to 70B+)
   - Micro, Compact, Standard, Power, Ultimate sizes
   - Mixture of Experts (MoE) support
   - Automatic parameter calculation

2. **Create Agent from Blueprint**
   - 8 professional agent blueprints
   - Architect, SpeedCoder, BugHunter, DeepResearcher, etc.
   - Pre-tuned temperature, top-p, and token limits
   - Personality and skill profiles

3. **Custom Creation**
   - Build models from scratch
   - Define custom agent behaviors
   - Full parameter control
   - Import existing configurations

4. **Model Management**
   - Quantization support (Q8_0, Q4_K_M, Q2_K)
   - HuggingFace model downloading
   - Local GGUF import
   - Model cloning and modification

### 📊 Agent Blueprints

| Blueprint | Role | Model | Speed | Use Case |
|-----------|------|-------|-------|----------|
| **ArchitectMaster** | System Design | Power-13B | Medium | Architecture, Planning |
| **SpeedCoder** | Rapid Coding | Standard-7B | Fast | Code Generation |
| **BugHunter** | Bug Detection | Standard-7B | Medium | Bug Fixing, QA |
| **SwarmScout** | Scanning | Micro-1B | Ultra Fast | Pattern Recognition |
| **DeepResearcher** | Analysis | Ultimate-70B | Slow | Research, Deep Dive |
| **CodeReviewer** | QA | Power-13B | Medium | Code Review |
| **OracleValidator** | Decision Making | Ultimate-70B | Slow | Final Validation |
| **ParallelWorker** | Task Execution | Compact-3B | Fast | Parallel Tasks |

### 🎨 Model Templates

| Template | Size | Layers | Context | Memory | Use Case |
|----------|------|--------|---------|--------|----------|
| **Micro-1B** | 1B | 16 | 2K | 1-2GB | Fast inference, swarm agents |
| **Compact-3B** | 3B | 24 | 4K | 2-4GB | Balanced performance |
| **Standard-7B** | 7B | 32 | 4K | 4-8GB | General purpose, coding |
| **Power-13B** | 13B | 40 | 4K | 8-16GB | Advanced reasoning |
| **Ultimate-70B** | 70B | 80 | 8K | 40-80GB | Maximum quality |
| **MoE-8x7B** | 8x7B | 32 | 32K | 20-40GB | Multi-domain expert |

---

## 📖 Usage Guide

### Creating a Model

1. **From Template** (Recommended)
   ```
   Menu > [1] Create Model from Template
   - Select template (e.g., Standard-7B)
   - Name your model
   - Choose quantization (Q4_K_M recommended)
   - Model config created!
   ```

2. **Custom Model**
   ```
   Menu > [3] Create Custom Model
   - Define all parameters manually
   - For advanced users
   ```

### Creating an Agent

1. **From Blueprint** (Recommended)
   ```
   Menu > [2] Create Agent from Blueprint
   - Select blueprint (e.g., SpeedCoder)
   - Name your agent
   - Choose model (auto-recommended)
   - Agent ready to deploy!
   ```

2. **Quick Create Wizard**
   ```
   Menu > [Q] Quick Create Wizard
   - Answer a few questions
   - Agent created in seconds
   ```

3. **Custom Agent**
   ```
   Menu > [4] Create Custom Agent
   - Define all behaviors manually
   - Full control over parameters
   ```

### Workflow Example

**Goal: Create a code review agent**

```powershell
# 1. Launch Making Station
.\Launch-Making-Station.ps1

# 2. In the dashboard, press [2] - Create Agent from Blueprint
# 3. Select [5] CodeReviewer
# 4. Name it "MyCodeReviewer"
# 5. Accept recommended model (Power-13B)
# 6. Agent created!

# 7. Press [S] to go to Swarm Control Center
# 8. Deploy your agent to the swarm
```

---

## 🔗 Integration Points

### Swarm Control Center Integration

The Making Station seamlessly integrates with the Swarm Control Center:

- Access Making Station from Swarm Control: Press `[M]`
- Access Swarm Control from Making Station: Press `[S]`
- Shared configuration files
- Agents created here are immediately available in swarms

### Configuration Files

All configurations are stored in JSON format:

```
D:\lazy init ide\logs\swarm_config\
├── models.json              # Model configurations
├── agent_presets.json       # Agent definitions
├── model_sources.json       # Model download sources
└── making_station\
    ├── model_templates.json    # Model architectures
    ├── agent_blueprints.json   # Agent behaviors
    └── training_pipelines.json # Training configs
```

### Command Line Usage

```powershell
# Quick create
.\scripts\model_agent_making_station.ps1 -QuickCreate -ModelName "FastAgent" -BaseModel "Quantum"

# Interactive dashboard (default)
.\scripts\model_agent_making_station.ps1

# Help
.\scripts\model_agent_making_station.ps1 -?
```

---

## 🎓 Best Practices

### Model Selection

- **Swarm Agents**: Use Micro-1B or Compact-3B for parallel operations
- **Coding Tasks**: Standard-7B is the sweet spot
- **Architecture/Review**: Power-13B for quality
- **Research/Oracle**: Ultimate-70B when you need maximum quality

### Agent Configuration

- **Temperature**: 
  - 0.1-0.15: Deterministic tasks (bug fixing, code generation)
  - 0.2-0.3: Balanced (review, architecture)
  - 0.4+: Creative tasks (research, brainstorming)

- **Max Tokens**:
  - 512: Quick tasks, scanning
  - 1024-2048: Standard tasks
  - 4096+: Deep analysis, documentation

### Quantization

- **Q8_0**: Best quality, larger size (~1.6x original)
- **Q4_K_M**: Best balance (recommended) (~0.5x original)
- **Q2_K**: Smallest size, quality loss (~0.25x original)

---

## 🛠️ Advanced Features (Coming Soon)

- [ ] Fine-tuning pipeline integration
- [ ] Training from scratch support
- [ ] Model benchmarking and A/B testing
- [ ] Automatic hyperparameter optimization
- [ ] Model merging and blending
- [ ] Agent behavior testing suite
- [ ] Export/Import configurations
- [ ] Batch agent creation
- [ ] Model quantization tool
- [ ] Performance profiling

---

## 📚 Architecture

### System Design

```
Making Station
├── Configuration Layer
│   ├── Model Templates (architectures)
│   ├── Agent Blueprints (behaviors)
│   └── Training Pipelines (fine-tuning)
├── Creation Layer
│   ├── Model Factory (template-based, custom)
│   ├── Agent Factory (blueprint-based, custom)
│   └── Quick Creation Wizard
├── Management Layer
│   ├── Model Registry
│   ├── Agent Registry
│   └── Configuration Storage
└── Integration Layer
    ├── Swarm Control Center
    ├── Model Sources (HuggingFace, local)
    └── Deployment Pipeline
```

### Data Flow

```
Template/Blueprint → Making Station → Configuration Files → Swarm Control → Active Agents
                              ↓
                      Model Downloads/Training
                              ↓
                         GGUF Files
```

---

## 🔍 Troubleshooting

### Agent Not Showing in Swarm

1. Check `agent_presets.json` exists in `logs/swarm_config/`
2. Verify JSON is valid (use `Get-Content ... | ConvertFrom-Json`)
3. Restart Swarm Control Center

### Model Not Loading

1. Verify GGUF path is correct
2. Check file exists and is readable
3. Ensure sufficient VRAM/RAM
4. Check quantization format is supported

### Performance Issues

1. Reduce model size (use smaller template)
2. Increase quantization (Q2_K vs Q8_0)
3. Reduce context length
4. Check system resources

---

## 📞 Quick Reference

### Keyboard Shortcuts

| Key | Action |
|-----|--------|
| `1-20` | Menu items |
| `Q` | Quick Create Wizard |
| `W` | Quick Agent from Template |
| `S` | Swarm Control Center |
| `H` | Help & Documentation |
| `X` | Exit |

### File Locations

- **Making Station**: `scripts/model_agent_making_station.ps1`
- **Swarm Control**: `scripts/swarm_control_center.ps1`
- **Config Dir**: `logs/swarm_config/`
- **Launcher**: `Launch-Making-Station.ps1`

---

## 🌟 Examples

### Example 1: Create a Fast Coding Agent

```powershell
.\Launch-Making-Station.ps1
# Press [Q] for Quick Create
# Select [1] Agent for coding tasks
# Name: "FastCoder"
# Done!
```

### Example 2: Create a Research Team

```powershell
.\Launch-Making-Station.ps1
# Create 3 agents:
# [2] Create from Blueprint → DeepResearcher → "Researcher1"
# [2] Create from Blueprint → ArchitectMaster → "Architect1"
# [2] Create from Blueprint → CodeReviewer → "Reviewer1"
# Press [S] to deploy to swarm
```

### Example 3: Custom High-Performance Agent

```powershell
.\Launch-Making-Station.ps1
# [3] Create Custom Model
# Size: 13B, Context: 8192, Quant: Q4_K_M
# [4] Create Custom Agent
# Assign your custom model
# Configure temperature, max tokens
# Test and deploy!
```

---

## 📄 License & Credits

Part of the RawrXD AI IDE ecosystem. See main LICENSE file.

---

## 🎯 Next Steps

1. **Launch the Making Station**: `.\Launch-Making-Station.ps1`
2. **Create your first agent**: Use Quick Create Wizard
3. **Deploy to swarm**: Navigate to Swarm Control Center
4. **Monitor performance**: Check swarm dashboard

**Happy Making! 🏭**
