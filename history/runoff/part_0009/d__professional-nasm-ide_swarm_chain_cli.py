#!/usr/bin/env python3
"""
Chain CLI - Command-line interface for model chaining
Provides easy access to model chaining features from terminal/scripts
"""

import argparse
import asyncio
import json
import sys
from pathlib import Path
from typing import Optional
from chain_controller import ChainController, QuickChainExecutor
from model_chain_orchestrator import AgentRole

import logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class ChainCLI:
    """Command-line interface for chain operations"""
    
    def __init__(self):
        self.controller = ChainController()
        self.executor = QuickChainExecutor(self.controller)
    
    async def list_chains(self) -> int:
        """List available chains"""
        chains = self.controller.list_available_chains()
        
        print("\n" + "="*80)
        print("📋 AVAILABLE MODEL CHAINS".center(80))
        print("="*80)
        
        for i, chain in enumerate(chains, 1):
            print(f"\n{i}. {chain['name']} ({chain['chain_id']})")
            print(f"   Description: {chain['description']}")
            print(f"   Models: {' → '.join(chain['models'])}")
            print(f"   Chunk Size: {chain['chunk_size']} lines")
            print(f"   Feedback Loops: {chain['feedback_loops']}")
            if chain['tags']:
                print(f"   Tags: {', '.join(chain['tags'])}")
        
        print("\n" + "="*80)
        return 0
    
    async def execute_on_file(
        self,
        chain_id: str,
        file_path: str,
        language: Optional[str] = None,
        output_path: Optional[str] = None,
        feedback_loops: Optional[int] = None
    ) -> int:
        """Execute chain on a file"""
        try:
            print(f"\n🚀 Executing chain: {chain_id}")
            print(f"   File: {file_path}")
            print(f"   Language: {language or 'auto-detect'}")
            
            execution = await self.controller.execute_chain_on_file(
                chain_id,
                file_path,
                language=language,
                feedback_loops=feedback_loops
            )
            
            # Get report
            report = self.controller.get_execution_report(execution.execution_id)
            
            # Print summary
            self._print_execution_summary(report)
            
            # Save report if requested
            if output_path:
                self.controller.export_report(execution.execution_id, output_path)
                print(f"\n✓ Report saved to: {output_path}")
            
            return 0 if execution.status == "completed" else 1
            
        except Exception as e:
            logger.error(f"Error executing chain: {e}")
            return 1
    
    async def execute_on_code(
        self,
        chain_id: str,
        code: str,
        language: str = "unknown",
        output_path: Optional[str] = None
    ) -> int:
        """Execute chain on code string"""
        try:
            print(f"\n🚀 Executing chain: {chain_id}")
            print(f"   Code lines: {len(code.split(chr(10)))}")
            print(f"   Language: {language}")
            
            execution = await self.controller.execute_chain_on_code(
                chain_id,
                code,
                language=language
            )
            
            # Get report
            report = self.controller.get_execution_report(execution.execution_id)
            
            # Print summary
            self._print_execution_summary(report)
            
            # Save report if requested
            if output_path:
                self.controller.export_report(execution.execution_id, output_path)
                print(f"\n✓ Report saved to: {output_path}")
            
            return 0 if execution.status == "completed" else 1
            
        except Exception as e:
            logger.error(f"Error executing chain: {e}")
            return 1
    
    async def quick_review(
        self,
        file_path: str,
        output_path: Optional[str] = None
    ) -> int:
        """Quick code review"""
        try:
            code = Path(file_path).read_text()
            report = await self.executor.review_and_optimize(code)
            
            self._print_quick_report("CODE REVIEW + OPTIMIZATION", report)
            
            if output_path:
                Path(output_path).write_text(json.dumps(report, indent=2))
            
            return 0
        except Exception as e:
            logger.error(f"Error: {e}")
            return 1
    
    async def quick_secure(
        self,
        file_path: str,
        output_path: Optional[str] = None
    ) -> int:
        """Quick security check"""
        try:
            code = Path(file_path).read_text()
            report = await self.executor.secure_code(code)
            
            self._print_quick_report("SECURITY CHECK", report)
            
            if output_path:
                Path(output_path).write_text(json.dumps(report, indent=2))
            
            return 0
        except Exception as e:
            logger.error(f"Error: {e}")
            return 1
    
    async def quick_document(
        self,
        file_path: str,
        output_path: Optional[str] = None
    ) -> int:
        """Quick documentation"""
        try:
            code = Path(file_path).read_text()
            report = await self.executor.document_code(code)
            
            self._print_quick_report("DOCUMENTATION", report)
            
            if output_path:
                Path(output_path).write_text(json.dumps(report, indent=2))
            
            return 0
        except Exception as e:
            logger.error(f"Error: {e}")
            return 1
    
    async def quick_optimize(
        self,
        file_path: str,
        output_path: Optional[str] = None
    ) -> int:
        """Quick performance optimization"""
        try:
            code = Path(file_path).read_text()
            report = await self.executor.optimize_performance(code)
            
            self._print_quick_report("PERFORMANCE OPTIMIZATION", report)
            
            if output_path:
                Path(output_path).write_text(json.dumps(report, indent=2))
            
            return 0
        except Exception as e:
            logger.error(f"Error: {e}")
            return 1
    
    async def create_chain(
        self,
        chain_id: str,
        name: str,
        agents: str,
        chunk_size: int = 500,
        feedback_loops: int = 1,
        tags: Optional[str] = None
    ) -> int:
        """Create custom chain"""
        try:
            agent_roles = [a.strip() for a in agents.split(',')]
            tag_list = [t.strip() for t in tags.split(',')] if tags else []
            
            config = self.controller.create_custom_chain(
                chain_id=chain_id,
                name=name,
                agent_roles=agent_roles,
                chunk_size=chunk_size,
                feedback_loops=feedback_loops,
                tags=tag_list
            )
            
            print(f"\n✓ Created chain: {config.chain_id}")
            print(f"  Name: {config.name}")
            print(f"  Agents: {' → '.join([m['role'].upper() for m in config.models])}")
            print(f"  Chunk Size: {config.chunk_size} lines")
            print(f"  Feedback Loops: {config.feedback_loops}")
            
            return 0
        except Exception as e:
            logger.error(f"Error: {e}")
            return 1
    
    def _print_execution_summary(self, report: dict):
        """Print execution summary"""
        print("\n" + "="*60)
        print("EXECUTION SUMMARY".center(60))
        print("="*60)
        print(f"Status:        {report['status'].upper()}")
        print(f"Duration:      {report['duration_seconds']:.2f}s")
        print(f"Chunks:        {report['processed_chunks']}/{report['total_chunks']}")
        print(f"Success Rate:  {report['success_rate']}")
        print(f"Total Results: {report['results_count']}")
        print("="*60)
    
    def _print_quick_report(self, title: str, report: dict):
        """Print quick report"""
        print("\n" + "="*60)
        print(title.center(60))
        print("="*60)
        print(f"Status: {report['status'].upper()}")
        print(f"Duration: {report['duration_seconds']:.2f}s")
        print(f"Processed: {report['processed_chunks']}/{report['total_chunks']} chunks")
        print(f"Success Rate: {report['success_rate']}")
        print("="*60)


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description="Model Chain Orchestrator - Cycle models through code chunks",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # List available chains
  python chain_cli.py list
  
  # Execute a chain on a file
  python chain_cli.py execute code_review_chain code.py
  
  # Execute with custom feedback loops
  python chain_cli.py execute code_review_chain code.py --feedback-loops 2
  
  # Quick review
  python chain_cli.py review code.py
  
  # Quick security check
  python chain_cli.py secure code.py
  
  # Create custom chain
  python chain_cli.py create-chain my_chain "My Chain" "analyzer,validator,optimizer"
  
  # Execute and save report
  python chain_cli.py execute code_review_chain code.py -o report.json
        """
    )
    
    subparsers = parser.add_subparsers(dest="command", help="Command to execute")
    
    # List command
    subparsers.add_parser("list", help="List available chains")
    
    # Execute command
    exec_parser = subparsers.add_parser("execute", help="Execute a chain")
    exec_parser.add_argument("chain_id", help="Chain ID")
    exec_parser.add_argument("file", help="Code file to process")
    exec_parser.add_argument("-l", "--language", help="Language (auto-detect if not specified)")
    exec_parser.add_argument("-o", "--output", help="Output report path")
    exec_parser.add_argument("--feedback-loops", type=int, help="Number of feedback loops")
    
    # Quick commands
    for cmd in ["review", "secure", "document", "optimize"]:
        cmd_parser = subparsers.add_parser(cmd, help=f"Quick {cmd}")
        cmd_parser.add_argument("file", help="Code file to process")
        cmd_parser.add_argument("-o", "--output", help="Output report path")
    
    # Create chain command
    create_parser = subparsers.add_parser("create-chain", help="Create custom chain")
    create_parser.add_argument("chain_id", help="Unique chain ID")
    create_parser.add_argument("name", help="Chain name")
    create_parser.add_argument("agents", help="Comma-separated agent roles")
    create_parser.add_argument("-c", "--chunk-size", type=int, default=500, help="Lines per chunk")
    create_parser.add_argument("-f", "--feedback-loops", type=int, default=1, help="Feedback loops")
    create_parser.add_argument("-t", "--tags", help="Comma-separated tags")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return 1
    
    cli = ChainCLI()
    
    # Execute command
    if args.command == "list":
        return asyncio.run(cli.list_chains())
    
    elif args.command == "execute":
        return asyncio.run(cli.execute_on_file(
            args.chain_id,
            args.file,
            language=args.language,
            output_path=args.output,
            feedback_loops=args.feedback_loops
        ))
    
    elif args.command == "review":
        return asyncio.run(cli.quick_review(args.file, args.output))
    
    elif args.command == "secure":
        return asyncio.run(cli.quick_secure(args.file, args.output))
    
    elif args.command == "document":
        return asyncio.run(cli.quick_document(args.file, args.output))
    
    elif args.command == "optimize":
        return asyncio.run(cli.quick_optimize(args.file, args.output))
    
    elif args.command == "create-chain":
        return asyncio.run(cli.create_chain(
            args.chain_id,
            args.name,
            args.agents,
            chunk_size=args.chunk_size,
            feedback_loops=args.feedback_loops,
            tags=args.tags
        ))
    
    return 1


if __name__ == "__main__":
    sys.exit(main())
