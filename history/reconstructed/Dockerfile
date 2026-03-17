# =============================================================================
# RawrXD-Shell Production Dockerfile
# Multi-stage: Build context + Runtime
# Spec: tools.instructions.md §5 — Deployment and Isolation
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1: Build context (copy sources, install dependencies)
# ---------------------------------------------------------------------------
FROM mcr.microsoft.com/powershell:lts-alpine-3.18 AS build

WORKDIR /build

# Copy only what's needed — exclude build artifacts, docs, test data
COPY modules/        /build/modules/
COPY src/             /build/src/
COPY include/         /build/include/
COPY config/          /build/config/
COPY scripts/         /build/scripts/
COPY bin/             /build/bin/
COPY *.ps1            /build/
COPY *.psm1           /build/
COPY config.example.json /build/
COPY package.json     /build/

# Validate critical files exist
RUN test -f /build/RawrXD.ps1 && \
    test -d /build/modules && \
    echo "Build context validated"

# ---------------------------------------------------------------------------
# Stage 2: Production runtime
# ---------------------------------------------------------------------------
FROM mcr.microsoft.com/powershell:lts-alpine-3.18 AS runtime

LABEL maintainer="RawrXD-Shell Team" \
      version="7.4.0" \
      description="RawrXD-Shell — Advanced GGUF Model Loader with Live Hotpatching & Agentic Correction"

# Security: run as non-root
RUN addgroup -S rawrxd && adduser -S rawrxd -G rawrxd

# Install minimal runtime deps (curl for health checks, jq for JSON)
RUN apk add --no-cache curl jq tini

WORKDIR /app

# Copy validated build context
COPY --from=build /build/ /app/

# Create required directories with correct ownership
RUN mkdir -p /app/logs /app/config /app/data /app/models /app/temp && \
    chown -R rawrxd:rawrxd /app

# Copy default config if not mounted
RUN cp -n /app/config.example.json /app/config/config.json 2>/dev/null || true

# Environment: production defaults
ENV POWERSHELL_TELEMETRY_OPTOUT=1 \
    RAWRXD_ENV=production \
    RAWRXD_LOG_LEVEL=INFO \
    RAWRXD_LOG_FORMAT=json \
    RAWRXD_METRICS_PORT=9090 \
    RAWRXD_HTTP_PORT=8080 \
    LAZY_INIT_IDE_ROOT=/app \
    DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1

# Expose service ports
EXPOSE 8080 9090

# Volumes for persistent state
VOLUME ["/app/logs", "/app/config", "/app/data", "/app/models"]

# Health check: verify the core module loads and service responds
HEALTHCHECK --interval=30s --timeout=10s --start-period=40s --retries=3 \
    CMD pwsh -NoProfile -c "Import-Module /app/modules/RawrXD.Core.psm1 -ErrorAction Stop; Write-Output 'healthy'" || exit 1

# Security: drop to non-root
USER rawrxd

# Use tini as PID 1 for proper signal handling
ENTRYPOINT ["/sbin/tini", "--"]
CMD ["pwsh", "-NoLogo", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "/app/RawrXD.ps1"]
