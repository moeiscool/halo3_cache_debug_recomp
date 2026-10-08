# Build environment for the Linux version: the compiler, CMake, Ninja and the
# ReXGlue SDK, so nothing has to be installed on the machine you build on.
#
# The image holds tools only. Your repository and your game executable are
# mounted when it runs (scripts/docker_build_linux.sh does both), so the image
# is built once and reused, and nothing of the game ever goes into it.
#
# Ubuntu 24.04's own Clang and C++ runtime are new enough for the SDK, so
# everything comes from Ubuntu's archive and the SDK's release.
#
# What is built here runs where the SDK's prebuilt libraries run: glibc 2.35
# or newer with GCC 13's C++ runtime or newer (Ubuntu 24.04 and newer, Debian
# 13, Fedora 39 and newer, Arch, SteamOS).
#
# BASE_IMAGE: an Ubuntu 24.04 image of your own, for example one that trusts a
# company proxy's certificate authority.
ARG BASE_IMAGE=ubuntu:24.04
FROM ${BASE_IMAGE}

ARG DEBIAN_FRONTEND=noninteractive
ARG CLANG_VERSION=18
ARG REXGLUE_SDK_VERSION=0.10.0
ARG REXGLUE_SDK_SHA256=0388edb3f8fb1444f1b0ad5d440cb02d9690ac20a76b04fd6b7dcc19f59328a6

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl unzip \
        cmake ninja-build clang-${CLANG_VERSION} libstdc++-14-dev \
        libx11-dev libx11-xcb-dev libxcb1-dev libwayland-dev \
 && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-${CLANG_VERSION} 100 \
 && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-${CLANG_VERSION} 100 \
 && rm -rf /var/lib/apt/lists/*

# The ReXGlue SDK release, checked against its known checksum.
RUN curl -fL --retry 3 -o /tmp/sdk.zip \
        "https://github.com/rexglue/rexglue-sdk/releases/download/v${REXGLUE_SDK_VERSION}/rexglue-sdk-${REXGLUE_SDK_VERSION}-linux-amd64.zip" \
 && echo "${REXGLUE_SDK_SHA256}  /tmp/sdk.zip" | sha256sum -c - \
 && mkdir -p /tmp/sdk && unzip -q /tmp/sdk.zip -d /tmp/sdk \
 && mv /tmp/sdk/linux-amd64 /opt/rexglue-sdk \
 && rm -rf /tmp/sdk /tmp/sdk.zip \
 && /opt/rexglue-sdk/bin/rexglue --version

ENV REXGLUE_SDK=/opt/rexglue-sdk \
    CC=clang \
    CXX=clang++ \
    HOME=/tmp

WORKDIR /src
