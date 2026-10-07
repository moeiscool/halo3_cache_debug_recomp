#!/usr/bin/env bash
# One command from your own copy of the game to Halo 3 installed on a PS5.
#
#   bash ps5/make_ps5.sh --game-dir /path/to/halo3 [--console 192.168.1.50]
#
# Adapted from mcla-recomp's ps5/make_ps5.sh (holdmysocks/mcla-recomp,
# GPL-3.0-or-later).
#
# Host: Arch Linux, as root (a fresh Arch under WSL2 on Windows works; see
# ps5/README.md). Everything is built on your machine from your own copy of
# the game: nothing of the game is in this repository, and what this script
# produces from it is yours to keep, not to share.
#
# Every step is skipped when its result is already there, so the script can be
# run again after a failure, or after a change, and continues from that step.
#
#   --game-dir DIR    your Halo 3 08172.07.03.08.2240.delta build: the folder
#                     with halo3_cache_debug.xex and the game's data (maps
#                     and the rest), as you run it on the desktop
#   --console IP      upload the title and the game data to the console (FTP)
#   --ftp-port N      the console's FTP port (default 2121)
#   --title-id ID     the title's id on the console (default PPSA99783)
#   --tile IMAGE      your own picture for the home-screen tile (any common
#                     format; it is resized to 512x512)
#   --art-dir DIR     your own tile and backgrounds in the console's formats
#                     (icon0.png, pic0.dds, pic1.dds, snd0.at9)
#   --test-build      keep the test scaffolding (waits for a log connection
#                     from ps5/title_log_client.py)
#   --jobs N          parallel compile jobs (default: all cores)
#
# Locations, changeable through the environment:
#   PS5VK   (/root/ps5vk)   the Vulkan driver project and its toolchain
#   WORK    (/root/halo3)   the SDK checkout, generated code and build trees
set -euo pipefail

here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$here/.." && pwd)
PS5VK=${PS5VK:-/root/ps5vk}
WORK=${WORK:-/root/halo3}
driver="$PS5VK/PS5_Vulkan"
sdk="$driver/.deps/native/ps5-payload-sdk"
rex_src="$WORK/rexglue-sdk"
rex_build="$WORK/build-ps5"
rex_host="$WORK/build-host"
gen_dir="$WORK/generated"
# The SDK release the PS5 patches are made against. The desktop build may pin
# a different one (halo3_cache_debug_manifest.toml); the PS5 build keeps its
# own checkout and generated code, so the two do not interfere.
rex_tag=v0.10.0
rex_commit=f5337cdc947ff6d4c4196737e2c807a48f2a1fc2
project=halo3_cache_debug
xex=$project.xex

game_dir= console= ftp_port=2121 title_id=PPSA99783 art_dir= tile= play=1 jobs=$(nproc)
while [ $# -gt 0 ]; do
    case $1 in
        --game-dir) game_dir=$2; shift 2 ;;
        --console) console=$2; shift 2 ;;
        --ftp-port) ftp_port=$2; shift 2 ;;
        --title-id) title_id=$2; shift 2 ;;
        --art-dir) art_dir=$2; shift 2 ;;
        --tile) tile=$2; shift 2 ;;
        --test-build) play=; shift ;;
        --jobs) jobs=$2; shift 2 ;;
        -h|--help) sed -n '2,36p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $1 (see --help)" >&2; exit 2 ;;
    esac
done

step() { printf '\n==> %s\n' "$*"; }
fail() { printf '\nFAILED: %s\n' "$*" >&2; exit 1; }
mkdir -p "$WORK"
logs="$WORK/logs"; mkdir -p "$logs"
# Run a long command with its output in a log; on failure show the end of it.
logged() {
    local name=$1; shift
    if ! "$@" > "$logs/$name.log" 2>&1; then
        tail -25 "$logs/$name.log" >&2
        fail "$name (full log: $logs/$name.log)"
    fi
}

[ "$(id -u)" -eq 0 ] || fail "run as root (the driver project's build installs packages and writes under /root)"
command -v pacman > /dev/null || fail "this script needs Arch Linux (pacman); see ps5/README.md"

# The game folder is remembered, so later runs need no --game-dir.
if [ -n "$game_dir" ]; then
    game_dir=$(cd -- "$game_dir" && pwd) || fail "no such folder: $game_dir"
    echo "$game_dir" > "$WORK/game-dir"
elif [ -f "$WORK/game-dir" ]; then
    game_dir=$(cat "$WORK/game-dir")
fi
[ -n "$game_dir" ] || fail "give your game folder with --game-dir"
[ -f "$game_dir/$xex" ] || fail "$game_dir has no $xex"

# ---------------------------------------------------------------------------
step "1/8 host packages"
# The X11 packages are for the recompiler's own build: it is a command-line
# tool, but the SDK configures its window library along with everything else.
logged packages pacman -Sy --noconfirm --needed base-devel clang llvm lld cmake ninja git curl rsync python \
    python-pillow unzip libx11 libxext libxi libxcursor libxrandr libxinerama libxss libxkbcommon \
    libxtst libxfixes libxrender mesa

# ---------------------------------------------------------------------------
step "2/8 PS5 toolchain and Vulkan driver (about 20 minutes the first time)"
if [ -f "$driver/.deps/native/radv-release/lib/libvulkan_radeon.ps5.a" ] && [ -x "$driver/build/host/ps5-native-tool" ]; then
    echo "already built: $driver"
else
    [ "$PS5VK" = /root/ps5vk ] || fail "the driver build script works in /root/ps5vk; leave PS5VK unset"
    bash "$here/build_ps5_vulkan_driver.sh" || true
    grep -q "ALL STEPS DONE" /root/ps5vk-arch.log || { tail -25 /root/ps5vk-arch.log >&2; fail "driver build (log: /root/ps5vk-arch.log)"; }
fi
[ -f "$sdk/toolchain/prospero.cmake" ] || fail "the PS5 toolchain is missing under $sdk"

# ---------------------------------------------------------------------------
step "3/8 ReXGlue SDK $rex_tag with the PS5 patches"
if [ ! -f "$rex_src/.ps5-patched" ]; then
    if [ ! -d "$rex_src/.git" ]; then
        logged sdk-clone git clone --recursive --branch "$rex_tag" https://github.com/rexglue/rexglue-sdk.git "$rex_src"
    fi
    [ "$(git -C "$rex_src" rev-parse HEAD)" = "$rex_commit" ] || fail "the SDK checkout is not $rex_tag ($rex_commit)"
    git -C "$rex_src" apply "$here/patches/rexglue-v0.10.0-ps5.patch" || fail "SDK patch does not apply (is the checkout clean?)"
    git -C "$rex_src/thirdparty/FFmpeg" apply "$here/patches/rexglue-ffmpeg-ps5-config.patch" || fail "FFmpeg patch does not apply"
    touch "$rex_src/.ps5-patched"
else
    echo "already patched: $rex_src"
fi

# ---------------------------------------------------------------------------
step "4/8 the recompiler (rexglue), for this machine"
rexglue=$(find "$rex_host" "$rex_src/out" -maxdepth 4 -name rexglue -type f -perm -u+x 2> /dev/null | head -1 || true)
if [ -z "$rexglue" ]; then
    logged host-configure cmake -S "$rex_src" -B "$rex_host" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_C_FLAGS=-march=x86-64-v2 -DCMAKE_CXX_FLAGS=-march=x86-64-v2 \
        -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_BUILD_TESTS=OFF
    logged host-build ninja -C "$rex_host" -j "$jobs" rexglue
    rexglue=$(find "$rex_host" "$rex_src/out" -maxdepth 4 -name rexglue -type f -perm -u+x | head -1)
fi
[ -x "$rexglue" ] || fail "the recompiler was not built"
echo "recompiler: $rexglue"

# ---------------------------------------------------------------------------
step "5/8 recompile the game's code"
# A manifest of the PS5 build's own: the repository's, with the game, the
# function list and the output folder as absolute paths (the desktop
# manifest's are relative to it and point elsewhere).
manifest="$WORK/$project.manifest.toml"
python3 - "$repo/${project}_manifest.toml" "$manifest" "$game_dir/$xex" "$gen_dir" "$repo" <<'PY' || fail "could not write the PS5 manifest"
import os, sys, tomllib
source, target, xex, out_dir, repo = sys.argv[1:]
with open(source, "rb") as f:
    manifest = tomllib.load(f)
entry = manifest["entrypoint"]
includes = [os.path.join(repo, "config", os.path.basename(p)) for p in entry.get("includes", [])]
lines = ["# Written by ps5/make_ps5.sh from %s; do not edit." % os.path.basename(source),
         "[project]", 'name = "%s"' % manifest["project"]["name"], "",
         "[entrypoint]", 'file_path = "%s"' % xex, 'out_directory_path = "%s"' % out_dir,
         "includes = [%s]" % ", ".join('"%s"' % p for p in includes)]
for key in ("setjmp_address", "longjmp_address"):
    if key in entry:
        lines.append("%s = 0x%08X" % (key, entry[key]))
open(target, "w").write("\n".join(lines) + "\n")
PY
# The recompiled code depends on the function list in config/ and on the
# recompiler: regenerate when either is newer than the result.
stamp="$gen_dir/.generated"
if [ ! -f "$stamp" ] || [ -n "$(find "$repo/config" "$repo/${project}_manifest.toml" "$rexglue" "$game_dir/$xex" -newer "$stamp" -print -quit)" ]; then
    ( cd "$WORK" && logged codegen "$rexglue" codegen "$manifest" )
    grep -q 'REX_PLATFORM_PS5' "$gen_dir/${project}_pch.h" || fail "the generated header lacks the PS5 rule (unpatched recompiler?)"
    touch "$stamp"
else
    echo "generated code is up to date"
fi
echo "generated sources: $(ls "$gen_dir"/*.cpp | wc -l)"

# ---------------------------------------------------------------------------
step "6/8 the runtime, for PS5"
if [ ! -f "$rex_build/build.ninja" ]; then
    # -flto=thin: 5-8% more draws a second on the console (measured by mcla-recomp).
    logged ps5-configure cmake -S "$rex_src" -B "$rex_build" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$sdk/toolchain/prospero.cmake" \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=23 \
        -DCMAKE_C_FLAGS="-march=znver2 -flto=thin" \
        -DCMAKE_CXX_FLAGS="-march=znver2 -fexperimental-library -flto=thin" \
        -DREXGLUE_USE_D3D12=OFF -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_TRACY=OFF
fi
logged ps5-runtime ninja -C "$rex_build" -j "$jobs" rexruntime rexgpu-xenos

# ---------------------------------------------------------------------------
step "7/8 the title's tile"
# No art is made from the game here: without --tile or --art-dir the title has
# the driver project's default tile.
if [ -n "$tile" ]; then
    [ -f "$tile" ] || fail "no such file: $tile"
    own="$WORK/title-art-own"
    rm -rf -- "$own"; mkdir -p "$own"
    [ -z "$art_dir" ] || cp -r "$art_dir"/. "$own"/
    python3 - "$tile" "$own/icon0.png" <<'PY' || fail "the tile could not be converted"
import sys
from PIL import Image
Image.open(sys.argv[1]).convert("RGBA").resize((512, 512), Image.LANCZOS).save(sys.argv[2])
PY
    art_dir=$own
fi
if [ -n "$art_dir" ]; then
    [ -d "$art_dir" ] || fail "no such folder: $art_dir"
    echo "art: $art_dir"
else
    echo "no --tile or --art-dir: the driver project's default tile"
fi

# ---------------------------------------------------------------------------
step "8/8 the game for PS5 (10 to 30 minutes the first time)"
export PS5_VULKAN="$driver" REX_SRC="$rex_src" REX_BUILD="$rex_build" GEN_DIR="$gen_dir" \
    HALO3_PS5_WORK="$WORK/game-ps5" JOBS="$jobs"
export TITLE="$title_id" PS5_TITLE_NAME="Halo 3" ART_DIR="$art_dir"
if [ -n "$play" ]; then
    export PS5_PLAY=1 PS5_LOG_LEVEL=warning
else
    export PS5_RUN_SECONDS=3600
fi
logged game-build bash "$here/game/build.sh" 7
dist="$driver/dist/$title_id"
[ -f "$dist/eboot.bin" ] || fail "no eboot.bin in $dist"
out="$repo/out/ps5-title/$title_id"
rm -rf -- "$out"; mkdir -p "$(dirname "$out")"; cp -r "$dist" "$out"
echo "title: $out ($(du -sh "$out" | cut -f1))"

# ---------------------------------------------------------------------------
if [ -z "$console" ]; then
    cat <<EOF

No --console given, so nothing was uploaded. To install by hand, copy
  $out            ->  /data/homebrew/$title_id   on the console
  $game_dir       ->  /data/halo3/game          on the console
and see ps5/README.md, "On the console".
EOF
    exit 0
fi
step "upload to the console"
ftp="ftp://$console:$ftp_port"
curl -s --max-time 15 "$ftp/data/" > /dev/null || fail "no FTP server at $console:$ftp_port (start one on the console first)"
# The size of a file on the console; nothing if it is not there (curl fails
# then, which must not end the script).
remote_size_of() {
    { curl -s --max-time 30 -I "$ftp$1" 2> /dev/null || true; } | tr -d '\r' | awk '/Content-Length/{print $2}'
}
# Upload a file unless one of the same size is already there.
put() {
    local source=$1 target=$2 local_size remote_size
    local_size=$(stat -c %s "$source")
    remote_size=$(remote_size_of "$target")
    if [ "$local_size" = "$remote_size" ] && [ "${3:-}" != always ]; then
        echo "  same size, kept: $target"
        return
    fi
    echo "  uploading $target ($local_size bytes)"
    # Three tries: a console's FTP server now and then drops a long transfer.
    local attempt
    for attempt in 1 2 3; do
        curl -s -S --ftp-create-dirs -T "$source" "$ftp$target" && break
        [ "$attempt" -lt 3 ] || fail "upload of $target (run the same command again to continue; files already there are kept)"
        echo "  retrying $target"
        sleep 10
    done
    remote_size=$(remote_size_of "$target")
    [ "$local_size" = "$remote_size" ] || fail "$target is $remote_size bytes on the console, $local_size here"
}
echo "game data to /data/halo3/game"
( cd "$game_dir" && find . -type f | sed 's#^\./##' | sort ) | while read -r file; do
    put "$game_dir/$file" "/data/halo3/game/$file"
done
echo "title to /data/homebrew/$title_id"
( cd "$out" && find . -type f | sed 's#^\./##' | sort ) | while read -r file; do
    put "$out/$file" "/data/homebrew/$title_id/$file" always
done
cat <<EOF

Done. On the console, "Halo 3" appears on the home screen once your homebrew
mounter has picked up /data/homebrew/$title_id (see ps5/README.md).
EOF
