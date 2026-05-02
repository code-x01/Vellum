# ── Stage 1: Build Crow from source ──────────────────────────────────────────
FROM ubuntu:22.04 AS crow-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN git clone --depth 1 https://github.com/CrowCpp/Crow.git /crow

# ── Stage 2: Build frontend ───────────────────────────────────────────────────
FROM node:18-alpine AS frontend-builder

WORKDIR /frontend
COPY frontend/package*.json ./
RUN npm ci --silent

COPY frontend/ ./
RUN npm run build

# ── Stage 3: Build Vellum C++ backend ────────────────────────────────────────
FROM ubuntu:22.04 AS cpp-builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    pkg-config \
    libboost-all-dev \
    libssl-dev \
    linux-headers-generic \
    && rm -rf /var/lib/apt/lists/*

# Install Crow headers
COPY --from=crow-builder /crow/include /usr/local/include

WORKDIR /app
COPY CMakeLists.txt ./
COPY src/ ./src/
COPY include/ ./include/

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ── Stage 4: Runtime image ────────────────────────────────────────────────────
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libssl3 \
    # KVM tools for diagnostics
    cpu-checker \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Backend binary
COPY --from=cpp-builder /app/build/vellum ./

# Frontend assets
COPY --from=frontend-builder /frontend/build ./frontend/build/

EXPOSE 8080

# Note: requires --privileged (or --device /dev/kvm) for KVM access
CMD ["./vellum"]