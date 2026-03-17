#!/usr/bin/env python3
"""
Chain Controller - Integration layer for ModelChainOrchestrator with SwarmController
Provides seamless integration and high-level API for chaining models in agentic workflows
"""

import asyncio
import json
import logging
from typing import Dict, List, Optional, Any
from pathlib import Path
from datetime import datetime
from model_chain_orchestrator import (
    ModelChainOrchestrator,
    ModelChainConfig,
    ChainExecution,
    ChainableAgent,
    AgentRole,
    DEFAULT_CHAINS,
)

logger = logging.getLogger(__name__)


class ChainController:
    """High-level controller for managing model chains in the IDE"""
    
    def __init__(self, swarm_controller=None, chains_config_path: str = None):
        """Initialize chain controller"""
        self.swarm_controller = swarm_controller
        self.orchestrator = ModelChainOrchestrator(swarm_controller)
        self.chains_config_path = chains_config_path or Path(__file__).parent / "chains_config.json"
        
        # Register default chains
        for chain in DEFAULT_CHAINS.values():
            self.orchestrator.register_chain(chain)
        
        # Load custom chains if config exists
        if Path(self.chains_config_path).exists():
            self._load_custom_chains()
        
        logger.info("✓ ChainController initialized")
    
    def _load_custom_chains(self) -> None:
        """Load custom chain configurations from file"""
        try:
            with open(self.chains_config_path, 'r') as f:
                config_data = json.load(f)
            
            for chain_data in config_data.get("chains", []):
                config = ModelChainConfig(
                    chain_id=chain_data["chain_id"],
                    name=chain_data["name"],
                    description=chain_data.get("description", ""),
                    models=chain_data.get("models", []),
                    chunk_size=chain_data.get("chunk_size", 500),
                    timeout_seconds=chain_data.get("timeout_seconds", 300),
                    feedback_loops=chain_data.get("feedback_loops", 1),
                    enable_caching=chain_data.get("enable_caching", True),
                    enable_async=chain_data.get("enable_async", True),
                    priority=chain_data.get("priority", 1),
                    tags=chain_data.get("tags", [])
                )
                self.orchestrator.register_chain(config)
                logger.info(f"✓ Loaded custom chain: {chain_data['chain_id']}")
        except Exception as e:
            logger.warning(f"Could not load custom chains: {e}")
    
    def list_available_chains(self) -> List[Dict]:
        """List all available chains"""
        chains = []
        for chain_id, config in self.orchestrator.chains.items():
            chains.append({
                "chain_id": chain_id,
                "name": config.name,
                "description": config.description,
                "models": [m.get("id") for m in config.models],
                "chunk_size": config.chunk_size,
                "feedback_loops": config.feedback_loops,
                "tags": config.tags
            })
        return chains
    
    def get_chain_info(self, chain_id: str) -> Optional[Dict]:
        """Get detailed information about a chain"""
        if chain_id not in self.orchestrator.chains:
            return None
        
        config = self.orchestrator.chains[chain_id]
        return {
            "chain_id": chain_id,
            "name": config.name,
            "description": config.description,
            "models": config.models,
            "chunk_size": config.chunk_size,
            "timeout_seconds": config.timeout_seconds,
            "feedback_loops": config.feedback_loops,
            "enable_caching": config.enable_caching,
            "enable_async": config.enable_async,
            "priority": config.priority,
            "tags": config.tags
        }
    
    async def execute_chain_on_file(
        self,
        chain_id: str,
        file_path: str,
        language: str = None,
        feedback_loops: int = None
    ) -> ChainExecution:
        """Execute chain on a file"""
        # Read file
        file_path = Path(file_path)
        if not file_path.exists():
            raise FileNotFoundError(f"File not found: {file_path}")
        
        with open(file_path, 'r') as f:
            code = f.read()
        
        # Detect language from extension
        if not language:
            ext_to_lang = {
                ".py": "python",
                ".js": "javascript",
                ".ts": "typescript",
                ".java": "java",
                ".cpp": "cpp",
                ".c": "c",
                ".go": "go",
                ".rs": "rust",
                ".ps1": "powershell",
                ".asm": "nasm",
            }
            language = ext_to_lang.get(file_path.suffix, "unknown")
        
        # Override feedback loops if specified
        config = self.orchestrator.chains[chain_id]
        if feedback_loops:
            config.feedback_loops = feedback_loops
        
        logger.info(f"Processing file: {file_path.name} ({language})")
        
        # Execute chain
        return await self.orchestrator.execute_chain(
            chain_id=chain_id,
            code=code,
            language=language,
            execution_context={
                "source_file": str(file_path),
                "file_size_bytes": file_path.stat().st_size
            }
        )
    
    async def execute_chain_on_code(
        self,
        chain_id: str,
        code: str,
        language: str = "unknown",
        task_context: Dict = None
    ) -> ChainExecution:
        """Execute chain on raw code string"""
        if chain_id not in self.orchestrator.chains:
            raise ValueError(f"Chain '{chain_id}' not found")
        
        context = {
            "code_size_lines": len(code.split('\n')),
            **(task_context or {})
        }
        
        return await self.orchestrator.execute_chain(
            chain_id=chain_id,
            code=code,
            language=language,
            execution_context=context
        )
    
    def create_custom_chain(
        self,
        chain_id: str,
        name: str,
        agent_roles: List[str],
        chunk_size: int = 500,
        feedback_loops: int = 1,
        tags: List[str] = None,
        save_to_config: bool = True
    ) -> ModelChainConfig:
        """Create a new custom chain"""
        models = [
            {"id": f"agent_{role}", "role": role.lower()}
            for role in agent_roles
        ]
        
        config = ModelChainConfig(
            chain_id=chain_id,
            name=name,
            description=f"Custom chain: {' → '.join(agent_roles)}",
            models=models,
            chunk_size=chunk_size,
            timeout_seconds=300 * len(agent_roles),
            feedback_loops=feedback_loops,
            tags=tags or []
        )
        
        self.orchestrator.register_chain(config)
        
        if save_to_config:
            self._save_custom_chain(config)
        
        logger.info(f"✓ Created custom chain: {chain_id}")
        return config
    
    def _save_custom_chain(self, config: ModelChainConfig) -> None:
        """Save custom chain to config file"""
        self.chains_config_path = Path(self.chains_config_path)
        self.chains_config_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Load existing config or create new
        if self.chains_config_path.exists():
            with open(self.chains_config_path, 'r') as f:
                data = json.load(f)
        else:
            data = {"chains": []}
        
        # Add/update chain
        chain_data = config.to_dict()
        existing_idx = next(
            (i for i, c in enumerate(data["chains"]) if c["chain_id"] == config.chain_id),
            -1
        )
        
        if existing_idx >= 0:
            data["chains"][existing_idx] = chain_data
        else:
            data["chains"].append(chain_data)
        
        # Save
        with open(self.chains_config_path, 'w') as f:
            json.dump(data, f, indent=2)
        
        logger.info(f"✓ Saved chain to {self.chains_config_path}")
    
    def get_execution_report(self, execution_id: str) -> Dict:
        """Get detailed report for a chain execution"""
        return self.orchestrator.get_execution_report(execution_id)
    
    def export_report(self, execution_id: str, output_path: str) -> None:
        """Export execution report"""
        self.orchestrator.export_execution_report(execution_id, output_path)


class QuickChainExecutor:
    """Simplified interface for common chaining patterns"""
    
    def __init__(self, controller: ChainController):
        self.controller = controller
    
    async def review_and_optimize(self, code: str, language: str = "python") -> Dict:
        """Quick review + optimization chain"""
        execution = await self.controller.execute_chain_on_code(
            "code_review_chain",
            code,
            language
        )
        return self.controller.get_execution_report(execution.execution_id)
    
    async def secure_code(self, code: str, language: str = "python") -> Dict:
        """Quick security + debugging chain"""
        execution = await self.controller.execute_chain_on_code(
            "secure_coding_chain",
            code,
            language
        )
        return self.controller.get_execution_report(execution.execution_id)
    
    async def document_code(self, code: str, language: str = "python") -> Dict:
        """Quick documentation chain"""
        execution = await self.controller.execute_chain_on_code(
            "documentation_chain",
            code,
            language
        )
        return self.controller.get_execution_report(execution.execution_id)
    
    async def optimize_performance(self, code: str, language: str = "python") -> Dict:
        """Quick performance optimization chain"""
        execution = await self.controller.execute_chain_on_code(
            "optimization_chain",
            code,
            language
        )
        return self.controller.get_execution_report(execution.execution_id)


# Convenience functions for quick access
_global_controller: Optional[ChainController] = None


def initialize_chain_controller(swarm_controller=None) -> ChainController:
    """Initialize global chain controller"""
    global _global_controller
    _global_controller = ChainController(swarm_controller)
    return _global_controller


def get_chain_controller() -> ChainController:
    """Get global chain controller"""
    global _global_controller
    if _global_controller is None:
        _global_controller = ChainController()
    return _global_controller


async def quick_chain_review_optimize(code: str) -> Dict:
    """Quick shortcut for review + optimization"""
    executor = QuickChainExecutor(get_chain_controller())
    return await executor.review_and_optimize(code)


async def quick_chain_secure(code: str) -> Dict:
    """Quick shortcut for security checks"""
    executor = QuickChainExecutor(get_chain_controller())
    return await executor.secure_code(code)


async def quick_chain_document(code: str) -> Dict:
    """Quick shortcut for documentation"""
    executor = QuickChainExecutor(get_chain_controller())
    return await executor.document_code(code)


if __name__ == "__main__":
    # Example usage
    async def demo():
        controller = ChainController()
        
        # List available chains
        print("\n📋 Available Chains:")
        print("=" * 60)
        for chain in controller.list_available_chains():
            print(f"  {chain['chain_id']:30} - {chain['name']}")
        
        # Create custom chain
        custom_config = controller.create_custom_chain(
            chain_id="custom_validation",
            name="Custom Validation Chain",
            agent_roles=["Analyzer", "Validator", "Security"],
            feedback_loops=2,
            tags=["custom", "validation"]
        )
        
        print(f"\n✓ Created custom chain: {custom_config.chain_id}")
        
        # Example code
        example_code = "def hello():\n    print('world')\n" * 100
        
        # Execute chain
        print("\n🚀 Executing code_review_chain...")
        execution = await controller.execute_chain_on_code(
            "code_review_chain",
            example_code,
            language="python"
        )
        
        # Get report
        report = controller.get_execution_report(execution.execution_id)
        print(f"\n📊 Execution Report:")
        print(f"  Status: {report['status']}")
        print(f"  Duration: {report['duration_seconds']:.2f}s")
        print(f"  Success Rate: {report['success_rate']}")
        print(f"  Results: {report['results_count']}")
    
    asyncio.run(demo())
