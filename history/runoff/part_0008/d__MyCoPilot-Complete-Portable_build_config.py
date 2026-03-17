from compiler_manager import CompilerManager
import asyncio
import logging
from pathlib import Path

class BuildConfiguration:
    def __init__(self, compiler_manager: CompilerManager):
        self.compiler_manager = compiler_manager
        self.logger = logging.getLogger("BuildConfiguration")
        
    async def build_project(self, project_path: str, options: dict = None):
        """Build entire project using appropriate compilers"""
        try:
            build_tasks = []
            for file_path in Path(project_path).rglob("*"):
                if file_path.is_file() and self._is_source_file(file_path):
                    task = self.compiler_manager.compile(
                        str(file_path),
                        options or {"optimization": "O2"}
                    )
                    build_tasks.append(task)
            
            results = await asyncio.gather(*build_tasks, return_exceptions=True)
            return self._process_build_results(results)
        except Exception as e:
            self.logger.error(f"Build failed: {str(e)}")
            raise

    def _is_source_file(self, file_path: Path) -> bool:
        """Check if file is a source file that needs compilation"""
        source_extensions = {
            ".c", ".cpp", ".cs", ".java", ".rs", ".go", ".swift", ".kt",
            ".js", ".py", ".rb", ".php", ".ts", ".scala", ".hs", ".ml",
            ".lua", ".pl", ".jl", ".r", ".m", ".erl", ".ex", ".dart",
            ".zig", ".cr", ".f90", ".cob", ".s", ".wat", ".ll"
        }
        return file_path.suffix.lower() in source_extensions

    def _process_build_results(self, results: list) -> dict:
        """Process and summarize build results"""
        summary = {
            "success": 0,
            "failed": 0,
            "details": []
        }

        for result in results:
            if isinstance(result, Exception):
                summary["failed"] += 1
                summary["details"].append({
                    "status": "failed",
                    "error": str(result)
                })
            else:
                summary["success"] += 1
                summary["details"].append({
                    "status": "success",
                    "result": result
                })

        return summary

async def main():
    # Initialize compiler manager
    compiler_manager = CompilerManager("http://localhost:8080")
    if not await compiler_manager.initialize_toolchains():
        print("Failed to initialize compiler toolchains")
        return

    # Initialize build configuration
    build_config = BuildConfiguration(compiler_manager)

    # Example: Build a project
    project_path = "D:/MyProject"
    try:
        results = await build_config.build_project(project_path)
        print(f"Build complete: {results['success']} succeeded, {results['failed']} failed")
    except Exception as e:
        print(f"Build failed: {str(e)}")

if __name__ == "__main__":
    asyncio.run(main())