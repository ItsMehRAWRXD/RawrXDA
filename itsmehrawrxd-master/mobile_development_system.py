#!/usr/bin/env python3
"""
Mobile Development System for iOS/Android
Supports BlueStacks, Android Studio, Xcode, and mobile containers
"""

import os
import sys
import subprocess
import platform
import json
import time
from pathlib import Path

class MobileDevelopmentSystem:
    """Cross-platform mobile development support for iOS/Android"""
    
    def __init__(self):
        self.platform = platform.system().lower()
        self.architecture = platform.machine().lower()
        self.mobile_tools = []
        self.available_tools = []
        
        print(f"Mobile Development System initialized for {self.platform} {self.architecture}")
        self.detect_mobile_tools()
    
    def detect_mobile_tools(self):
        """Detect available mobile development tools"""
        tools_to_check = [
            ('adb', 'Android Debug Bridge'),
            ('emulator', 'Android Emulator'),
            ('xcodebuild', 'Xcode Build Tools'),
            ('flutter', 'Flutter SDK'),
            ('react-native', 'React Native'),
            ('cordova', 'Apache Cordova'),
            ('ionic', 'Ionic Framework'),
            ('bluestacks', 'BlueStacks'),
            ('genymotion', 'Genymotion'),
            ('android-studio', 'Android Studio')
        ]
        
        for command, name in tools_to_check:
            if self.is_command_available(command):
                self.available_tools.append((command, name))
                print(f"✓ {name} detected")
            else:
                print(f"✗ {name} not available")
        
        if not self.available_tools:
            print("⚠️ No mobile development tools detected")
        else:
            print(f"Found {len(self.available_tools)} mobile development tools")
    
    def is_command_available(self, command):
        """Check if a command is available in PATH"""
        try:
            subprocess.run([command, '--version'], 
                         capture_output=True, check=True, timeout=5)
            return True
        except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
            return False
    
    def create_mobile_container(self, platform_type='android'):
        """Create mobile development container"""
        print(f"Creating {platform_type} development container...")
        
        # Create Dockerfile for mobile development
        dockerfile_content = self.generate_mobile_dockerfile(platform_type)
        
        # Write Dockerfile
        dockerfile_name = f'Dockerfile.{platform_type}'
        with open(dockerfile_name, 'w') as f:
            f.write(dockerfile_content)
        
        # Build container
        try:
            cmd = ['docker', 'build', '-t', f'mobile-{platform_type}-dev', '-f', dockerfile_name, '.']
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)
            
            if result.returncode == 0:
                print(f"✓ {platform_type} development container built successfully")
                return {"success": True, "platform": platform_type, "image": f"mobile-{platform_type}-dev"}
            else:
                print(f"✗ Container build failed: {result.stderr}")
                return {"success": False, "error": result.stderr}
                
        except subprocess.TimeoutExpired:
            return {"success": False, "error": "Container build timeout"}
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def generate_mobile_dockerfile(self, platform_type):
        """Generate Dockerfile for mobile development"""
        if platform_type == 'android':
            return self.generate_android_dockerfile()
        elif platform_type == 'ios':
            return self.generate_ios_dockerfile()
        elif platform_type == 'flutter':
            return self.generate_flutter_dockerfile()
        else:
            return self.generate_cross_platform_dockerfile()
    
    def generate_android_dockerfile(self):
        """Generate Android development Dockerfile"""
        return """# Android Development Container
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools
ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    openjdk-11-jdk \\
    wget \\
    curl \\
    unzip \\
    git \\
    python3 \\
    python3-pip \\
    build-essential \\
    cmake \\
    ninja-build \\
    pkg-config \\
    libgtk-3-dev \\
    libglib2.0-dev \\
    libgstreamer1.0-dev \\
    libgstreamer-plugins-base1.0-dev \\
    libgstreamer-plugins-bad1.0-dev \\
    libgstreamer-gl1.0-dev \\
    libgstreamer-plugins-good1.0-dev \\
    libgstreamer-plugins-ugly1.0-dev \\
    libgstreamer-plugins-bad1.0-dev \\
    libgstreamer-plugins-ugly1.0-dev \\
    libgstreamer-plugins-bad1.0-dev \\
    && rm -rf /var/lib/apt/lists/*

# Install Android SDK
RUN mkdir -p $ANDROID_HOME && \\
    cd $ANDROID_HOME && \\
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip && \\
    unzip commandlinetools-linux-9477386_latest.zip && \\
    rm commandlinetools-linux-9477386_latest.zip && \\
    mkdir -p cmdline-tools/latest && \\
    mv cmdline-tools/* cmdline-tools/latest/ 2>/dev/null || true

# Install Android SDK components
RUN yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses && \\
    $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \\
    "platform-tools" \\
    "platforms;android-33" \\
    "platforms;android-32" \\
    "platforms;android-31" \\
    "build-tools;33.0.0" \\
    "build-tools;32.0.0" \\
    "build-tools;31.0.0" \\
    "emulator" \\
    "system-images;android-33;google_apis;x86_64" \\
    "system-images;android-32;google_apis;x86_64"

# Install Flutter
RUN cd /opt && \\
    git clone https://github.com/flutter/flutter.git -b stable && \\
    export PATH="$PATH:/opt/flutter/bin" && \\
    flutter doctor

# Install React Native CLI
RUN npm install -g react-native-cli

# Install Cordova
RUN npm install -g cordova

# Install Ionic
RUN npm install -g @ionic/cli

# Create workspace
WORKDIR /workspace
RUN mkdir -p /workspace/projects \\
    /workspace/android \\
    /workspace/ios \\
    /workspace/flutter \\
    /workspace/react-native

# Copy IDE files
COPY *.py /workspace/
COPY *.eon /workspace/
COPY *.bat /workspace/
COPY *.sh /workspace/

# Set permissions
RUN chmod +x /workspace/*.sh 2>/dev/null || true

# Expose ports
EXPOSE 8080 8081 8082 8083 8084 5555 5556

# Create startup script
RUN echo '#!/bin/bash' > /workspace/start-mobile-dev.sh && \\
    echo 'cd /workspace' >> /workspace/start-mobile-dev.sh && \\
    echo 'export ANDROID_HOME=/opt/android-sdk' >> /workspace/start-mobile-dev.sh && \\
    echo 'export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools' >> /workspace/start-mobile-dev.sh && \\
    echo 'export PATH=$PATH:/opt/flutter/bin' >> /workspace/start-mobile-dev.sh && \\
    echo 'python3 eon-compiler-gui.py' >> /workspace/start-mobile-dev.sh && \\
    chmod +x /workspace/start-mobile-dev.sh

# Default command
CMD ["/workspace/start-mobile-dev.sh"]
"""
    
    def generate_ios_dockerfile(self):
        """Generate iOS development Dockerfile"""
        return """# iOS Development Container
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=$PATH:/opt/flutter/bin

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    curl \\
    git \\
    unzip \\
    xz-utils \\
    zip \\
    libglu1-mesa \\
    python3 \\
    python3-pip \\
    build-essential \\
    cmake \\
    ninja-build \\
    pkg-config \\
    libgtk-3-dev \\
    libglib2.0-dev \\
    && rm -rf /var/lib/apt/lists/*

# Install Flutter
RUN cd /opt && \\
    git clone https://github.com/flutter/flutter.git -b stable && \\
    export PATH="$PATH:/opt/flutter/bin" && \\
    flutter doctor

# Install React Native CLI
RUN npm install -g react-native-cli

# Install Cordova
RUN npm install -g cordova

# Install Ionic
RUN npm install -g @ionic/cli

# Create workspace
WORKDIR /workspace
RUN mkdir -p /workspace/projects \\
    /workspace/ios \\
    /workspace/flutter \\
    /workspace/react-native

# Copy IDE files
COPY *.py /workspace/
COPY *.eon /workspace/
COPY *.bat /workspace/
COPY *.sh /workspace/

# Set permissions
RUN chmod +x /workspace/*.sh 2>/dev/null || true

# Expose ports
EXPOSE 8080 8081 8082 8083 8084

# Create startup script
RUN echo '#!/bin/bash' > /workspace/start-ios-dev.sh && \\
    echo 'cd /workspace' >> /workspace/start-ios-dev.sh && \\
    echo 'export PATH=$PATH:/opt/flutter/bin' >> /workspace/start-ios-dev.sh && \\
    echo 'python3 eon-compiler-gui.py' >> /workspace/start-ios-dev.sh && \\
    chmod +x /workspace/start-ios-dev.sh

# Default command
CMD ["/workspace/start-ios-dev.sh"]
"""
    
    def generate_flutter_dockerfile(self):
        """Generate Flutter development Dockerfile"""
        return """# Flutter Development Container
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV PATH=$PATH:/opt/flutter/bin

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    curl \\
    git \\
    unzip \\
    xz-utils \\
    zip \\
    libglu1-mesa \\
    python3 \\
    python3-pip \\
    build-essential \\
    cmake \\
    ninja-build \\
    pkg-config \\
    libgtk-3-dev \\
    libglib2.0-dev \\
    openjdk-11-jdk \\
    wget \\
    && rm -rf /var/lib/apt/lists/*

# Install Flutter
RUN cd /opt && \\
    git clone https://github.com/flutter/flutter.git -b stable && \\
    export PATH="$PATH:/opt/flutter/bin" && \\
    flutter doctor

# Install Android SDK
ENV ANDROID_HOME=/opt/android-sdk
RUN mkdir -p $ANDROID_HOME && \\
    cd $ANDROID_HOME && \\
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip && \\
    unzip commandlinetools-linux-9477386_latest.zip && \\
    rm commandlinetools-linux-9477386_latest.zip && \\
    mkdir -p cmdline-tools/latest && \\
    mv cmdline-tools/* cmdline-tools/latest/ 2>/dev/null || true

# Install Android SDK components
RUN yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses && \\
    $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \\
    "platform-tools" \\
    "platforms;android-33" \\
    "platforms;android-32" \\
    "build-tools;33.0.0" \\
    "build-tools;32.0.0" \\
    "emulator" \\
    "system-images;android-33;google_apis;x86_64"

# Create workspace
WORKDIR /workspace
RUN mkdir -p /workspace/projects \\
    /workspace/flutter \\
    /workspace/android \\
    /workspace/ios

# Copy IDE files
COPY *.py /workspace/
COPY *.eon /workspace/
COPY *.bat /workspace/
COPY *.sh /workspace/

# Set permissions
RUN chmod +x /workspace/*.sh 2>/dev/null || true

# Expose ports
EXPOSE 8080 8081 8082 8083 8084 5555

# Create startup script
RUN echo '#!/bin/bash' > /workspace/start-flutter-dev.sh && \\
    echo 'cd /workspace' >> /workspace/start-flutter-dev.sh && \\
    echo 'export PATH=$PATH:/opt/flutter/bin' >> /workspace/start-flutter-dev.sh && \\
    echo 'export ANDROID_HOME=/opt/android-sdk' >> /workspace/start-flutter-dev.sh && \\
    echo 'export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools' >> /workspace/start-flutter-dev.sh && \\
    echo 'python3 eon-compiler-gui.py' >> /workspace/start-flutter-dev.sh && \\
    chmod +x /workspace/start-flutter-dev.sh

# Default command
CMD ["/workspace/start-flutter-dev.sh"]
"""
    
    def generate_cross_platform_dockerfile(self):
        """Generate cross-platform mobile development Dockerfile"""
        return """# Cross-Platform Mobile Development Container
FROM ubuntu:22.04

# Set environment variables
ENV DEBIAN_FRONTEND=noninteractive
ENV ANDROID_HOME=/opt/android-sdk
ENV PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools
ENV PATH=$PATH:/opt/flutter/bin
ENV JAVA_HOME=/usr/lib/jvm/java-11-openjdk-amd64

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    openjdk-11-jdk \\
    wget \\
    curl \\
    unzip \\
    git \\
    python3 \\
    python3-pip \\
    build-essential \\
    cmake \\
    ninja-build \\
    pkg-config \\
    libgtk-3-dev \\
    libglib2.0-dev \\
    libgstreamer1.0-dev \\
    libgstreamer-plugins-base1.0-dev \\
    libgstreamer-plugins-bad1.0-dev \\
    libgstreamer-gl1.0-dev \\
    libgstreamer-plugins-good1.0-dev \\
    libgstreamer-plugins-ugly1.0-dev \\
    xz-utils \\
    zip \\
    libglu1-mesa \\
    && rm -rf /var/lib/apt/lists/*

# Install Android SDK
RUN mkdir -p $ANDROID_HOME && \\
    cd $ANDROID_HOME && \\
    wget -q https://dl.google.com/android/repository/commandlinetools-linux-9477386_latest.zip && \\
    unzip commandlinetools-linux-9477386_latest.zip && \\
    rm commandlinetools-linux-9477386_latest.zip && \\
    mkdir -p cmdline-tools/latest && \\
    mv cmdline-tools/* cmdline-tools/latest/ 2>/dev/null || true

# Install Android SDK components
RUN yes | $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager --licenses && \\
    $ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \\
    "platform-tools" \\
    "platforms;android-33" \\
    "platforms;android-32" \\
    "platforms;android-31" \\
    "build-tools;33.0.0" \\
    "build-tools;32.0.0" \\
    "build-tools;31.0.0" \\
    "emulator" \\
    "system-images;android-33;google_apis;x86_64" \\
    "system-images;android-32;google_apis;x86_64"

# Install Flutter
RUN cd /opt && \\
    git clone https://github.com/flutter/flutter.git -b stable && \\
    export PATH="$PATH:/opt/flutter/bin" && \\
    flutter doctor

# Install React Native CLI
RUN npm install -g react-native-cli

# Install Cordova
RUN npm install -g cordova

# Install Ionic
RUN npm install -g @ionic/cli

# Create workspace
WORKDIR /workspace
RUN mkdir -p /workspace/projects \\
    /workspace/android \\
    /workspace/ios \\
    /workspace/flutter \\
    /workspace/react-native \\
    /workspace/cordova \\
    /workspace/ionic

# Copy IDE files
COPY *.py /workspace/
COPY *.eon /workspace/
COPY *.bat /workspace/
COPY *.sh /workspace/

# Set permissions
RUN chmod +x /workspace/*.sh 2>/dev/null || true

# Expose ports
EXPOSE 8080 8081 8082 8083 8084 5555 5556

# Create startup script
RUN echo '#!/bin/bash' > /workspace/start-mobile-dev.sh && \\
    echo 'cd /workspace' >> /workspace/start-mobile-dev.sh && \\
    echo 'export ANDROID_HOME=/opt/android-sdk' >> /workspace/start-mobile-dev.sh && \\
    echo 'export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/platform-tools' >> /workspace/start-mobile-dev.sh && \\
    echo 'export PATH=$PATH:/opt/flutter/bin' >> /workspace/start-mobile-dev.sh && \\
    echo 'python3 eon-compiler-gui.py' >> /workspace/start-mobile-dev.sh && \\
    chmod +x /workspace/start-mobile-dev.sh

# Default command
CMD ["/workspace/start-mobile-dev.sh"]
"""
    
    def create_bluestacks_integration(self):
        """Create BlueStacks integration for Android development"""
        print("Creating BlueStacks integration...")
        
        bluestacks_script = """#!/bin/bash
# BlueStacks Integration Script

echo "Starting BlueStacks integration..."

# Check if BlueStacks is installed
if [ -d "/c/Program Files/BlueStacks_nxt" ]; then
    BLUESTACKS_PATH="/c/Program Files/BlueStacks_nxt"
elif [ -d "/c/Program Files (x86)/BlueStacks_nxt" ]; then
    BLUESTACKS_PATH="/c/Program Files (x86)/BlueStacks_nxt"
else
    echo "BlueStacks not found. Please install BlueStacks first."
    exit 1
fi

echo "BlueStacks found at: $BLUESTACKS_PATH"

# Start BlueStacks
echo "Starting BlueStacks..."
"$BLUESTACKS_PATH/HD-Player.exe" &

# Wait for BlueStacks to start
echo "Waiting for BlueStacks to start..."
sleep 30

# Connect to BlueStacks via ADB
echo "Connecting to BlueStacks via ADB..."
adb connect 127.0.0.1:5555

# Check connection
echo "Checking ADB connection..."
adb devices

echo "BlueStacks integration complete!"
echo "You can now develop Android apps with BlueStacks as the emulator."
"""
        
        with open('bluestacks-integration.sh', 'w') as f:
            f.write(bluestacks_script)
        os.chmod('bluestacks-integration.sh', 0o755)
        
        # Create Windows batch file
        bluestacks_bat = """@echo off
REM BlueStacks Integration Script for Windows

echo Starting BlueStacks integration...

REM Check if BlueStacks is installed
if exist "C:\\Program Files\\BlueStacks_nxt\\HD-Player.exe" (
    set BLUESTACKS_PATH=C:\\Program Files\\BlueStacks_nxt
) else if exist "C:\\Program Files (x86)\\BlueStacks_nxt\\HD-Player.exe" (
    set BLUESTACKS_PATH=C:\\Program Files (x86)\\BlueStacks_nxt
) else (
    echo BlueStacks not found. Please install BlueStacks first.
    pause
    exit /b 1
)

echo BlueStacks found at: %BLUESTACKS_PATH%

REM Start BlueStacks
echo Starting BlueStacks...
start "" "%BLUESTACKS_PATH%\\HD-Player.exe"

REM Wait for BlueStacks to start
echo Waiting for BlueStacks to start...
timeout /t 30 /nobreak

REM Connect to BlueStacks via ADB
echo Connecting to BlueStacks via ADB...
adb connect 127.0.0.1:5555

REM Check connection
echo Checking ADB connection...
adb devices

echo BlueStacks integration complete!
echo You can now develop Android apps with BlueStacks as the emulator.
pause
"""
        
        with open('bluestacks-integration.bat', 'w') as f:
            f.write(bluestacks_bat)
        
        return {"success": True, "scripts": ["bluestacks-integration.sh", "bluestacks-integration.bat"]}
    
    def create_mobile_docker_compose(self):
        """Create docker-compose.yml for mobile development"""
        compose_content = """version: '3.8'

services:
  mobile-android-dev:
    build:
      context: .
      dockerfile: Dockerfile.android
    container_name: mobile-android-dev
    ports:
      - "8080:8080"
      - "8081:8081"
      - "8082:8082"
      - "8083:8083"
      - "8084:8084"
      - "5555:5555"
      - "5556:5556"
    volumes:
      - .:/workspace
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - ANDROID_HOME=/opt/android-sdk
      - PATH=$PATH:/opt/android-sdk/tools:/opt/android-sdk/platform-tools
    networks:
      - mobile-network

  mobile-ios-dev:
    build:
      context: .
      dockerfile: Dockerfile.ios
    container_name: mobile-ios-dev
    ports:
      - "8085:8080"
      - "8086:8081"
      - "8087:8082"
      - "8088:8083"
      - "8089:8084"
    volumes:
      - .:/workspace
    environment:
      - PATH=$PATH:/opt/flutter/bin
    networks:
      - mobile-network

  mobile-flutter-dev:
    build:
      context: .
      dockerfile: Dockerfile.flutter
    container_name: mobile-flutter-dev
    ports:
      - "8090:8080"
      - "8091:8081"
      - "8092:8082"
      - "8093:8083"
      - "8094:8084"
      - "8095:5555"
    volumes:
      - .:/workspace
      - /var/run/docker.sock:/var/run/docker.sock
    environment:
      - ANDROID_HOME=/opt/android-sdk
      - PATH=$PATH:/opt/android-sdk/tools:/opt/android-sdk/platform-tools:/opt/flutter/bin
    networks:
      - mobile-network

  bluestacks-integration:
    image: bluestacks-integration:latest
    container_name: bluestacks-integration
    ports:
      - "5555:5555"
    volumes:
      - .:/workspace
    networks:
      - mobile-network

networks:
  mobile-network:
    driver: bridge
"""
        
        with open('docker-compose.mobile.yml', 'w') as f:
            f.write(compose_content)
        
        return {"success": True, "file": "docker-compose.mobile.yml"}
    
    def create_mobile_startup_scripts(self):
        """Create mobile development startup scripts"""
        
        # Android startup script
        android_script = """#!/bin/bash
# Android Development Startup Script

echo "Starting Android Development Environment..."

# Check for Docker
if command -v docker &> /dev/null; then
    ENGINE="docker"
elif command -v podman &> /dev/null; then
    ENGINE="podman"
else
    echo "Error: No container engine found (Docker or Podman required)"
    exit 1
fi

echo "Using container engine: $ENGINE"

# Start Android development container
echo "Starting Android development container..."
$ENGINE run -it --rm \\
    -v $(pwd):/workspace \\
    -p 8080:8080 \\
    -p 8081:8081 \\
    -p 8082:8082 \\
    -p 8083:8083 \\
    -p 8084:8084 \\
    -p 5555:5555 \\
    -p 5556:5556 \\
    --name android-dev-container \\
    mobile-android-dev

echo "Android Development Environment started!"
echo "Access IDE at: http://localhost:8080"
"""
        
        # iOS startup script
        ios_script = """#!/bin/bash
# iOS Development Startup Script

echo "Starting iOS Development Environment..."

# Check for Docker
if command -v docker &> /dev/null; then
    ENGINE="docker"
elif command -v podman &> /dev/null; then
    ENGINE="podman"
else
    echo "Error: No container engine found (Docker or Podman required)"
    exit 1
fi

echo "Using container engine: $ENGINE"

# Start iOS development container
echo "Starting iOS development container..."
$ENGINE run -it --rm \\
    -v $(pwd):/workspace \\
    -p 8080:8080 \\
    -p 8081:8081 \\
    -p 8082:8082 \\
    -p 8083:8083 \\
    -p 8084:8084 \\
    --name ios-dev-container \\
    mobile-ios-dev

echo "iOS Development Environment started!"
echo "Access IDE at: http://localhost:8080"
"""
        
        # Flutter startup script
        flutter_script = """#!/bin/bash
# Flutter Development Startup Script

echo "Starting Flutter Development Environment..."

# Check for Docker
if command -v docker &> /dev/null; then
    ENGINE="docker"
elif command -v podman &> /dev/null; then
    ENGINE="podman"
else
    echo "Error: No container engine found (Docker or Podman required)"
    exit 1
fi

echo "Using container engine: $ENGINE"

# Start Flutter development container
echo "Starting Flutter development container..."
$ENGINE run -it --rm \\
    -v $(pwd):/workspace \\
    -p 8080:8080 \\
    -p 8081:8081 \\
    -p 8082:8082 \\
    -p 8083:8083 \\
    -p 8084:8084 \\
    -p 8095:5555 \\
    --name flutter-dev-container \\
    mobile-flutter-dev

echo "Flutter Development Environment started!"
echo "Access IDE at: http://localhost:8080"
"""
        
        # Write scripts
        with open('start-android-dev.sh', 'w') as f:
            f.write(android_script)
        os.chmod('start-android-dev.sh', 0o755)
        
        with open('start-ios-dev.sh', 'w') as f:
            f.write(ios_script)
        os.chmod('start-ios-dev.sh', 0o755)
        
        with open('start-flutter-dev.sh', 'w') as f:
            f.write(flutter_script)
        os.chmod('start-flutter-dev.sh', 0o755)
        
        # Create Windows batch files
        android_bat = """@echo off
REM Android Development Startup Script for Windows

echo Starting Android Development Environment...

REM Check for Docker
docker --version >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Docker Desktop not found
    echo Please install Docker Desktop from https://www.docker.com/products/docker-desktop
    pause
    exit /b 1
)

echo Using container engine: Docker

REM Start Android development container
echo Starting Android development container...
docker run -it --rm ^
    -v "%cd%":/workspace ^
    -p 8080:8080 ^
    -p 8081:8081 ^
    -p 8082:8082 ^
    -p 8083:8083 ^
    -p 8084:8084 ^
    -p 5555:5555 ^
    -p 5556:5556 ^
    --name android-dev-container ^
    mobile-android-dev

echo Android Development Environment started!
echo Access IDE at: http://localhost:8080
pause
"""
        
        with open('start-android-dev.bat', 'w') as f:
            f.write(android_bat)
        
        return {"success": True, "scripts": [
            "start-android-dev.sh", "start-ios-dev.sh", "start-flutter-dev.sh",
            "start-android-dev.bat"
        ]}
    
    def get_mobile_status(self):
        """Get status of mobile development tools"""
        status = {
            "available_tools": len(self.available_tools),
            "tools": self.available_tools,
            "platform": self.platform,
            "architecture": self.architecture
        }
        
        # Check for BlueStacks
        bluestacks_paths = [
            "/c/Program Files/BlueStacks_nxt",
            "/c/Program Files (x86)/BlueStacks_nxt",
            "C:\\Program Files\\BlueStacks_nxt",
            "C:\\Program Files (x86)\\BlueStacks_nxt"
        ]
        
        bluestacks_found = False
        for path in bluestacks_paths:
            if os.path.exists(path):
                bluestacks_found = True
                break
        
        status["bluestacks"] = bluestacks_found
        
        return status

def main():
    """Test mobile development system"""
    print("Testing Mobile Development System...")
    
    mobile_system = MobileDevelopmentSystem()
    
    # Test detection
    print(f"\nAvailable tools: {mobile_system.available_tools}")
    
    # Test status
    status = mobile_system.get_mobile_status()
    print(f"\nMobile development status: {status}")
    
    # Test BlueStacks integration
    print("\nTesting BlueStacks integration...")
    bluestacks_result = mobile_system.create_bluestacks_integration()
    print(f"BlueStacks integration: {bluestacks_result}")
    
    # Test mobile docker compose
    print("\nTesting mobile docker compose...")
    compose_result = mobile_system.create_mobile_docker_compose()
    print(f"Mobile docker compose: {compose_result}")
    
    # Test startup scripts
    print("\nTesting startup scripts...")
    scripts_result = mobile_system.create_mobile_startup_scripts()
    print(f"Startup scripts: {scripts_result}")
    
    print("\nMobile Development System test complete!")

if __name__ == "__main__":
    main()
