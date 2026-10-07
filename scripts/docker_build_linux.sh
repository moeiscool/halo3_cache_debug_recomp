#!/usr/bin/env bash
# Build the Linux version inside Docker, so the only thing the host needs is
# Docker itself (no compiler, CMake or SDK).
#
#   bash scripts/docker_build_linux.sh --xex /path/to/halo3/halo3_cache_debug.xex
#
# The first run builds the image (docker/linux/Dockerfile: the toolchain and
# the ReXGlue SDK, a few minutes once). Later runs reuse it and rebuild only
# what changed. The result is out/linux/halo3_cache_debug, a folder that runs on
# x86-64 Linux with a Vulkan driver, glibc 2.35+ and GCC 13's C++ runtime or
# newer, as the SDK requires (Ubuntu 24.04+, Debian 13, Fedora 39+, Arch,
# SteamOS).
#
#   --xex PATH        your halo3_cache_debug.xex (read only, never copied
#                     into the image); not needed again once remembered
#   --config NAME     release (default), debug or relwithdebinfo
#   --rebuild-image   build the image again (after docker/linux changes)
#   --jobs N          parallel compile jobs (default: all cores)
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$here/.." && pwd)
project=halo3_cache_debug
image=${HALO3_BUILD_IMAGE:-halo3-linux-build}
remembered="$repo/out/docker/xex-path"

xex= config=release rebuild_image= jobs=
while [ $# -gt 0 ]; do
    case $1 in
        --xex) xex=$2; shift 2 ;;
        --config) config=$2; shift 2 ;;
        --rebuild-image) rebuild_image=1; shift ;;
        --jobs) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1 (see --help)" >&2; exit 2 ;;
    esac
done

fail() { printf '\nFAILED: %s\n' "$*" >&2; exit 1; }
command -v docker > /dev/null || fail "docker is not installed"
docker info > /dev/null 2>&1 || fail "cannot reach the Docker daemon (is it running, and may this user use it?)"

# The executable: given, or remembered from an earlier run.
mkdir -p "$repo/out/docker"
if [ -n "$xex" ]; then
    [ -f "$xex" ] || fail "no such file: $xex"
    xex=$(cd -- "$(dirname -- "$xex")" && pwd)/$(basename -- "$xex")
    echo "$xex" > "$remembered"
elif [ -f "$remembered" ]; then
    xex=$(cat "$remembered")
fi
[ -n "$xex" ] || fail "give your executable with --xex"
[ -f "$xex" ] || fail "no such file: $xex (give it again with --xex)"
[ "$(basename -- "$xex")" = "$project.xex" ] || fail "the executable must be named $project.xex"

if [ -n "$rebuild_image" ] || ! docker image inspect "$image" > /dev/null 2>&1; then
    echo "==> building the image $image (once)"
    docker build -t "$image" "$repo/docker/linux"
fi

# The repository at /src, the executable's folder at /assets (where the
# manifest looks for it), files written as you rather than as root.
docker run --rm -t \
    --user "$(id -u):$(id -g)" \
    -v "$repo:/src" \
    -v "$(dirname -- "$xex"):/assets:ro" \
    ${jobs:+-e JOBS="$jobs"} \
    "$image" bash /src/docker/linux/build-in-container.sh "$config"

cat <<EOF

Run it from the folder with your game's data:

  cd "$(dirname -- "$xex")" && "$repo/out/linux/$project/$project"
EOF
