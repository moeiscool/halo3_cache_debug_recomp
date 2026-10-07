#!/usr/bin/env bash
# Build halo3_cache_debug for PS5: the recompiled code, Halo 3's host sources
# (halo3/source and the hooks in source/), the PS5 host (main_ps5.cpp) and the
# runtime's static libraries.
#
# Adapted from mcla-recomp's ps5/game/build.sh (holdmysocks/mcla-recomp,
# GPL-3.0-or-later). Run on the Arch host after the runtime has been built for
# PS5 and codegen has run with the patched SDK (ps5/make_ps5.sh does both).
# Touches no console.
#
# Usage: build.sh [stage ...]     (default: 7)
#   without TITLE: a payload ELF per stage, $work/halo3-stage<N>.elf (stages 1-4)
#   TITLE=<TITLEID>: an installable title, <driver>/dist/<TITLEID> (see ps5/title_build.sh)
# Environment:
#   GEN_DIR      the codegen output (halo3_cache_debug_init.h and the sources)
#   PS5_PLAY=1   a build for playing: no wait for a log connection, no time
#                limit, no periodic diagnostics; the log is
#                /data/halo3/halo3-play.log on the console
#   PS5_RUN_SECONDS, PS5_LOG_LEVEL, PS5_TITLE_NAME, ART_DIR, JOBS
set -euo pipefail
here=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo=$(cd -- "$here/../.." && pwd)
driver=${PS5_VULKAN:-/root/ps5vk/PS5_Vulkan}
sdk="$driver/.deps/native/ps5-payload-sdk"
runtime_src=${REX_SRC:-/root/halo3/rexglue-sdk}
runtime_build=${REX_BUILD:-/root/halo3/build-ps5}
gen_dir=${GEN_DIR:-/root/halo3/generated}
libs="$runtime_src/out/ps5-amd64"
work=${HALO3_PS5_WORK:-/root/halo3/game-ps5}
jobs=${JOBS:-$(nproc)}
cxx="$sdk/bin/prospero-clang++"
project=halo3_cache_debug
mkdir -p "$work/obj" "$work/src/generated" "$work/src/repo" "$work/src/ps5"

[ -e "$gen_dir/${project}_init.h" ] || { echo "$gen_dir has no ${project}_init.h; run codegen first" >&2; exit 1; }

# A local copy of the sources, with Unix line endings: the repository may be on
# a Windows drive, which is slow to read from for every compile.
rsync -a --delete "$gen_dir/" "$work/src/generated/"
rsync -a --delete --include='*/' --include='*.cpp' --include='*.h' --exclude='*' \
    "$repo/halo3" "$repo/source" "$work/src/repo/"
find "$work/src/repo" -type f \( -name '*.cpp' -o -name '*.h' \) -exec sed -i 's/\r$//' {} +
for file in "$here"/main_ps5.cpp "$here"/ps5_pad_input.h "$here"/ps5_audio.h "$here"/../title_log.h "$here"/../log_fd_sink.h; do
    tr -d '\r' < "$file" > "$work/src/ps5/$(basename "$file").new"
    cmp -s "$work/src/ps5/$(basename "$file").new" "$work/src/ps5/$(basename "$file")" 2>/dev/null \
        && rm "$work/src/ps5/$(basename "$file").new" \
        || mv "$work/src/ps5/$(basename "$file").new" "$work/src/ps5/$(basename "$file")"
done

# The runtime's own defines and include paths, taken from its build; its
# precompiled header and float model are not wanted for the game code.
ninja -C "$runtime_build" -t commands rexruntime > "$work/commands.txt"
# grep -m1, not "grep | head -1": under pipefail the latter fails now and then.
command=$(grep -m1 'xmemory\.cpp\.o ' "$work/commands.txt")
inherited=$(printf '%s' "$command" | tr ' ' '\n' | grep -E '^(--sysroot=|-D|-I)' | grep -v -E '^-DNDEBUG$' | tr '\n' ' ')
inherited_system=$(printf '%s' "$command" | grep -o -E -- '-isystem [^ ]+' | tr '\n' ' ')
# -ffp-contract=off: the desktop build targets SSE4.1 and so never fuses a
# multiply and an add; with znver2 the compiler would, and guest floating
# point results would differ between the two builds.
# The include roots: the work folder for "generated/<project>_init.h", the
# generated folder for the recompiled sources' own headers, and Halo 3's two
# source roots as the desktop CMakeLists.txt has them.
flags="$inherited $inherited_system -march=znver2 -fexperimental-library -O3 -DNDEBUG -std=c++23 -fPIC \
  -mcmodel=large -fno-strict-aliasing -fno-char8_t -ffp-contract=off -g0 -w \
  -I$work/src -I$work/src/generated -I$work/src/repo -I$work/src/repo/source -I$work/src/repo/halo3/source"
# The generated header has its own copy of the rule for the 0xE0000000 physical
# range's 4 KiB host offset, and it must name PS5 (16 KiB pages) as the runtime
# does, or the game code and the runtime disagree about where that memory is
# (the game then stalls waiting for a GPU that never sees its commands). The
# SDK patch fixes the codegen template; an unpatched recompiler's header lacks it.
grep -q 'REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64) || REX_PLATFORM_PS5' \
    "$work/src/generated/${project}_pch.h" \
    || { echo "${project}_pch.h lacks the PS5 physical host offset; regenerate with the patched SDK" >&2; exit 1; }

printf '%s' "$flags" > "$work/flags.new"
if ! cmp -s "$work/flags.new" "$work/flags.txt" 2>/dev/null; then
    find "$work/obj" -maxdepth 1 -type f \( -name '*.o' -o -name '*.pch' \) -delete
    mv "$work/flags.new" "$work/flags.txt"
fi

pch="$work/obj/${project}_pch.h.pch"
if [ ! -e "$pch" ] || [ -n "$(find "$work/src/generated" -name '*.h' -newer "$pch" -print -quit)" ]; then
    echo "precompiling ${project}_pch.h"
    ( cd "$runtime_build" && eval "\"$cxx\" $flags -x c++-header -o \"$pch\" -c \"$work/src/generated/${project}_pch.h\"" )
    find "$work/obj" -maxdepth 1 -name 'gen_*.o' -delete
fi

# One compile per line, run in parallel; only what is out of date. Host
# sources are rebuilt when any host header or the generated headers change.
newest_header=$(find "$work/src/repo" "$work/src/generated" -name '*.h' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2-)
: > "$work/todo.txt"
for source in "$work"/src/generated/*.cpp; do
    object="$work/obj/gen_$(basename "$source" .cpp).o"
    [ "$object" -nt "$source" ] || printf '%s\t%s\t%s\n' "$source" "$object" "-include-pch $pch" >> "$work/todo.txt"
done
# Everything the desktop build compiles except source/main.cpp, the windowed
# ReXApp host that main_ps5.cpp replaces.
host_sources=$(cd "$work/src/repo" && find halo3/source -name '*.cpp' | sort; echo source/halo3_cache_debug_hooks.cpp)
for relative in $host_sources; do
    source="$work/src/repo/$relative"
    object="$work/obj/host_$(printf '%s' "${relative%.cpp}" | tr '/' '_').o"
    [ "$object" -nt "$source" ] && [ "$object" -nt "$newest_header" ] || printf '%s\t%s\t%s\n' "$source" "$object" "" >> "$work/todo.txt"
done
count=$(wc -l < "$work/todo.txt")
echo "compiling $count sources with $jobs jobs"
if [ "$count" -gt 0 ]; then
    export cxx flags runtime_build
    compile_one() {
        IFS=$'\t' read -r source object extra <<< "$1"
        ( cd "$runtime_build" && eval "\"$cxx\" $flags $extra -o \"$object\" -c \"$source\"" ) 2> "$object.log" \
            || { echo "FAILED $source"; head -20 "$object.log"; rm -f -- "$object"; return 1; }
        rm -f -- "$object.log"
    }
    export -f compile_one
    nice -n 10 xargs -a "$work/todo.txt" -d '\n' -P "$jobs" -I{} bash -c 'compile_one "$1"' _ {} || { echo "compile failed" >&2; exit 1; }
fi

bash "$here/../payload_ld.sh" write "$sdk" "$work/payload.ld"
stages=("$@"); [ ${#stages[@]} -gt 0 ] || stages=(7)
for stage in "${stages[@]}"; do
    extra="-DPS5_STAGE=$stage"
    [ -z "${PS5_RUN_SECONDS:-}" ] || extra="$extra -DPS5_RUN_SECONDS=$PS5_RUN_SECONDS"
    [ -z "${PS5_LOG_LEVEL:-}" ] || extra="$extra -DPS5_LOG_LEVEL=\\\"$PS5_LOG_LEVEL\\\""
    [ -z "${PS5_PLAY:-}" ] || extra="$extra -DPS5_PLAY"
    # Tuning of the runtime's write tracking (see ApplyConsoleSettings in main_ps5.cpp).
    for setting in PS5_WATCH_GRANULARITY PS5_REQUEST_GRANULARITY_LOG2 PS5_HOT_PAGE_FAULTS \
                   PS5_CLEAR_PAGE_STATE PS5_SUBMIT_ON_BUFFER_END; do
        [ -z "${!setting:-}" ] || extra="$extra -D$setting=${!setting}"
    done
    if [ -n "${TITLE:-}" ]; then
        # As an installable title, linked with the Vulkan driver.
        ( cd "$runtime_build" && eval "\"$cxx\" $flags $extra -DPS5_TITLE -I\"$work/src/ps5\" -o \"$work/obj/title_stage$stage.o\" -c \"$work/src/ps5/main_ps5.cpp\"" )
        bash "$here/../title_build.sh" "$TITLE" "${PS5_TITLE_NAME:-Halo 3 Stage $stage}" \
            "$work/obj/title_stage$stage.o" "$work"/obj/gen_*.o "$work"/obj/host_*.o \
            --start-group $(ls "$libs"/*.a | tr '\n' ' ') "$sdk/target/lib/libc++experimental.a" --end-group
        continue
    fi
    [ "$stage" -le 4 ] || { echo "stage $stage needs graphics, which only a title has: set TITLE" >&2; exit 2; }
    ( cd "$runtime_build" && eval "\"$cxx\" $flags $extra -I\"$work/src/ps5\" -o \"$work/obj/main_stage$stage.o\" -c \"$work/src/ps5/main_ps5.cpp\"" )
    # The runtime's libraries as one group because they reference each other.
    "$cxx" -Wl,-z,nodynamic-undefined-weak -Wl,-T,"$work/payload.ld" \
        -o "$work/halo3-stage$stage.elf" "$work/obj/main_stage$stage.o" "$work"/obj/gen_*.o "$work"/obj/host_*.o \
        -Wl,--start-group $(ls "$libs"/*.a | tr '\n' ' ') "$sdk/target/lib/libc++experimental.a" -Wl,--end-group
    bash "$here/../payload_ld.sh" check "$work/halo3-stage$stage.elf"
    ls -la "$work/halo3-stage$stage.elf"
done
