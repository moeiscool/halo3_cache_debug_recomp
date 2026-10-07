#!/usr/bin/env bash
# Runs inside the docker/linux image (scripts/docker_build_linux.sh starts it).
# The repository is mounted at /src and your game executable's folder at
# /assets, which is where the manifest looks for it (../assets from /src).
#
# Builds into out/docker/linux-amd64-<config> and puts a folder you can run
# from anywhere into out/linux/halo3_cache_debug: the executable with the
# runtime and GPU plugin beside it. The host's own C++ runtime is used: one
# shipped with the game would be older than what the host's Vulkan driver,
# loaded into the same process, may need.
set -euo pipefail

config=${1:-release}
case $config in
    release) build_type=Release ;;
    debug) build_type=Debug ;;
    relwithdebinfo) build_type=RelWithDebInfo ;;
    *) echo "config: release, debug or relwithdebinfo" >&2; exit 2 ;;
esac
project=halo3_cache_debug
sdk=${REXGLUE_SDK:-/opt/rexglue-sdk}
build=/src/out/docker/linux-amd64-$config
dist=/src/out/linux/$project

step() { printf '\n==> %s\n' "$*"; }
fail() { printf '\nFAILED: %s\n' "$*" >&2; exit 1; }

[ -f /src/${project}_manifest.toml ] || fail "the repository is not mounted at /src"
[ -f /assets/$project.xex ] || fail "no /assets/$project.xex: mount the folder with your executable at /assets"

step "1/3 recompile the game's code"
# Before configuring: CMake picks up the generated source list when it configures.
( cd /src && "$sdk/bin/rexglue" codegen ${project}_manifest.toml ) || fail "codegen"
ls /src/generated/*.cpp > /dev/null 2>&1 || fail "codegen produced no sources in generated/"

step "2/3 build ($config)"
cmake -S /src -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=$build_type \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_PREFIX_PATH="$sdk" > /dev/null \
    || fail "configure"
cmake --build "$build" --parallel "${JOBS:-$(nproc)}" || fail "build"

step "3/3 package into out/linux/$project"
rm -rf -- "$dist"; mkdir -p "$dist"
cp "$build/$project" "$build/librexruntime.so" "$build/librexgpu-xenos.so" "$dist/"
echo "done: out/linux/$project ($(du -sh "$dist" | cut -f1))"
