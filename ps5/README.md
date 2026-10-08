# halo3_cache_debug on PS5

Build and install the recomp of `halo3_cache_debug.xex` (Halo 3 08172.07.03.08.2240.delta) as a homebrew title on a jailbroken PS5.

**Status: builds are wired up but nothing here has run on a console yet.** The host code compiles for a 64-bit non-Windows target (checked with clang against ReXGlue v0.10.0 plus the PS5 patches), the SDK patches apply cleanly to ReXGlue v0.10.0, and the PS5 host compiles apart from the FreeBSD-only parts. The PS5 toolchain, the link and the console runs have not been done. Bring it up one stage at a time, as described in "First bring-up" below.

Everything in this folder is derived from [mcla-recomp](https://github.com/holdmysocks/mcla-recomp) by holdmysocks, the PS5 port of *Midnight Club: Los Angeles* on the same SDK. It is under **GPL-3.0-or-later** (`ps5/LICENSE`), like that project and like the PS5 Vulkan driver it links. The rest of the repository keeps its BSD-3-Clause licence. A PS5 build links GPL code, so the resulting title is GPL-3.0.

## How the MCLA port works

MCLA and this project are both ReXGlue recomps: the Xbox 360's PowerPC code is translated ahead of time to C++, and ReXGlue (derived from Xenia) supplies the kernel, the Xenos GPU emulation and audio. The PS5 port keeps the recompiled code as it is and changes the runtime underneath:

1. **Toolchain and graphics.** [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) (mihawk-99, GPL-3) is a Mesa RADV port for the PS5 with a fork of the PS5 payload SDK. It is built on Arch Linux. The title is cross-compiled with its `prospero-clang`. The runtime's Vulkan backend then works much as on PC. There is no Vulkan loader: the driver is linked in as an ICD and the instance code gets its functions from `vk_icdGetInstanceProcAddr`. Presentation uses `VK_KHR_display`.
2. **A PS5 platform layer for the runtime** (`patches/rexglue-v0.10.0-ps5.patch`, behind `REX_PLATFORM_PS5`):
   - Guest memory is 4.5 GiB of direct memory, reserved with `sceKernelReserveVirtualRange`. A title cannot size a shared-memory object that large.
   - Pages are 16 KiB, so the 0xE0000000 physical range takes the same 4 KiB host offset as on Apple Silicon. The codegen template carries that rule into the generated header.
   - The patch also covers the fault handler's signal-context offset, thread stacks in direct memory and the clock frequency. That frequency had been reported 81 times too fast.
   - Smaller fixes: FreeBSD libc gaps, no X11/Wayland/RenderDoc, a static GPU plugin instead of a DLL, and a pipeline cache that is actually written.
   - Tuning for the console: coarser write-watch requests, "hot pages" that are uploaded rather than watched, and one submission per frame.
3. **Its own host instead of `ReXApp`** (`game/main_ps5.cpp`). The host drives `rex::Runtime` directly: setup, XEX load, guest heap, then the main thread. It uses an SDL offscreen "window" standing for the display, and pad and audio drivers on the console's own `scePad` and `sceAudioOut`.
4. **A link that suits a title.** One static executable with the driver linked whole. The runtime is built with the large code model, so a custom linker script places the `.ltext`/`.lrodata`/`.ldata`/`.lbss` sections. Without it the code lands in a non-executable segment, which once took a console down. Stand-ins replace libc functions that are null inside a title (`title_support.c`). The ELF is then converted and signed into `eboot.bin` with the driver project's tools.
5. **Staged bring-up with logs over the network.** Every step is announced before it runs, a crash reporter installs itself first, and thread dumps are symbolised on the PC. A title cannot redirect stdout, so the log goes to a TCP connection, or to a file in play builds.

Those notes are in mcla-recomp's `docs/ps5-port-plan.md` and `docs/ps5-feasibility.md`; they are worth reading before a first console run.

## What was needed for Halo 3

The runtime side is reused unchanged: the two SDK patches are mcla-recomp's, byte for byte (renamed). Halo 3's own code needed the work:

- **64-bit `long`.** The engine code (`halo3/source`) was written for Windows, where `long` is 32 bits. On the PS5 (and Linux) it is 64, which breaks every structure shared with the guest. `long`/`unsigned long` are now `int32`/`uns32` (`cseries/platform.h`), the same sizes everywhere. `wchar_t` (2 bytes on Windows, 4 here) is now `char16_t`.
- **Guest data references.** `REX_DATA_REFERENCE_DECLARE` assumed guest memory at host address `0x100000000`. On PS5 it sits wherever the kernel put it. The references now resolve through the runtime's memory base on first use (`rex_macros.h`). Members are reached with `->`.
- **Windows-only code.** MSVC headers, `__pragma`, `_inline`, untyped forward-declared enums, `strncpy_s`/`vsnprintf_s`/`fopen_s`, `_Interlocked*`, and the Win32 time and sleep functions the hooks call are now portable. Windows still uses the originals (`cseries/cseries_win32_compat.h`, `cseries/platform.h`).
- **Hooks shared with the PS5 host.** The hooks and the `cache:`/`xstorage:` devices moved from `source/main.cpp` to `source/halo3_cache_debug_hooks.cpp`, so the PS5 host links them without the desktop app.
- **`main` clash.** The engine's own `main` (hooked over the guest's) is now `xenon_main` on the host, so it does not collide with the PS5 host's `main` (or the SDK's on Linux).
- **Game data.** There is no disc image to extract. You give the folder you run the desktop build from (`halo3_cache_debug.xex` and the data next to it), and it is uploaded to `/data/halo3/game`.

The PS5 build uses the same SDK release as the desktop build (v0.10.0), but its own patched checkout and its own generated code (`/root/halo3/generated`). The repository's `generated/` folder is not touched.

## Building and installing

You need:

- A jailbroken PS5 that runs homebrew titles, with an FTP server payload (port 2121) and a homebrew mounter that lists `/data/homebrew` (mcla-recomp used ShadowMountPlus). That project was tested on firmware 13.42 (PS5 Pro) and 12.70 (Slim).
- A PC with Arch Linux, or Arch under WSL2 on Windows, as root, with about 30 GB free and 16 GB of memory. mcla-recomp's `docs/ps5-build-guide.md` has the WSL2 steps.
- Your own copy of the game build: the folder with `halo3_cache_debug.xex` and its data.

Then:

```bash
pacman -Sy --noconfirm git
git clone <this repository> /root/halo3_cache_debug_recomp
cd /root/halo3_cache_debug_recomp
bash ps5/make_ps5.sh --game-dir /mnt/c/path/to/halo3 --console 192.168.1.50
```

Leave out `--console` to build without uploading. The script resumes at the first step whose result is missing, so run the same command again after a failure. Logs are in `/root/halo3/logs`, and the driver build's log is `/root/ps5vk-arch.log`. The first run takes about an hour (the Vulkan driver alone is about 20 minutes). `--help` lists the options (`--tile`, `--art-dir`, `--title-id`, `--test-build`, `--jobs`).

| On the console | |
|---|---|
| `/data/homebrew/PPSA99783` | The title |
| `/data/halo3/game` | Your game data, unmodified |
| `/data/halo3/halo3_cache_debug.toml` | Optional settings, read at start (same format as on the desktop) |
| `/data/halo3/cache` | Saved shaders and pipelines |
| `/data/halo3/halo3-play.log` | Warnings, errors and a crash report from the last start |

## First bring-up

mcla-recomp's rules for console runs, learned the hard way: one new risky thing per run, and payloads before titles where possible. The first malformed title it ran took the whole console down and needed a re-jailbreak. Expect the same risk here.

1. **Payload stages 1 to 4**, without graphics, through an ELF loader (for example elfldr on port 9021). Each stage stops later: 1 is runtime setup, 2 loads the XEX, 3 creates the main thread, and 4 runs guest code for `PS5_RUN_SECONDS`.
   ```bash
   export PS5_VULKAN=/root/ps5vk/PS5_Vulkan REX_SRC=/root/halo3/rexglue-sdk REX_BUILD=/root/halo3/build-ps5 GEN_DIR=/root/halo3/generated
   bash ps5/game/build.sh 1 2 3 4         # /root/halo3/game-ps5/halo3-stage<N>.elf
   nc -q1 <console> 9021 < /root/halo3/game-ps5/halo3-stage1.elf
   ```
   Stage 4 should end at the game's video setup (`VdInitializeRingBuffer`), because no GPU is attached yet.
2. **Title stages 5 and 6.** Stage 5 creates the Vulkan device, and stage 6 presents to the display, with no guest code. Build one with `TITLE=PPSA99783 bash ps5/game/build.sh 5`, install it and run `python3 ps5/title_log_client.py <console> run.log` while it starts. A test title waits up to 90 s for that connection.
3. **Stage 7 test build** (`make_ps5.sh --test-build`): the game, with the log over the network, thread dumps at 4 s and 8 s, and counters every 5 s. Symbolise a dump with `python3 ps5/symdump.py run.log /root/ps5vk/PS5_Vulkan/build/halo3-PPSA99783/llvm-pie.elf`.
4. **Play build** (the default of `make_ps5.sh`).

Things that are likely to need attention, by what MCLA ran into:

- **"Call to invalid or unregistered function at guest address ..."**: a function only reached through a pointer. Add it to `config/functions.toml`, as on the desktop.
- **Imports that are null in a title.** A crash at address 0 means an import no system module exports. `title_support.c` covers the ones MCLA's runtime needed. Halo 3's code may import more (mcla-recomp's `ps5/probes/symcheck` lists them on the console).
- **Performance.** MCLA held 30 FPS on a PS5 Pro after the tuning in `ApplyConsoleSettings`. Halo 3's cache debug build is a debug build and may be heavier. The runtime counters in the test build's `STATS` lines are the place to start.
- **Networking.** The socket hooks and `raw_host` settings are untested on the console.

## Files

| File | From mcla-recomp | |
|---|---|---|
| `make_ps5.sh` | adapted | One command: driver, SDK, recompiler, codegen, runtime, title, upload |
| `game/build.sh` | adapted | Compiles generated code, `halo3/source`, the hooks and the host; links payload or title |
| `game/main_ps5.cpp` | adapted | The PS5 host (stages, crash reporter, thread dump, console settings) |
| `game/ps5_pad_input.h` | as is (one name changed) | DualSense to Xbox 360 controller through `scePad` |
| `game/ps5_audio.h` | driver as is; audio system new | `sceAudioOut` output, SDL and silent fallbacks |
| `title_build.sh` | adapted | Links and signs the title with the RADV driver |
| `title_support.c`, `title_stub_system_service.c` | as is (renamed symbols) | libc stand-ins for titles; splash-screen import stub |
| `title_log.h`, `log_fd_sink.h` | adapted | Log over TCP (test) or to a file (play) |
| `payload_ld.sh`, `build_ps5_vulkan_driver.sh` | as is | Linker script for payloads; driver and toolchain build |
| `title_log_client.py`, `symdump.py` | as is | Log receiver; thread-dump symboliser |
| `patches/` | as is | ReXGlue v0.10.0 PS5 platform layer and FFmpeg PS5 configuration |

## Credits

- [mcla-recomp](https://github.com/holdmysocks/mcla-recomp) by holdmysocks: the PS5 port this is based on.
- [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) by mihawk-99 (GPL-3): the RADV port and toolchain.
- [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) by Tom Clay and contributors; Xenia by Ben Vanik and contributors.
