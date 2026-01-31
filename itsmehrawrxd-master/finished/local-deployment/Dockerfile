# RawrZ Security Platform - Airtight Environment Dockerfile
FROM ubuntu:22.04

# Set working directory
WORKDIR /app

# Install Node.js 18 and system dependencies
RUN apt-get update && apt-get install -y \
    curl \
    ca-certificates \
    gnupg \
    && curl -fsSL https://deb.nodesource.com/setup_18.x | bash - \
    && apt-get install -y nodejs \
    && apt-get install -y \
    python3 \
    python3-pip \
    python3-dev \
    build-essential \
    make \
    g++ \
    gcc \
    libc6-dev \
    libffi-dev \
    libssl-dev \
    libsqlite3-dev \
    imagemagick \
    graphicsmagick \
    libvips-dev \
    libpng-dev \
    libjpeg-dev \
    libgif-dev \
    libwebp-dev \
    librsvg2-dev \
    libpango1.0-dev \
    libcairo2-dev \
    libpixman-1-dev \
    libgdk-pixbuf2.0-dev \
    libfreetype6-dev \
    libfontconfig1-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxtst-dev \
    libxi-dev \
    libgl1-mesa-dev \
    libglu1-mesa-dev \
    libxrandr-dev \
    libxss-dev \
    libgconf-2-4 \
    libxcomposite-dev \
    libxdamage-dev \
    libxfixes-dev \
    libxinerama-dev \
    libxcursor-dev \
    libasound2-dev \
    libpangocairo-1.0-0 \
    libatk1.0-0 \
    libcairo-gobject2 \
    libgtk-3-0 \
    libgdk-pixbuf2.0-0 \
    libxcomposite1 \
    libxcursor1 \
    libxdamage1 \
    libxext6 \
    libxfixes3 \
    libxi6 \
    libxrandr2 \
    libxrender1 \
    libxss1 \
    libxtst6 \
    ca-certificates \
    curl \
    wget \
    git \
    bash \
    sudo \
    nano \
    vim \
    htop \
    net-tools \
    iputils-ping \
    tcpdump \
    nmap \
    openssh-client \
    rsync \
    unzip \
    zip \
    tar \
    gzip \
    bzip2 \
    xz-utils \
    lz4 \
    zstd \
    jq \
    postgresql-client \
    redis-tools \
    && rm -rf /var/lib/apt/lists/*

# Install Node.js global packages for advanced features
RUN npm install -g \
    pm2 \
    nodemon \
    typescript \
    ts-node \
    @types/node \
    eslint \
    prettier \
    jest \
    mocha \
    chai \
    supertest \
    nyc \
    cross-env \
    concurrently \
    wait-port \
    http-server \
    serve \
    live-server

# Copy minimal package file for Docker build
COPY package-docker.json package.json

# Install core dependencies
RUN npm install --omit=dev --ignore-scripts --no-optional

# Copy application files
COPY . .

# Create necessary directories
RUN mkdir -p \
    /app/uploads \
    /app/downloads \
    /app/temp \
    /app/logs \
    /app/data \
    /app/keys \
    /app/stubs \
    /app/payloads \
    /app/bots \
    /app/cve \
    /app/engines \
    /app/backups

# Create node user and set proper permissions
RUN useradd -m -s /bin/bash node && \
    chmod -R 755 /app && \
    chown -R node:node /app

# Switch to non-root user
USER node

# Expose ports
EXPOSE 3000 3001 3002 8080 8081 8082

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:3000/api/health || exit 1

# Start the application
CMD ["node", "api-server-no-cli.js"]