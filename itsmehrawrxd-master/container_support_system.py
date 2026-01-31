#!/usr/bin/env python3
"""
Container Support System for Cross-Platform IDE
Provides Docker, Podman, and native container support for Mac/Linux
"""

import os
import sys
import subprocess
import platform
import json
import time
from pathlib import Path

class ContainerSupportSystem:
    """Cross-platform container support for the IDE"""
    
    def __init__(self):
        self.platform = platform.system().lower()
        self.architecture = platform.machine().lower()
        self.container_engines = []
        self.available_engines = []
        
        print(f"Container Support System initialized for {self.platform} {self.architecture}")
        self.detect_container_engines()
    
    def detect_container_engines(self):
        """Detect available container engines on the system"""
        engines_to_check = [
            ('docker', 'Docker'),
            ('podman', 'Podman'),
            ('containerd', 'containerd'),
            ('crictl', 'CRI-O'),
            ('nerdctl', 'nerdctl')
        ]
        
        for command, name in engines_to_check:
            if self.is_command_available(command):
                self.available_engines.append((command, name))
                print(f"✓ {name} detected")
            else:
                print(f"✗ {name} not available")
        
        if not self.available_engines:
            print("⚠️ No container engines detected")
        else:
            print(f"Found {len(self.available_engines)} container engines")
    
    def is_command_available(self, command):
        """Check if a command is available in PATH"""
        try:
            subprocess.run([command, '--version'], 
                         capture_output=True, check=True, timeout=5)
            return True
        except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
            return False
    
    def get_primary_engine(self):
        """Get the primary container engine to use"""
        if not self.available_engines:
            return None
        
        # Prefer Docker, then Podman, then others
        preferred_order = ['docker', 'podman', 'containerd', 'crictl', 'nerdctl']
        
        for preferred in preferred_order:
            for command, name in self.available_engines:
                if command == preferred:
                    return (command, name)
        
        return self.available_engines[0]
    
    def create_ide_container(self, engine=None):
        """Create IDE container for cross-platform development"""
        if not engine:
            engine, name = self.get_primary_engine()
            if not engine:
                return {"success": False, "error": "No container engine available"}
        else:
            name = engine
        
        print(f"Creating IDE container using {name}...")
        
        # Create Dockerfile for IDE
        dockerfile_content = self.generate_ide_dockerfile()
        
        # Write Dockerfile
        with open('Dockerfile.ide', 'w') as f:
            f.write(dockerfile_content)
        
        # Build container
        try:
            cmd = [engine, 'build', '-t', 'eon-ide', '-f', 'Dockerfile.ide', '.']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
            
            if result.returncode == 0:
                print("✓ IDE container built successfully")
                return {"success": True, "engine": name, "image": "eon-ide"}
            else:
                print(f"✗ Container build failed: {result.stderr}")
                return {"success": False, "error": result.stderr}
                
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Container build timeout"}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def generate_ide_dockerfile(self):
        """Generate Dockerfile for IDE container"""
        base_image = self.get_base_image()
        
        dockerfile = f"""# EON IDE Container
FROM {base_image}

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV PYTHONUNBUFFERED=1
ENV DISPLAY=:0

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    python3 \\
    python3-pip \\
    python3-dev \\
    build-essential \\
    gcc \\
    g++ \\
    make \\
    cmake \\
    git \\
    curl \\
    wget \\
    vim \\
    nano \\
    htop \\
    tree \\
    file \\
    && rm -rf /var/lib/apt/lists/*

# Install Python packages
RUN pip3 install --no-cache-dir \\
    tkinter \\
    requests \\
    psutil \\
    docker \\
    pyyaml \\
    colorama

# Create IDE workspace
WORKDIR /workspace
RUN mkdir -p /workspace/projects \\
    /workspace/build \\
    /workspace/tools

# Copy IDE files
COPY *.py /workspace/
COPY *.eon /workspace/
COPY *.bat /workspace/
COPY *.sh /workspace/

# Set permissions
RUN chmod +x /workspace/*.sh 2>/dev/null || true

# Expose ports for IDE services
EXPOSE 8080 8081 8082 8083 8084

# Create startup script
RUN echo '#!/bin/bash' > /workspace/start-ide.sh && \\
    echo 'cd /workspace' >> /workspace/start-ide.sh && \\
    echo 'python3 eon-compiler-gui.py' >> /workspace/start-ide.sh && \\
    chmod +x /workspace/start-ide.sh

# Default command
CMD ["/workspace/start-ide.sh"]
"""
        return dockerfile
    
    def get_base_image(self):
        """Get appropriate base image for the platform"""
        if self.architecture in ['x86_64', 'amd64']:
            return 'ubuntu:22.04'
        elif self.architecture in ['arm64', 'aarch64']:
            return 'ubuntu:22.04'  # ARM64 support
        else:
            return 'ubuntu:22.04'
    
    def run_ide_container(self, engine=None, port=8080):
        """Run IDE in container"""
        if not engine:
            engine, name = self.get_primary_engine()
            if not engine:
                return {"success": False, "error": "No container engine available"}
        else:
            name = engine
        
        print(f"Starting IDE container using {name}...")
        
        # Prepare volume mounts
        current_dir = os.getcwd()
        volume_mount = f"{current_dir}:/workspace"
        
        # Prepare port mapping
        port_mapping = f"{port}:8080"
        
        try:
            cmd = [
                engine, 'run', '-it', '--rm',
                '-v', volume_mount,
                '-p', port_mapping,
                '--name', 'eon-ide-container',
                'eon-ide'
            ]
            
            print(f"Running: {' '.join(cmd)}")
            subprocess.run(cmd, timeout=30)
            
            return {"success": True, "engine": name, "port": port}
            
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Container startup timeout"}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def create_development_environment(self):
        """Create complete development environment"""
        print("Creating cross-platform development environment...")
        
        # Create container
        container_result = self.create_ide_container()
        if not container_result["success"]:
            return container_result
        
        # Create docker-compose for services
        compose_result = self.create_docker_compose()
        
        # Create startup scripts
        self.create_startup_scripts()
        
        return {
            "success": True,
            "container": container_result,
            "compose": compose_result,
            "scripts": "startup scripts created"
        }
    
    def create_docker_compose(self):
        """Create docker-compose.yml for IDE services"""
        compose_content = """version: '3.8'

services:
  eon-ide:
    build:
      context: .
      dockerfile: Dockerfile.ide
    container_name: eon-ide
    ports:
      - "8080:8080"
      - "8081:8081"
      - "8082:8082"
      - "8083:8083"
      - "8084:8084"
    volumes:
      - .:/workspace
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - DISPLAY=${DISPLAY}
      - PYTHONUNBUFFERED=1
    networks:
      - eon-network

  chatgpt-api:
    image: chatgpt-api:latest
    container_name: chatgpt-api
    ports:
      - "8081:8081"
    environment:
      - API_KEY=${CHATGPT_API_KEY}
    networks:
      - eon-network

  claude-api:
    image: claude-api:latest
    container_name: claude-api
    ports:
      - "8082:8082"
    environment:
      - API_KEY=${CLAUDE_API_KEY}
    networks:
      - eon-network

  ollama-server:
    image: ollama/ollama:latest
    container_name: ollama-server
    ports:
      - "8083:11434"
    volumes:
      - ollama_data:/root/.ollama
    networks:
      - eon-network

  code-analyzer:
    image: code-analyzer:latest
    container_name: code-analyzer
    ports:
      - "8084:8084"
    volumes:
      - .:/workspace
    networks:
      - eon-network

networks:
  eon-network:
    driver: bridge

volumes:
  ollama_data:
"""
        
        with open('docker-compose.yml', 'w') as f:
            f.write(compose_content)
        
        print("✓ docker-compose.yml created")
        return {"success": True, "file": "docker-compose.yml"}
    
    def create_startup_scripts(self):
        """Create platform-specific startup scripts"""
        
        # Linux/Mac startup script
        linux_script = """#!/bin/bash
# EON IDE Container Startup Script

echo "Starting EON IDE Container Environment..."

# Check for container engine
if command -v docker &> /dev/null; then
    ENGINE="docker"
elif command -v podman &> /dev/null; then
    ENGINE="podman"
else
    echo "Error: No container engine found (Docker or Podman required)"
    exit 1
fi

echo "Using container engine: $ENGINE"

# Start services with docker-compose
if [ -f "docker-compose.yml" ]; then
    echo "Starting services with docker-compose..."
    $ENGINE-compose up -d
else
    echo "Building and starting IDE container..."
    $ENGINE build -t eon-ide .
    $ENGINE run -it --rm -v $(pwd):/workspace -p 8080:8080 --name eon-ide-container eon-ide
fi

echo "EON IDE Container Environment started!"
echo "Access IDE at: http://localhost:8080"
"""
        
        # Windows startup script
        windows_script = """@echo off
REM EON IDE Container Startup Script

echo Starting EON IDE Container Environment...

REM Check for Docker Desktop
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Docker Desktop not found
    echo Please install Docker Desktop from https://www.docker.com/products/docker-desktop
    pause
    exit /b 1
)

echo Using container engine: Docker

REM Start services with docker-compose
if exist "docker-compose.yml" (
    echo Starting services with docker-compose...
    docker-compose up -d
) else (
    echo Building and starting IDE container...
    docker build -t eon-ide .
    docker run -it --rm -v "%cd%":/workspace -p 8080:8080 --name eon-ide-container eon-ide
)

echo EON IDE Container Environment started!
echo Access IDE at: http://localhost:8080
pause
"""
        
        # Write scripts
        with open('start-ide.sh', 'w') as f:
            f.write(linux_script)
        os.chmod('start-ide.sh', 0o755)
        
        with open('start-ide.bat', 'w') as f:
            f.write(windows_script)
        
        print("✓ Startup scripts created")
        return {"success": True, "scripts": ["start-ide.sh", "start-ide.bat"]}
    
    def install_container_engine(self, engine='docker'):
        """Install container engine on the system"""
        if self.platform == 'linux':
            return self.install_docker_linux()
        elif self.platform == 'darwin':  # macOS
            return self.install_docker_macos()
        elif self.platform == 'windows':
            return self.install_docker_windows()
        else:
            return {"success": False, "error": f"Unsupported platform: {self.platform}"}
    
    def install_docker_linux(self):
        """Install Docker on Linux"""
        print("Installing Docker on Linux...")
        
        install_script = """#!/bin/bash
# Docker installation script for Linux

# Update package index
sudo apt-get update

# Install prerequisites
sudo apt-get install -y \\
    apt-transport-https \\
    ca-certificates \\
    curl \\
    gnupg \\
    lsb-release

# Add Docker's official GPG key
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg

# Set up stable repository
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

# Update package index again
sudo apt-get update

# Install Docker Engine
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-compose-plugin

# Add user to docker group
sudo usermod -aG docker $USER

echo "Docker installation complete!"
echo "Please log out and log back in for group changes to take effect."
"""
        
        with open('install-docker.sh', 'w') as f:
            f.write(install_script)
        os.chmod('install-docker.sh', 0o755)
        
        return {"success": True, "script": "install-docker.sh", "message": "Run ./install-docker.sh to install Docker"}
    
    def install_docker_macos(self):
        """Install Docker on macOS"""
        return {
            "success": True, 
            "message": "Please install Docker Desktop for Mac from https://www.docker.com/products/docker-desktop",
            "url": "https://www.docker.com/products/docker-desktop"
        }
    
    def install_docker_windows(self):
        """Install Docker on Windows"""
        return {
            "success": True,
            "message": "Please install Docker Desktop for Windows from https://www.docker.com/products/docker-desktop",
            "url": "https://www.docker.com/products/docker-desktop"
        }
    
    def get_container_status(self):
        """Get status of container services"""
        engine, name = self.get_primary_engine()
        if not engine:
            return {"available": False, "engines": []}
        
        try:
            # Check running containers
            cmd = [engine, 'ps', '--format', 'json']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            
            containers = []
            if result.returncode == 0:
                for line in result.stdout.strip().split('\n'):
                    if line:
                        try:
                            container = json.loads(line)
                            containers.append(container)
                        except json.JSONDecodeError:
                            continue
            
            return {
                "available": True,
                "engine": name,
                "containers": containers,
                "count": len(containers)
            }
            
        except Exception as e:
            return {"available": True, "engine": name, "error": str(e)}
    
    def stop_all_containers(self):
        """Stop all IDE-related containers"""
        engine, name = self.get_primary_engine()
        if not engine:
            return {"success": False, "error": "No container engine available"}
        
        try:
            # Stop containers
            cmd = [engine, 'stop', 'eon-ide-container', 'chatgpt-api', 'claude-api', 'ollama-server', 'code-analyzer']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            # Remove containers
            cmd = [engine, 'rm', 'eon-ide-container', 'chatgpt-api', 'claude-api', 'ollama-server', 'code-analyzer']
            subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            return {"success": True, "engine": name}
            
        except Exception as e:
            return {"success": False, "error": str(e)}

def main():
    """Test container support system"""
    print("Testing Container Support System...")
    
    container_system = ContainerSupportSystem()
    
    # Test detection
    print(f"\nAvailable engines: {container_system.available_engines}")
    
    # Test primary engine
    primary = container_system.get_primary_engine()
    if primary:
        print(f"Primary engine: {primary[1]}")
    else:
        print("No container engines available")
    
    # Test container status
    status = container_system.get_container_status()
    print(f"Container status: {status}")
    
    # Create development environment
    print("\nCreating development environment...")
    env_result = container_system.create_development_environment()
    print(f"Environment creation: {env_result}")

if __name__ == "__main__":
    main()
