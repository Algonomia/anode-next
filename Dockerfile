# AnodeServer Meta - All-in-one image
# Contains: C++ build tools, Node.js, meta server, client, source code
# The meta server orchestrates builds, server start/stop, and plugin management.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# C++ build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libpqxx-dev \
    libpq-dev \
    libboost-all-dev \
    libsqlite3-dev \
    ca-certificates \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Node.js 20.x
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source code
COPY CMakeLists.txt main.cpp ./
COPY src/ src/
COPY meta/ meta/
COPY examples/ examples/

# Pre-build C++ server
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
    && cmake --build build --target anodeServer -j$(nproc)

# Install client dependencies and build
RUN cd src/client && npm ci && npm run build

# Install meta server dependencies
RUN cd meta && npm ci --production

# Create data directory
RUN mkdir -p /app/data && chown -R 1000:1000 /app/data

EXPOSE 9090 8080

ENTRYPOINT ["node", "meta/server.js"]