# 🔥 WEAPONIZED AGENT.JS - Usage Guide

## Overview
120-second TTL autonomous coding agent that boots, codes, compiles, tests, and delivers merge-ready patches with zero residue.

## Quick Start
```bash
# Default holographic pulse mission
node agent.js

# Custom mission
node agent.js "your mission here"
```

## Mission Examples

### Shader Effects
```bash
node agent.js "add holographic pulse to tab edges, GL 3.3, 60 fps, < 64 instructions"
node agent.js "create animated tab edge glow with RGB cycling"
node agent.js "implement particle system in vertex shader"
node agent.js "add chromatic aberration post-process effect"
```

### Performance Optimizations
```bash
node agent.js "optimize renderer for mobile GPUs, reduce draw calls"
node agent.js "implement GPU instancing for particle rendering"
node agent.js "add level-of-detail system for complex meshes"
```

### Visual Features
```bash
node agent.js "implement real-time shadows with PCF filtering"
node agent.js "add screen-space ambient occlusion"
node agent.js "create dynamic lighting system with multiple sources"
```

## Agent Capabilities

### Input Processing
- ✅ Ingests all `.vert`, `.frag`, `.cpp`, `.h` files in `src/renderer/`
- ✅ Captures git diff state (if repository exists)
- ✅ Parses mission requirements and generates appropriate code

### Code Generation
- ✅ OpenGL 3.3+ shader generation
- ✅ C++ renderer code with modern OpenGL
- ✅ Optimized for 60fps performance
- ✅ Instruction count optimization (<64 for critical shaders)

### Output Delivery
- ✅ Ready-to-apply git diff patches
- ✅ Mission report with execution metrics
- ✅ File manifest for non-git repositories
- ✅ Zero residue - agent self-destructs after completion

## Directory Structure Expected
```
project-root/
├── agent.js              # The weaponized agent
├── CMakeLists.txt         # Build configuration
├── src/renderer/          # Target directory for modifications
│   ├── *.vert            # Vertex shaders
│   ├── *.frag            # Fragment shaders
│   ├── *.cpp             # C++ source
│   └── *.h               # Headers
└── agent_output/          # Generated after execution
    ├── mission-report.md  # Execution summary
    └── file-manifest.txt  # Modified files list
```

## Agent Lifecycle
1. **Boot (0-2s)**: Capture pristine state, ingest renderer files
2. **Mission (2-60s)**: Parse requirements, generate code patches
3. **Compile (60-90s)**: Attempt build if CMakeLists.txt exists
4. **Test (90-110s)**: Mock 120-frame performance test
5. **Deliver (110-120s)**: Generate diffs and reports
6. **Self-Destruct (120s)**: Clean residue, exit with code 0

## Mission Format Guidelines

### Effective Mission Statements
- **Specific**: "add holographic pulse to tab edges"
- **Technical**: "GL 3.3, 60 fps, <64 instructions"  
- **Actionable**: "implement", "optimize", "create", "add"

### Mission Keywords Recognized
- **Effects**: holographic, pulse, glow, shimmer, distortion
- **Performance**: 60fps, optimize, reduce, efficient
- **Graphics**: shader, vertex, fragment, lighting, shadows
- **Targets**: tab, edge, background, UI, particle

## Integration with Existing Code

### With Git Repository
```bash
# After agent execution
git apply agent_output/ready-to-apply.diff
git commit -m "Agent: implemented holographic pulse effects"
```

### Without Git
```bash
# Files are directly modified in place
# Check agent_output/file-manifest.txt for changes
```

## Performance Characteristics
- **Boot Time**: ~50ms
- **Mission Time**: ~60ms  
- **Total TTL**: 120s max
- **Memory Usage**: <10MB
- **Zero Dependencies**: Pure Node.js

## Troubleshooting

### Agent Fails to Boot
- Ensure Node.js is installed
- Check directory permissions
- Verify `src/renderer/` exists (agent creates if missing)

### No Code Generated
- Mission too vague - add specific technical requirements
- Check mission keywords match agent's pattern recognition
- Try simpler, more specific missions first

### Compilation Skipped
- Install CMake if build system is needed
- Agent works without compilation - generates code only
- Check CMakeLists.txt exists for auto-compilation

## Security Notes
- ⚠️ Agent modifies files in `src/renderer/` directory
- ✅ No network access - fully offline operation
- ✅ No system modifications outside project directory
- ✅ Self-destructs after 120 seconds maximum

---

**Agent TTL: 120s | Zero residue | Merge-ready patches**

*"Sparks waiting. Deploy, forge, deliver, die quiet."* 🖤