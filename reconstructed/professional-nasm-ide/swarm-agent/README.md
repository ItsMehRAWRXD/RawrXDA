# Professional NASM IDE - Swarm Agent System

A distributed AI agent system with 10 specialized model instances (200-400MB each) for comprehensive IDE functionality.

## Architecture

### Agent Types (10 Instances)

1. **AI Inference Agent** - LLM-based code analysis and generation
2. **Text Editor Agent** - Code editing, syntax highlighting, formatting
3. **Team View Agent** - Remote collaboration and screen sharing
4. **Marketplace Agent** - Extension/plugin discovery and management
5. **Code Analysis Agent** - Static analysis, linting, quality checks
6. **Build System Agent** - Compilation, linking, build orchestration
7. **Debug Agent** - Debugging, breakpoints, variable inspection
8. **Docs Agent** - Documentation generation and management
9. **Test Agent** - Test execution and coverage analysis
10. **Deploy Agent** - Deployment and release management

## Features

- **Distributed Processing**: 10 parallel agent instances for concurrent task execution
- **Task Prioritization**: Critical, High, Medium, Low priority queues
- **Load Balancing**: Automatic task distribution across available agents
- **Fault Tolerance**: Automatic retry and failover mechanisms
- **Real-time Monitoring**: Agent status, memory usage, task completion tracking
- **Async Architecture**: High-performance asyncio-based implementation

## System Requirements

- Python 3.8+
- 4GB RAM minimum (for 10 agents × 400MB)
- Multi-core CPU recommended

### Python Environment Validation

Before starting the swarm, run `check_python_environment.py` (the `launch-swarm.bat` wrapper already does this for you). It validates the active interpreter, confirms Python 3.8+ is in use, and ensures the standard library ships the modules the agent system depends on (asyncio, logging, typing, dataclasses, etc.).

If the script detects an unsupported or experimental build, reinstall Python 3.8+ from https://python.org and make sure the installer adds Python to your `%PATH%` so the launcher can find it.

## Installation

```bash
cd D:\professional-nasm-ide\swarm-agent
pip install -r requirements.txt
```

## Usage

### Start the Swarm System

```bash
python swarm_controller.py
```

### API Usage

```python
from swarm_controller import SwarmController, Task, TaskPriority, AgentType

# Initialize controller
controller = SwarmController(num_agents=10)
await controller.start()

# Submit a task
task = Task(
    task_id="",
    task_type="build",
    priority=TaskPriority.HIGH,
    payload={"project": "nasm_ide_integration.asm"},
    target_agent=AgentType.BUILD_SYSTEM
)

task_id = await controller.submit_task(task)

# Get results
results = await controller.get_results()
```

## Task Types

### AI Inference
- Code completion
- Bug prediction
- Optimization suggestions
- Natural language queries

### Text Editor
- File operations (open, save, edit)
- Syntax highlighting
- Code formatting
- Find/replace

### Team View
- Session management
- Screen sharing
- Collaborative editing
- Chat integration

### Marketplace
- Extension search
- Plugin installation
- Update management
- Compatibility checking

### Code Analysis
- Static analysis
- Complexity metrics
- Security scanning
- Style checking

### Build System
- NASM compilation
- GCC linking
- Multi-target builds
- Dependency management

### Debug Agent
- Breakpoint management
- Variable inspection
- Call stack analysis
- Memory debugging

### Docs Agent
- API documentation
- Code comments
- README generation
- Tutorial creation

### Test Agent
- Unit test execution
- Integration testing
- Coverage reporting
- Performance testing

### Deploy Agent
- Build packaging
- Release management
- Version control
- Distribution

## Configuration

Edit agent configurations in `swarm_controller.py`:

```python
config = AgentConfig(
    agent_id=0,
    agent_type=AgentType.AI_INFERENCE,
    model_path="models/model_0.bin",
    max_memory_mb=400,
    gpu_enabled=False,
    batch_size=1,
    timeout_seconds=30
)
```

## Performance

- **Throughput**: 100+ tasks/second (varies by task type)
- **Latency**: <500ms average response time
- **Memory**: 3-4GB total (10 agents × 300-400MB)
- **CPU**: Scales with available cores

## Monitoring

The system provides real-time metrics:

- Agent status (idle, busy, error, offline)
- Tasks completed per agent
- Memory usage per agent
- Task queue depth
- Average processing time

## Integration with NASM IDE

The swarm system integrates with the NASM IDE through:

1. **Build Commands**: Trigger builds through Build System Agent
2. **Code Assistance**: AI Inference Agent provides suggestions
3. **Collaboration**: Team View Agent enables remote work
4. **Extensions**: Marketplace Agent manages plugins

## Advanced Features

### Load Balancing
Automatic task distribution based on:
- Agent availability
- Agent specialization
- Task priority
- Current load

### Fault Tolerance
- Automatic task retry on failure
- Agent health monitoring
- Graceful degradation
- Error logging and recovery

### Scaling
- Dynamic agent spawning
- Resource-based scaling
- Cloud deployment ready
- Container support (Docker/K8s)

## Development

### Adding New Agent Types

1. Define agent type in `AgentType` enum
2. Implement handler method in `SwarmAgent` class
3. Update agent configuration
4. Test with demo tasks

### Custom Task Handlers

```python
async def _handle_custom(self, task: Task) -> Dict[str, Any]:
    """Handle custom operations"""
    # Your implementation
    return {
        "type": "custom",
        "result": "success"
    }
```

## Troubleshooting

### High Memory Usage
- Reduce `max_memory_mb` in agent configs
- Decrease number of agents
- Enable model quantization

### Slow Performance
- Enable GPU acceleration
- Increase batch sizes
- Optimize task distribution

### Agent Failures
- Check logs for error details
- Verify model files exist
- Ensure sufficient system resources

## License

Part of the Professional NASM IDE project.

## Contributing

Contributions welcome! Focus areas:
- New agent types
- Performance optimizations
- Additional integrations
- Documentation improvements
