FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libasound2-dev \
    libx11-dev \
    libxinerama-dev \
    libxext-dev \
    libxrandr-dev \
    libxcursor-dev \
    libfreetype6-dev \
    libgl1-mesa-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
