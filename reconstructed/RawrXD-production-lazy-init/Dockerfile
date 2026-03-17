# Production Deployment: Dockerfile

FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    qt6-base-dev \
    qt6-tools-dev \
    libqt6charts6-dev \
    libqt6httpserver6-dev \
    libvulkan-dev \
    libcurl4-openssl-dev \
    libzstd-dev \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /build

# Copy source code
COPY . .

# Configure CMake with production settings
RUN cmake -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/rawrxd \
    -DENABLE_VULKAN=ON \
    -DENABLE_MASM_INTEGRATION=OFF \
    -DTEST_BRUTAL=OFF

# Build the application
RUN cmake --build build --config Release --target RawrXD-AgenticIDE -j$(nproc)

# Install to staging directory
RUN cmake --install build --prefix /opt/rawrxd

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    qt6-base-dev \
    libqt6charts6 \
    libqt6httpserver6 \
    libvulkan1 \
    libcurl4 \
    libzstd1 \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for security
RUN useradd -m -u 1000 -s /bin/bash rawrxd

# Copy application from builder
COPY --from=builder /opt/rawrxd /opt/rawrxd

# Copy production configuration
COPY config/production.json /opt/rawrxd/config/production.json

# Set up directories with proper permissions
RUN mkdir -p /opt/rawrxd/logs /opt/rawrxd/models /opt/rawrxd/data && \
    chown -R rawrxd:rawrxd /opt/rawrxd

# Switch to non-root user
USER rawrxd

# Set environment variables
ENV RAWRXD_ENV=production
ENV PATH="/opt/rawrxd/bin:${PATH}"
ENV QT_QPA_PLATFORM=offscreen
ENV LD_LIBRARY_PATH="/opt/rawrxd/lib:${LD_LIBRARY_PATH}"

# Expose Prometheus metrics port
EXPOSE 9090

# Health check
HEALTHCHECK --interval=60s --timeout=5s --start-period=30s --retries=3 \
    CMD curl -f http://localhost:9090/metrics || exit 1

# Set resource limits (optional, can be overridden by orchestrator)
# Default: 8GB memory, 80% CPU
LABEL memory.limit="8192m"
LABEL cpu.limit="80"

# Working directory
WORKDIR /opt/rawrxd

# Entrypoint
CMD ["/opt/rawrxd/bin/RawrXD-AgenticIDE", "--config", "/opt/rawrxd/config/production.json"]
