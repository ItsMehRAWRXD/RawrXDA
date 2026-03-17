from typing import Dict, List, Optional, Any
import asyncio
import aiohttp
import logging
from pathlib import Path
from enum import Enum

class CompilerType(Enum):
    GCC = "gcc"
    CLANG = "clang"
    ROSLYN = "roslyn"
    MSVC = "msvc"
    JAVAC = "javac"
    RUSTC = "rustc"
    GO = "go"
    SWIFT = "swift"
    KOTLIN = "kotlin"
    NODEJS = "nodejs"
    PYTHON = "python"
    RUBY = "ruby"
    PHP = "php"
    MONO = "mono"
    DOTNET = "dotnet"
    LUA = "lua"
    PERL = "perl"
    JULIA = "julia"
    HASKELL = "haskell"
    OCAML = "ocaml"
    SCALA = "scala"
    TSC = "tsc"
    BABEL = "babel"
    DENO = "deno"
    V8 = "v8"
    SPIDERMONKEY = "spidermonkey"
    CHAKRA = "chakra"
    MATLAB = "matlab"
    R = "r"
    ERLANG = "erlang"
    ELIXIR = "elixir"
    DART = "dart"
    ZIG = "zig"
    CRYSTAL = "crystal"
    FORTRAN = "fortran"
    COBOL = "cobol"
    ASSEMBLY = "assembly"
    WASM = "wasm"
    LLVM = "llvm"

class CompilerManager:
    def __init__(self, api_base: str):
        self.api_base = api_base
        self.logger = logging.getLogger("CompilerManager")
        self.active_toolchains: Dict[str, Dict[str, Any]] = {}

    async def initialize_toolchains(self):
        """Initialize all compiler toolchains with proper validation"""
        try:
            # First validate all required compilers are available
            async with aiohttp.ClientSession() as session:
                for compiler in CompilerType:
                    status = await self._validate_compiler(session, compiler)
                    if not status["available"]:
                        self.logger.warning(f"Compiler {compiler.value} not available: {status['reason']}")
                    else:
                        self.active_toolchains[compiler.value] = status

            if not self.active_toolchains:
                raise RuntimeError("No compiler toolchains available")

            return True
        except Exception as e:
            self.logger.error(f"Toolchain initialization failed: {str(e)}")
            return False

    async def _validate_compiler(self, session: aiohttp.ClientSession, compiler: CompilerType) -> Dict[str, Any]:
        """Validate individual compiler availability and capabilities"""
        try:
            async with session.get(f"{self.api_base}/api/compiler/{compiler.value}/validate") as response:
                if response.status == 200:
                    data = await response.json()
                    return {
                        "available": True,
                        "version": data.get("version"),
                        "features": data.get("features", []),
                        "optimizations": data.get("optimizations", []),
                        "path": data.get("path")
                    }
                return {"available": False, "reason": f"Validation failed with status {response.status}"}
        except Exception as e:
            return {"available": False, "reason": str(e)}

    async def compile(self, file_path: str, options: Dict[str, Any] = None) -> Dict[str, Any]:
        """Compile code using appropriate compiler based on file extension"""
        ext = Path(file_path).suffix.lower()
        compiler = self._get_compiler_for_extension(ext)
        
        if not compiler:
            raise ValueError(f"No compiler available for extension {ext}")

        if compiler not in self.active_toolchains:
            raise RuntimeError(f"Compiler {compiler} not initialized")

        compile_options = {
            "file": file_path,
            "options": options or {},
            "compiler": compiler,
            "toolchain": self.active_toolchains[compiler]
        }

        try:
            async with aiohttp.ClientSession() as session:
                async with session.post(
                    f"{self.api_base}/api/compile",
                    json=compile_options
                ) as response:
                    if response.status != 200:
                        raise RuntimeError(f"Compilation failed with status {response.status}")
                    return await response.json()
        except Exception as e:
            self.logger.error(f"Compilation error: {str(e)}")
            raise

    def _get_compiler_for_extension(self, ext: str) -> Optional[str]:
        """Map file extensions to appropriate compiler"""
        extension_map = {
            ".c": "gcc",
            ".cpp": "gcc",
            ".cs": "roslyn",
            ".java": "javac",
            ".rs": "rustc",
            ".go": "go",
            ".swift": "swift",
            ".kt": "kotlin",
            ".js": "nodejs",
            ".py": "python",
            ".rb": "ruby",
            ".php": "php",
            ".ts": "tsc",
            ".scala": "scala",
            ".hs": "haskell",
            ".ml": "ocaml",
            ".lua": "lua",
            ".pl": "perl",
            ".jl": "julia",
            ".r": "r",
            ".m": "matlab",
            ".erl": "erlang",
            ".ex": "elixir",
            ".dart": "dart",
            ".zig": "zig",
            ".cr": "crystal",
            ".f90": "fortran",
            ".cob": "cobol",
            ".s": "assembly",
            ".wat": "wasm",
            ".ll": "llvm"
        }
        return extension_map.get(ext)

    async def get_optimization_levels(self, compiler: str) -> List[str]:
        """Get available optimization levels for a specific compiler"""
        if compiler not in self.active_toolchains:
            raise ValueError(f"Compiler {compiler} not available")
        return self.active_toolchains[compiler].get("optimizations", [])

    async def get_compiler_features(self, compiler: str) -> List[str]:
        """Get supported features for a specific compiler"""
        if compiler not in self.active_toolchains:
            raise ValueError(f"Compiler {compiler} not available")
        return self.active_toolchains[compiler].get("features", [])

    async def compile_with_all_optimizations(self, file_path: str) -> Dict[str, List[Dict[str, Any]]]:
        """Compile code with all available optimization levels for benchmarking"""
        ext = Path(file_path).suffix.lower()
        compiler = self._get_compiler_for_extension(ext)
        
        if not compiler or compiler not in self.active_toolchains:
            raise ValueError(f"No valid compiler for {file_path}")

        results = []
        optimizations = await self.get_optimization_levels(compiler)
        
        for opt_level in optimizations:
            try:
                result = await self.compile(file_path, {"optimization": opt_level})
                results.append({
                    "optimization": opt_level,
                    "result": result
                })
            except Exception as e:
                self.logger.error(f"Optimization {opt_level} failed: {str(e)}")
                results.append({
                    "optimization": opt_level,
                    "error": str(e)
                })

        return {
            "compiler": compiler,
            "file": file_path,
            "results": results
        }

# Usage example:
async def main():
    compiler_manager = CompilerManager("http://localhost:8080")
    if await compiler_manager.initialize_toolchains():
        print("Available compilers:")
        for compiler, info in compiler_manager.active_toolchains.items():
            print(f"{compiler}: {info['version']}")
            print(f"Features: {info['features']}")
            print(f"Optimizations: {info['optimizations']}")
            print("---")

if __name__ == "__main__":
    asyncio.run(main())