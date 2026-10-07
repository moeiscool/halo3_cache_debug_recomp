#!/usr/bin/env bash
# Build halo3_cache_debug for Linux (x86-64) from your own copy of the game.
#
#   bash scripts/build_linux.sh --xex /path/to/halo3/halo3_cache_debug.xex [--play]
#
# Every step is skipped when its result is already there, so run the same
# command again after a failure or after `git pull`.
#
#   --xex PATH        your halo3_cache_debug.xex. The manifest reads it from
#                     ../assets/ (next to this repository, as on Windows);
#                     the script links it there if it is not there yet.
#   --sdk-dir DIR     use a ReXGlue SDK v0.10.0 you built or unpacked
#                     yourself, instead of downloading the release
#   --config NAME     release (default), debug or relwithdebinfo
#   --play DIR        start the game when the build is done, from DIR (the
#                     folder with the game's data); default: the xex's folder
#   --jobs N          parallel compile jobs (default: all cores)
#
# Needs: cmake (3.25+), ninja, clang (18 or newer), curl, unzip, sha256sum, and
# to run, a Vulkan driver (see README.md, "Building and running (Linux)").
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$here/.." && pwd)
project=halo3_cache_debug
sdk_version=0.10.0
sdk_zip_url="https://github.com/rexglue/rexglue-sdk/releases/download/v$sdk_version/rexglue-sdk-$sdk_version-linux-amd64.zip"
sdk_zip_sha256=0388edb3f8fb1444f1b0ad5d440cb02d9690ac20a76b04fd6b7dcc19f59328a6

xex= sdk_dir= config=release play= play_dir= jobs=$(nproc)
while [ $# -gt 0 ]; do
    case $1 in
        --xex) xex=$2; shift 2 ;;
        --sdk-dir) sdk_dir=$2; shift 2 ;;
        --config) config=$2; shift 2 ;;
        --play)
            play=1
            if [ $# -gt 1 ] && [ "${2#--}" = "$2" ]; then play_dir=$2; shift; fi
            shift ;;
        --jobs) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,21p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1 (see --help)" >&2; exit 2 ;;
    esac
done
case $config in release|debug|relwithdebinfo) ;; *) echo "--config: release, debug or relwithdebinfo" >&2; exit 2 ;; esac

step() { printf '\n==> %s\n' "$*"; }
fail() { printf '\nFAILED: %s\n' "$*" >&2; exit 1; }

# ---------------------------------------------------------------------------
step "1/5 tools"
missing=
for tool in cmake ninja clang clang++ curl unzip sha256sum; do
    command -v "$tool" > /dev/null || missing="$missing $tool"
done
[ -z "$missing" ] || fail "missing:$missing (README.md lists the packages for your distribution)"
echo "cmake $(cmake --version | head -1 | awk '{print $3}'), $(clang++ --version | head -1)"

# ---------------------------------------------------------------------------
step "2/5 ReXGlue SDK $sdk_version"
if [ -z "$sdk_dir" ]; then
    sdk_dir="$repo/third_party/rexglue-sdk-$sdk_version-linux-amd64"
    if [ ! -x "$sdk_dir/bin/rexglue" ]; then
        mkdir -p "$repo/third_party"
        zip="$repo/third_party/rexglue-sdk-$sdk_version-linux-amd64.zip"
        echo "downloading $sdk_zip_url"
        curl -fL --retry 3 -o "$zip" "$sdk_zip_url" || fail "SDK download"
        echo "$sdk_zip_sha256  $zip" | sha256sum -c --quiet - || fail "the SDK download does not match its expected checksum"
        unpack="$repo/third_party/.unpack"
        rm -rf -- "$unpack"; mkdir -p "$unpack"
        unzip -q "$zip" -d "$unpack" || fail "SDK unpack"
        rm -rf -- "$sdk_dir"
        mv "$unpack/linux-amd64" "$sdk_dir"
        rm -rf -- "$unpack" "$zip"
    fi
fi
[ -x "$sdk_dir/bin/rexglue" ] || fail "no bin/rexglue in $sdk_dir"
[ -f "$sdk_dir/lib/cmake/rexglue/rexglueConfig.cmake" ] || fail "no lib/cmake/rexglue in $sdk_dir"
echo "SDK: $sdk_dir"

# ---------------------------------------------------------------------------
step "3/5 your game executable"
assets="$(dirname -- "$repo")/assets"
target_xex="$assets/$project.xex"
if [ -n "$xex" ]; then
    [ -f "$xex" ] || fail "no such file: $xex"
    xex=$(cd -- "$(dirname -- "$xex")" && pwd)/$(basename -- "$xex")
    if [ ! -e "$target_xex" ]; then
        mkdir -p "$assets"
        ln -s "$xex" "$target_xex"
        echo "linked $target_xex -> $xex"
    fi
fi
[ -f "$target_xex" ] || fail "no $target_xex: give your executable with --xex"
[ -n "$play_dir" ] || play_dir=$(dirname -- "$(readlink -f "$target_xex")")

# ---------------------------------------------------------------------------
step "4/5 recompile the game's code"
# Before configuring: CMake picks up the generated source list when it configures.
( cd "$repo" && "$sdk_dir/bin/rexglue" codegen "$project"_manifest.toml ) || fail "codegen"
ls "$repo"/generated/*.cpp > /dev/null 2>&1 || fail "codegen produced no sources in generated/"

# ---------------------------------------------------------------------------
step "5/5 build ($config)"
preset="linux-amd64-$config"
build="$repo/out/build/$preset"
cmake --preset "$preset" -S "$repo" -DCMAKE_PREFIX_PATH="$sdk_dir" > /dev/null || fail "configure (cmake --preset $preset)"
cmake --build "$build" --parallel "$jobs" || fail "build"
exe="$build/$project"
[ -x "$exe" ] || fail "no executable at $exe"
for library in librexruntime.so librexgpu-xenos.so; do
    [ -f "$build/$library" ] || fail "$library was not staged next to the executable"
done
echo "built: $exe"

if [ -z "$play" ]; then
    cat <<EOF

Run it from the folder with your game's data (it uses the current folder for
the game, saves, shader cache and halo3_cache_debug.toml):

  cd "$play_dir" && "$exe"
EOF
    exit 0
fi
step "starting the game in $play_dir"
cd -- "$play_dir"
exec "$exe"
