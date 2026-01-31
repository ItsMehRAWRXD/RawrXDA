# RawrZ Native Compilation - Roslyn-for-Native System
# Builds native executables from C/C++ source entirely in memory

# ----------  build stage  ----------
FROM alpine:latest AS builder
RUN apk add --no-cache clang lld musl-dev curl gcc g++ make cmake

# ----------  serve stage  ----------
FROM alpine:latest
RUN apk add --no-cache python3 py3-pip nodejs npm curl wget

# Install additional compilation tools
RUN apk add --no-cache \
    gcc \
    g++ \
    make \
    cmake \
    nasm \
    yasm \
    clang \
    lld \
    musl-dev \
    linux-headers \
    binutils \
    mingw-w64-gcc \
    mingw-w64-crt \
    mingw-w64-winpthreads

WORKDIR /app

# Copy compilation script
COPY native-compile.sh /usr/local/bin/native-compile.sh
RUN chmod +x /usr/local/bin/native-compile.sh

# Copy Node.js server for API
COPY native-compile-server.js /app/server.js
COPY package.json /app/

# Install Node.js dependencies
RUN npm install

# Create output directory
RUN mkdir -p /app/output

EXPOSE 8080
CMD ["node", "server.js"]
