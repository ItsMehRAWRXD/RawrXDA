FROM openjdk:21-jdk-slim

# Install multiple language runtimes
RUN apt-get update && apt-get install -y \
    python3 python3-pip \
    nodejs npm \
    g++ \
    golang-go \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:${PATH}"

# Install .NET
RUN curl -sSL https://dot.net/v1/dotnet-install.sh | bash /dev/stdin --channel 8.0
ENV PATH="/root/.dotnet:${PATH}"

WORKDIR /app
COPY src/ ./src/
COPY package.json ./

# Compile Java
RUN javac src/pi-engine/PiEngine.java

EXPOSE 8080
CMD ["java", "-cp", "src/pi-engine", "PiEngine"]