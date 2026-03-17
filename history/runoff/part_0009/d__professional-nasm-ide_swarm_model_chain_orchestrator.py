#!/usr/bin/env python3
"""
Model Chain Orchestrator - Advanced Agent Chaining System
Enables cyclic chaining of multiple models/agents for agentic coding tasks
Each model processes ~500 lines before passing to the next model in the chain
Supports specialized agent sequences and feedback loops
"""

import asyncio
import json
import logging
import time
from typing import Dict, List, Any, Optional, Tuple
from dataclasses import dataclass, field, asdict
from enum import Enum
from datetime import datetime
from pathlib import Path
import hashlib
from concurrent.futures import ThreadPoolExecutor

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class AgentRole(Enum):
    """Specialized roles for agents in the chain"""
    ANALYZER = "analyzer"           # Analyzes code structure & patterns
    GENERATOR = "generator"         # Generates code solutions
    VALIDATOR = "validator"         # Validates correctness
    OPTIMIZER = "optimizer"         # Optimizes performance
    DOCUMENTER = "documenter"       # Generates documentation
    DEBUGGER = "debugger"           # Finds and fixes bugs
    SECURITY = "security"           # Security analysis
    REVIEWER = "reviewer"            # Code review & suggestions
    FORMATTER = "formatter"         # Code formatting & style
    ARCHITECT = "architect"         # High-level architecture decisions


class ChainPhase(Enum):
    """Phases in the model chain execution"""
    INITIALIZATION = "init"
    ANALYSIS = "analyze"
    PROCESSING = "process"
    VALIDATION = "validate"
    OPTIMIZATION = "optimize"
    FINALIZATION = "finalize"
    COMPLETION = "complete"


@dataclass
class CodeChunk:
    """Represents a 500-line chunk of code"""
    id: str
    chunk_number: int
    start_line: int
    end_line: int
    content: str
    language: str = "unknown"
    metadata: Dict[str, Any] = field(default_factory=dict)
    
    def size_lines(self) -> int:
        """Get chunk size in lines"""
        return len(self.content.split('\n'))
    
    def hash(self) -> str:
        """Get hash of chunk content"""
        return hashlib.sha256(self.content.encode()).hexdigest()[:16]


@dataclass
class ChainResult:
    """Result from a single model in the chain"""
    model_id: str
    agent_role: AgentRole
    phase: ChainPhase
    chunk_id: str
    chunk_number: int
    processed_content: str
    analysis: Dict[str, Any]
    metrics: Dict[str, Any]
    timestamp: str
    execution_time_ms: float
    success: bool
    error: Optional[str] = None
    suggestions: List[str] = field(default_factory=list)
    
    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "model_id": self.model_id,
            "agent_role": self.agent_role.value,
            "phase": self.phase.value,
            "chunk_id": self.chunk_id,
            "chunk_number": self.chunk_number,
            "processed_content": self.processed_content,
            "analysis": self.analysis,
            "metrics": self.metrics,
            "timestamp": self.timestamp,
            "execution_time_ms": self.execution_time_ms,
            "success": self.success,
            "error": self.error,
            "suggestions": self.suggestions
        }


@dataclass
class ModelChainConfig:
    """Configuration for a model chain"""
    chain_id: str
    name: str
    description: str
    models: List[Dict[str, Any]]              # [{"id": "code_analysis", "role": "analyzer", ...}]
    chunk_size: int = 500                     # Lines per chunk
    timeout_seconds: int = 300                # Per model timeout
    feedback_loops: int = 1                   # Number of feedback iterations
    enable_caching: bool = True
    enable_async: bool = True
    priority: int = 1
    tags: List[str] = field(default_factory=list)
    
    def to_dict(self) -> Dict:
        return asdict(self)


@dataclass
class ChainExecution:
    """Tracks execution of a model chain"""
    execution_id: str
    chain_id: str
    start_time: datetime
    end_time: Optional[datetime] = None
    total_chunks: int = 0
    processed_chunks: int = 0
    failed_chunks: int = 0
    results: List[ChainResult] = field(default_factory=list)
    status: str = "running"  # running, completed, failed
    error: Optional[str] = None
    total_execution_time_ms: float = 0.0
    
    def duration_seconds(self) -> float:
        """Get execution duration in seconds"""
        if self.end_time:
            return (self.end_time - self.start_time).total_seconds()
        return (datetime.now() - self.start_time).total_seconds()
    
    def success_rate(self) -> float:
        """Get percentage of successful chunks"""
        if self.total_chunks == 0:
            return 0.0
        return (self.processed_chunks / self.total_chunks) * 100


class ChainableAgent:
    """Wraps an individual agent/model for use in a chain"""
    
    def __init__(
        self,
        agent_id: str,
        agent_role: AgentRole,
        model_spec: Dict[str, Any],
        swarm_controller: Optional[Any] = None
    ):
        self.agent_id = agent_id
        self.agent_role = agent_role
        self.model_spec = model_spec
        self.swarm_controller = swarm_controller
        self.state: Dict[str, Any] = {
            "ready": True,
            "processing": False,
            "tasks_completed": 0,
            "total_execution_time_ms": 0.0,
            "last_error": None
        }
    
    async def process_chunk(
        self,
        chunk: CodeChunk,
        phase: ChainPhase,
        context: Dict[str, Any] = None
    ) -> ChainResult:
        """Process a single code chunk"""
        start_time = time.time()
        self.state["processing"] = True
        
        try:
            # Build specialized prompt based on agent role
            prompt = self._build_prompt(chunk, phase, context or {})
            
            # Call the underlying model via swarm controller or direct
            if self.swarm_controller:
                result_content, analysis = await self._call_swarm_model(
                    prompt, chunk, self.agent_role
                )
            else:
                # Direct model call simulation
                result_content, analysis = await self._call_direct_model(
                    prompt, chunk
                )
            
            execution_time_ms = (time.time() - start_time) * 1000
            self.state["tasks_completed"] += 1
            self.state["total_execution_time_ms"] += execution_time_ms
            
            result = ChainResult(
                model_id=self.agent_id,
                agent_role=self.agent_role,
                phase=phase,
                chunk_id=chunk.id,
                chunk_number=chunk.chunk_number,
                processed_content=result_content,
                analysis=analysis,
                metrics={
                    "chunk_size": chunk.size_lines(),
                    "processing_speed_lines_per_sec": chunk.size_lines() / (execution_time_ms / 1000)
                },
                timestamp=datetime.now().isoformat(),
                execution_time_ms=execution_time_ms,
                success=True,
                suggestions=self._extract_suggestions(analysis)
            )
            
            logger.info(
                f"✓ {self.agent_id} ({self.agent_role.value}) processed chunk {chunk.chunk_number} "
                f"in {execution_time_ms:.1f}ms"
            )
            return result
            
        except Exception as e:
            execution_time_ms = (time.time() - start_time) * 1000
            self.state["last_error"] = str(e)
            error_msg = f"Error in {self.agent_id}: {str(e)}"
            logger.error(error_msg)
            
            return ChainResult(
                model_id=self.agent_id,
                agent_role=self.agent_role,
                phase=phase,
                chunk_id=chunk.id,
                chunk_number=chunk.chunk_number,
                processed_content="",
                analysis={},
                metrics={},
                timestamp=datetime.now().isoformat(),
                execution_time_ms=execution_time_ms,
                success=False,
                error=error_msg
            )
        finally:
            self.state["processing"] = False
    
    def _build_prompt(
        self,
        chunk: CodeChunk,
        phase: ChainPhase,
        context: Dict[str, Any]
    ) -> str:
        """Build specialized prompt based on agent role"""
        base_prompt = f"""# Code Analysis Task
Language: {chunk.language}
Chunk: {chunk.chunk_number}
Lines: {chunk.start_line}-{chunk.end_line}
Phase: {phase.value}

## Code:
```{chunk.language if chunk.language != 'unknown' else ''}
{chunk.content}
```

"""
        
        role_prompts = {
            AgentRole.ANALYZER: """## Task: Analyze this code chunk
- Identify patterns and structure
- List key functions/classes
- Note complexity markers
- Flag potential issues
Return structured analysis with headings for each finding.""",
            
            AgentRole.GENERATOR: """## Task: Enhance/Generate code
- Add missing functionality
- Improve code patterns
- Generate helper functions if needed
- Provide working examples
Return complete, runnable code.""",
            
            AgentRole.VALIDATOR: """## Task: Validate code correctness
- Check for syntax errors
- Verify logic flow
- Test edge cases
- Validate against best practices
Return validation report with any issues found.""",
            
            AgentRole.OPTIMIZER: """## Task: Optimize this code
- Identify performance bottlenecks
- Suggest algorithmic improvements
- Optimize memory usage
- Add caching/memoization where applicable
Return optimized version with benchmarks.""",
            
            AgentRole.DOCUMENTER: """## Task: Generate documentation
- Create clear code comments
- Write docstrings
- Generate usage examples
- Document edge cases and assumptions
Return well-documented version.""",
            
            AgentRole.DEBUGGER: """## Task: Debug and fix issues
- Identify bugs and issues
- Trace execution flow
- Provide step-by-step fixes
- Add error handling
Return fixed version with bug report.""",
            
            AgentRole.SECURITY: """## Task: Security analysis
- Identify security vulnerabilities
- Check for injection risks
- Validate input handling
- Suggest security improvements
Return security report with recommendations.""",
            
            AgentRole.REVIEWER: """## Task: Code review
- Evaluate code quality
- Check naming conventions
- Suggest improvements
- Rate readability and maintainability
Return detailed review with specific suggestions.""",
            
            AgentRole.FORMATTER: """## Task: Format and style
- Apply consistent formatting
- Follow language conventions
- Improve readability
- Organize imports/dependencies
Return formatted, styled code.""",
            
            AgentRole.ARCHITECT: """## Task: Architecture review
- Evaluate design patterns
- Check modularity
- Suggest structural improvements
- Consider scalability
Return architecture recommendations.""",
        }
        
        role_prompt = role_prompts.get(self.agent_role, "Analyze and process this code chunk.")
        
        # Add context if available
        if context and context.get("previous_results"):
            base_prompt += f"\n## Previous Results:\n{context['previous_results']}\n"
        
        if context and context.get("task_goal"):
            base_prompt += f"\n## Task Goal:\n{context['task_goal']}\n"
        
        base_prompt += f"\n{role_prompt}"
        return base_prompt
    
    async def _call_swarm_model(
        self,
        prompt: str,
        chunk: CodeChunk,
        role: AgentRole
    ) -> Tuple[str, Dict]:
        """Call model through swarm controller"""
        try:
            # Submit task to swarm
            from swarm_controller import Task
            
            task = Task(
                id=f"{self.agent_id}_{chunk.id}",
                type=f"code_{role.value}",
                data={"prompt": prompt, "chunk": chunk.id}
            )
            
            # In real implementation, this would call swarm_controller.submit_task
            # For now, return simulated response
            result = {
                "processed_content": f"# Processed by {self.agent_id}\n{chunk.content}",
                "analysis": {
                    "role": role.value,
                    "chunk_size": chunk.size_lines(),
                    "findings": []
                }
            }
            return result["processed_content"], result["analysis"]
        except Exception as e:
            logger.error(f"Swarm model call failed: {e}")
            raise
    
    async def _call_direct_model(
        self,
        prompt: str,
        chunk: CodeChunk
    ) -> Tuple[str, Dict]:
        """Direct model call (simulation)"""
        await asyncio.sleep(0.1)  # Simulate processing
        
        analysis = {
            "role": self.agent_role.value,
            "chunk_size": chunk.size_lines(),
            "findings": [
                "Code structure identified",
                f"Processing {chunk.size_lines()} lines of {chunk.language} code"
            ]
        }
        
        return chunk.content, analysis
    
    def _extract_suggestions(self, analysis: Dict) -> List[str]:
        """Extract suggestions from analysis"""
        suggestions = []
        if "suggestions" in analysis:
            suggestions = analysis["suggestions"]
        elif "findings" in analysis:
            suggestions = analysis["findings"][:3]  # Top 3 findings
        return suggestions


class ModelChainOrchestrator:
    """Main orchestrator for executing model chains"""
    
    def __init__(self, swarm_controller: Optional[Any] = None):
        self.swarm_controller = swarm_controller
        self.chains: Dict[str, ModelChainConfig] = {}
        self.executions: Dict[str, ChainExecution] = {}
        self.agents: Dict[str, ChainableAgent] = {}
        self.executor = ThreadPoolExecutor(max_workers=5)
        self.cache: Dict[str, ChainResult] = {}
        logger.info("ModelChainOrchestrator initialized")
    
    def register_chain(self, config: ModelChainConfig) -> None:
        """Register a new model chain configuration"""
        self.chains[config.chain_id] = config
        logger.info(f"✓ Registered chain: {config.name} ({config.chain_id})")
        
        # Create chainable agents for this chain
        for model_spec in config.models:
            agent_id = model_spec.get("id", "unknown")
            role_name = model_spec.get("role", "analyzer")
            
            try:
                role = AgentRole[role_name.upper()]
            except KeyError:
                role = AgentRole.ANALYZER
            
            agent = ChainableAgent(
                agent_id=agent_id,
                agent_role=role,
                model_spec=model_spec,
                swarm_controller=self.swarm_controller
            )
            self.agents[agent_id] = agent
    
    def split_into_chunks(
        self,
        code: str,
        language: str = "unknown",
        chunk_size: int = 500,
        metadata: Dict = None
    ) -> List[CodeChunk]:
        """Split code into ~500 line chunks"""
        lines = code.split('\n')
        chunks = []
        chunk_num = 0
        
        for i in range(0, len(lines), chunk_size):
            chunk_num += 1
            chunk_lines = lines[i:i + chunk_size]
            chunk_content = '\n'.join(chunk_lines)
            
            chunk = CodeChunk(
                id=f"chunk_{chunk_num:04d}_{hashlib.md5(chunk_content.encode()).hexdigest()[:8]}",
                chunk_number=chunk_num,
                start_line=i + 1,
                end_line=min(i + chunk_size, len(lines)),
                content=chunk_content,
                language=language,
                metadata=metadata or {}
            )
            chunks.append(chunk)
        
        logger.info(f"Split code into {len(chunks)} chunks ({chunk_size} lines each)")
        return chunks
    
    async def execute_chain(
        self,
        chain_id: str,
        code: str,
        language: str = "unknown",
        execution_context: Dict = None,
        use_feedback_loops: bool = True
    ) -> ChainExecution:
        """Execute a registered model chain on code"""
        
        if chain_id not in self.chains:
            raise ValueError(f"Chain {chain_id} not found")
        
        config = self.chains[chain_id]
        execution_id = f"exec_{chain_id}_{int(time.time() * 1000)}"
        
        # Initialize execution tracker
        execution = ChainExecution(
            execution_id=execution_id,
            chain_id=chain_id,
            start_time=datetime.now()
        )
        
        # Split code into chunks
        chunks = self.split_into_chunks(code, language, config.chunk_size)
        execution.total_chunks = len(chunks)
        
        logger.info(f"\n🚀 Starting chain execution: {config.name}")
        logger.info(f"   Chunks: {len(chunks)} × {config.chunk_size} lines")
        logger.info(f"   Models: {len(config.models)}")
        logger.info(f"   Feedback loops: {config.feedback_loops}")
        
        try:
            context = execution_context or {"task_goal": f"Process {len(chunks)} code chunks"}
            
            # Execute chain for each chunk
            for feedback_loop in range(config.feedback_loops):
                logger.info(f"\n📍 Feedback Loop {feedback_loop + 1}/{config.feedback_loops}")
                
                for chunk in chunks:
                    # Cycle through agents
                    for model_spec in config.models:
                        agent_id = model_spec.get("id")
                        if agent_id not in self.agents:
                            continue
                        
                        agent = self.agents[agent_id]
                        
                        # Determine phase based on model order
                        model_index = config.models.index(model_spec)
                        if model_index == 0:
                            phase = ChainPhase.ANALYSIS
                        elif model_index == len(config.models) - 1:
                            phase = ChainPhase.FINALIZATION
                        else:
                            phase = ChainPhase.PROCESSING
                        
                        # Process chunk through agent
                        if config.enable_async:
                            result = await agent.process_chunk(chunk, phase, context)
                        else:
                            result = await asyncio.to_thread(
                                agent.process_chunk,
                                chunk,
                                phase,
                                context
                            )
                        
                        # Track result
                        execution.results.append(result)
                        if result.success:
                            execution.processed_chunks += 1
                        else:
                            execution.failed_chunks += 1
                        
                        # Update context with results
                        context["previous_results"] = "\n".join([
                            s for s in result.suggestions
                        ])
            
            # Mark execution as complete
            execution.end_time = datetime.now()
            execution.status = "completed"
            execution.total_execution_time_ms = execution.duration_seconds() * 1000
            
            self.executions[execution_id] = execution
            
            # Log summary
            self._log_execution_summary(execution, config)
            
            return execution
            
        except Exception as e:
            execution.status = "failed"
            execution.error = str(e)
            execution.end_time = datetime.now()
            logger.error(f"Chain execution failed: {e}")
            return execution
    
    def _log_execution_summary(self, execution: ChainExecution, config: ModelChainConfig):
        """Log execution summary"""
        logger.info(f"\n{'='*60}")
        logger.info(f"Chain Execution Summary: {config.name}")
        logger.info(f"{'='*60}")
        logger.info(f"  Status: {execution.status}")
        logger.info(f"  Duration: {execution.duration_seconds():.2f}s")
        logger.info(f"  Chunks: {execution.processed_chunks}/{execution.total_chunks} "
                   f"({execution.success_rate():.1f}%)")
        logger.info(f"  Total Results: {len(execution.results)}")
        
        # Agent performance summary
        agent_stats = {}
        for result in execution.results:
            if result.model_id not in agent_stats:
                agent_stats[result.model_id] = {"count": 0, "total_time": 0, "failures": 0}
            agent_stats[result.model_id]["count"] += 1
            agent_stats[result.model_id]["total_time"] += result.execution_time_ms
            if not result.success:
                agent_stats[result.model_id]["failures"] += 1
        
        logger.info(f"\n  Agent Performance:")
        for agent_id, stats in agent_stats.items():
            avg_time = stats["total_time"] / stats["count"] if stats["count"] > 0 else 0
            logger.info(f"    {agent_id}: {stats['count']} tasks, "
                       f"avg {avg_time:.1f}ms, {stats['failures']} failures")
        
        logger.info(f"{'='*60}\n")
    
    def get_execution_report(self, execution_id: str) -> Dict:
        """Get detailed report for an execution"""
        if execution_id not in self.executions:
            return {"error": "Execution not found"}
        
        execution = self.executions[execution_id]
        return {
            "execution_id": execution_id,
            "chain_id": execution.chain_id,
            "status": execution.status,
            "duration_seconds": execution.duration_seconds(),
            "total_chunks": execution.total_chunks,
            "processed_chunks": execution.processed_chunks,
            "failed_chunks": execution.failed_chunks,
            "success_rate": f"{execution.success_rate():.1f}%",
            "results_count": len(execution.results),
            "results": [r.to_dict() for r in execution.results],
            "error": execution.error
        }
    
    def export_execution_report(self, execution_id: str, output_path: str) -> None:
        """Export execution report to JSON file"""
        report = self.get_execution_report(execution_id)
        
        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)
        
        with open(output_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        logger.info(f"✓ Report exported to {output_file}")


# Predefined chain configurations
DEFAULT_CHAINS = {
    "code_review_chain": ModelChainConfig(
        chain_id="code_review_chain",
        name="Code Review Chain",
        description="Multi-stage code review: analyze → validate → optimize → review",
        models=[
            {"id": "analyzer", "role": "analyzer"},
            {"id": "validator", "role": "validator"},
            {"id": "optimizer", "role": "optimizer"},
            {"id": "reviewer", "role": "reviewer"}
        ],
        chunk_size=500,
        timeout_seconds=300,
        feedback_loops=1,
        tags=["review", "coding", "quality"]
    ),
    
    "secure_coding_chain": ModelChainConfig(
        chain_id="secure_coding_chain",
        name="Secure Coding Chain",
        description="Security-focused chain: analyze → security check → optimize",
        models=[
            {"id": "analyzer", "role": "analyzer"},
            {"id": "security", "role": "security"},
            {"id": "debugger", "role": "debugger"},
            {"id": "optimizer", "role": "optimizer"}
        ],
        chunk_size=500,
        timeout_seconds=600,
        feedback_loops=2,
        tags=["security", "coding"]
    ),
    
    "documentation_chain": ModelChainConfig(
        chain_id="documentation_chain",
        name="Documentation Chain",
        description="Generate comprehensive docs: analyze → document → review",
        models=[
            {"id": "analyzer", "role": "analyzer"},
            {"id": "documenter", "role": "documenter"},
            {"id": "formatter", "role": "formatter"}
        ],
        chunk_size=500,
        timeout_seconds=300,
        feedback_loops=1,
        tags=["documentation", "coding"]
    ),
    
    "optimization_chain": ModelChainConfig(
        chain_id="optimization_chain",
        name="Performance Optimization Chain",
        description="Optimize code: analyze → optimize → validate",
        models=[
            {"id": "analyzer", "role": "analyzer"},
            {"id": "optimizer", "role": "optimizer"},
            {"id": "validator", "role": "validator"}
        ],
        chunk_size=500,
        timeout_seconds=400,
        feedback_loops=1,
        tags=["performance", "optimization"]
    ),
}


async def main():
    """Example usage"""
    # Create orchestrator
    orchestrator = ModelChainOrchestrator()
    
    # Register default chains
    for chain in DEFAULT_CHAINS.values():
        orchestrator.register_chain(chain)
    
    # Example code to process
    example_code = """
def calculate_fibonacci(n):
    '''Calculate fibonacci number'''
    if n <= 1:
        return n
    return calculate_fibonacci(n-1) + calculate_fibonacci(n-2)

def sort_array(arr):
    '''Sort array'''
    for i in range(len(arr)):
        for j in range(len(arr)-1):
            if arr[j] > arr[j+1]:
                arr[j], arr[j+1] = arr[j+1], arr[j]
    return arr

def search_in_list(lst, target):
    '''Search for item in list'''
    for item in lst:
        if item == target:
            return True
    return False
""" * 50  # Repeat to create more lines
    
    # Execute chain
    execution = await orchestrator.execute_chain(
        chain_id="code_review_chain",
        code=example_code,
        language="python",
        execution_context={"task_goal": "Review and optimize Python code"}
    )
    
    # Export report
    orchestrator.export_execution_report(
        execution.execution_id,
        "chain_execution_report.json"
    )
    
    print(f"\n✓ Chain execution complete: {execution.execution_id}")


if __name__ == "__main__":
    asyncio.run(main())
