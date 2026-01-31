# Sunshine Engine - Digital Ocean Deployment
FROM ubuntu:22.04

# Install system dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libgl1-mesa-dev \
    libglfw3-dev \
    libglm-dev \
    libstb-dev \
    pkg-config \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Install Java for AI tools
RUN apt-get update && apt-get install -y \
    openjdk-11-jdk \
    maven \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Create build directory
RUN mkdir -p build

# Build the project
WORKDIR /app/build
RUN cmake .. && make -j$(nproc)

# Create directories for generated content
RUN mkdir -p /app/generated_maps /app/generated_bots /app/generated_loot

# Expose port for HTTP bot
EXPOSE 8080

# Set environment variables
ENV SUNSHINE_ENGINE_MODE=production
ENV GENERATION_INTERVAL=5000

# Start the HTTP bot and self-watering generators
CMD ["./bin/self_watering_generators"]