FROM mcr.microsoft.com/devcontainers/cpp:debian

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        libboost-all-dev \
        libssl-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*
