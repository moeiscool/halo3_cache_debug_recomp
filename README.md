# Halo 3 Cache Debug Recomp

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/twist84/halo3_cache_debug_recomp)

Static recompilation of `halo3_cache_debug.xex` from **Halo 3 08172.07.03.08.2240.delta (March 8 2007)**, an Xbox 360 development build, to native code for PC, with a port to jailbroken PS5 consoles in progress.

<a>
    <img width="1920" height="1080" alt="2026.03.24 - 10.27.03.01" src="https://github.com/user-attachments/assets/939169f0-bdaa-4564-aefe-d45dd337fb26" />
</a>

**This repository contains no game code, assets or keys.** You supply your own copy of the build. The recompiled code is generated on your machine from your own `halo3_cache_debug.xex` and is never committed.

See [halo3_cache_release_recomp](https://github.com/twist84/halo3_cache_release_recomp) for a recomp of `halo3_cache_release.xex` from the same build.

## Status

| Target | State |
|---|---|
| Windows | Boots and plays (screenshot above). This is where the project is developed. |
| Linux | Builds with one command (`scripts/build_linux.sh`) against the released SDK. Checked up to the runtime starting and loading the game file; not yet played through with the game. |
| PS5 (jailbroken) | Build scripts and host are in place, adapted from the [mcla-recomp](https://github.com/holdmysocks/mcla-recomp) PS5 port. **Not yet run on a console.** See [`ps5/README.md`](ps5/README.md). |

Help is welcome on all three: see [Contributing](#contributing).

## Which build of the game

Everything here is made for, and only known to work with, this one executable:

| | |
|---|---|
| Build | Halo 3 08172.07.03.08.2240.delta (March 8 2007) |
| File | `halo3_cache_debug.xex`, 33,353,728 bytes |
| CRC32 | `23DECE6C` |
| SHA-1 | `385f114e64b6c1395504f212e479967281f1fb24` |
| SHA-256 | `4f5c65b993212ac34c03ba13d2eda7830d651f9af3f9060045991b8f2f5781b8` |

The function list in `config/` and every hook and data reference in `halo3/source/` and `source/` are addresses in that executable. Another build would need them all found again.

## How it works

The game's PowerPC code is translated to C++ ahead of time by the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (BSD-3, derived from Xenia). The SDK also provides the Xbox 360 kernel, GPU and audio reimplementation. On top of that, this repository adds:

- **`config/functions.toml`**: function boundaries and names for the recompiler. Named functions (`rex_<name>`) can be hooked or called from host code.
- **`halo3/source/`**: engine functions reimplemented in C++, laid out like the original Blam! source. Each one either replaces the recompiled function (`REX_PPC_HOOK`) or calls into it (`REX_PPC_INVOKE`). Guest globals are reached with `REX_DATA_REFERENCE_DECLARE`.
- **`source/`**: the host application (`main.cpp`, a `rex::ReXApp`) and the hooks shared by every platform (`halo3_cache_debug_hooks.cpp`). The shared hooks cover time functions, sockets, online sign-in, `hs_doc`, and patches such as the bloom source and the raw server address.
- **`ps5/`**: the PS5 port: SDK patches, build scripts and a host that runs without `ReXApp`.

## Building and running (Windows)

### What you need

- Windows 10 or 11, x86-64.
- [CMake](https://cmake.org/) 3.25 or newer, [Ninja](https://ninja-build.org/), and Clang (the presets use `clang`/`clang++`).
- Visual Studio Build Tools 2022 with "Desktop development with C++", for the Windows SDK and C++ libraries.
- The **ReXGlue SDK v0.10.0** (the `sdk_version` in `halo3_cache_debug_manifest.toml`). Download `rexglue-sdk-0.10.0-win-amd64.zip` from the [SDK's releases](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.10.0) and unpack it. Alternatively, build it from source and point `REXSDK_DIR` at the source tree.
- Your `halo3_cache_debug.xex` and the build's data files.

### Build

1. Clone this repository and put your executable where the manifest expects it, in an `assets` folder next to the repository:
   ```
   some-folder\
     assets\halo3_cache_debug.xex
     halo3_cache_debug_recomp\        (this repository)
   ```
2. Recompile the game's code into `generated\` with the SDK's `rexglue` (also what `ProjectCodegen.bat` does):
   ```powershell
   rexglue codegen halo3_cache_debug_manifest.toml
   ```
   Run it again whenever `config\functions.toml` or the SDK changes.
3. Configure and build with a preset:
   ```powershell
   cmake --preset win-amd64-release
   cmake --build out\build\win-amd64-release
   ```
   Add `-DCMAKE_PREFIX_PATH=C:\path\to\unpacked\sdk` to the first command, or `-DREXSDK_DIR=C:\path\to\rexglue-sdk` for a source tree. `win-amd64-debug` and `win-amd64-relwithdebinfo` also exist.

### Run

Start `out\build\win-amd64-release\halo3_cache_debug.exe` from your game folder (the one with `halo3_cache_debug.xex` and the maps), or copy it there together with the DLLs the build put next to it (the runtime and the `xenos` GPU plugin among them). Started without a debugger, it uses the current folder for everything: game data, saves, shader cache and its settings file `halo3_cache_debug.toml`. Under a debugger it uses the SDK's default paths, so set them in your launch settings.

Settings you may want, as top-level keys in `halo3_cache_debug.toml` (for example `raw_host = "192.168.1.20"`) or on the command line (`--raw_host=192.168.1.20`):

| Setting | Default | |
|---|---|---|
| `raw_host` | `127.0.0.1` | Address of the server the game's "raw" connection uses |
| `raw_port` | `80` | Its port |
| `raw_services_supported` | `ttl,usr,shr,web,dbg` | Services advertised for it |
| `x_link_status_override` | `-1` | Forces the network link status so the game starts its transport layer |

Controller 0 is always treated as signed in to Xbox Live. Script documentation can be dumped to `hs_doc.txt` with the game's `hs_doc` command.

## Building and running (Linux)

### What you need

- x86-64 Linux with a Vulkan driver for your GPU (Mesa's RADV or ANV, or NVIDIA's).
- CMake 3.25 or newer, Ninja, Clang 18 or newer, curl, unzip. For example:
  - Debian/Ubuntu: `sudo apt install cmake ninja-build clang curl unzip libvulkan1 mesa-vulkan-drivers`
  - Arch: `sudo pacman -S --needed cmake ninja clang curl unzip vulkan-icd-loader` (plus `vulkan-radeon`, `vulkan-intel` or your NVIDIA driver)
  - Fedora: `sudo dnf install cmake ninja-build clang curl unzip vulkan-loader mesa-vulkan-drivers`
- Your `halo3_cache_debug.xex` and the build's data files.

### Build

```bash
git clone <this repository>
cd halo3_cache_debug_recomp
bash scripts/build_linux.sh --xex /path/to/halo3/halo3_cache_debug.xex
```

The script runs these steps, skipping any whose result is already there, so run it again after a failure or a `git pull`:

1. Checks for the tools.
2. Downloads the prebuilt ReXGlue SDK v0.10.0 for Linux into `third_party/` and checks it against a pinned SHA-256. Pass `--sdk-dir DIR` to use your own instead.
3. Links your executable into `../assets/`, where the manifest looks for it, as on Windows.
4. Recompiles the game's code into `generated/`.
5. Builds `out/build/linux-amd64-release/halo3_cache_debug`, with `librexruntime.so` and the GPU plugin `librexgpu-xenos.so` beside it.

Options: `--config debug` or `--config relwithdebinfo` for other builds, `--jobs N`, and `--play [DIR]` to start the game when the build is done. By hand, the same is `rexglue codegen halo3_cache_debug_manifest.toml`, then `cmake --preset linux-amd64-release -DCMAKE_PREFIX_PATH=<sdk>`, then `cmake --build out/build/linux-amd64-release`.

### Run

Start the executable from your game folder; it uses the current folder exactly as on Windows (game data, saves, shader cache, `halo3_cache_debug.toml`), and takes the same settings:

```bash
cd /path/to/halo3 && /path/to/halo3_cache_debug_recomp/out/build/linux-amd64-release/halo3_cache_debug
```

The executable finds its two libraries in its own folder, so it can be started from anywhere. Add `--log_level=debug --log_file=run.log` when reporting a problem.

## Building and installing (PS5)

```bash
bash ps5/make_ps5.sh --game-dir /path/to/your/halo3 --console <console address>
```

Run it on Arch Linux as root (Arch under WSL2 works). It builds the PS5 toolchain and Vulkan driver, a patched ReXGlue v0.10.0, the recompiler and the game. It then uploads the title and your game data over FTP. This has **not yet been run on a console**: read [`ps5/README.md`](ps5/README.md) first. It explains how the port works, the staged bring-up to use on a first run, and what is likely to need fixing.

## Layout

| Path | Contents |
|---|---|
| `halo3_cache_debug_manifest.toml` | ReXGlue project manifest: the executable, the function list, setjmp/longjmp addresses |
| `config/` | Function boundaries and names for the recompiler |
| `halo3/source/` | Engine code reimplemented in C++, in the original source layout |
| `source/` | Desktop host (`main.cpp`) and the hooks shared by all platforms |
| `generated/` | `rexglue.cmake` (committed); the recompiled code is generated here and not committed |
| `scripts/` | `build_linux.sh`: the one-command Linux build |
| `ps5/` | PS5 port: build scripts, host, SDK patches (GPL-3.0-or-later) |

## Contributing

Pull requests are welcome: new hooks and reimplemented functions, fixes, function names, Linux and PS5 work, and documentation. Open an issue first for anything large, so work isn't duplicated.

Some things that keep the code working everywhere:

- **Never commit game files**: no `.xex`, maps or generated code. `generated/` holds only `rexglue.cmake`, and `.gitignore` covers the rest.
- **Build on more than one platform if you can.** Windows and Linux use the same SDK release (v0.10.0), so a change can be checked on both.
- **Use explicit integer widths** in `halo3/source/`: `int32`/`uns32` (from `cseries/platform.h`), not `long`, and `char16_t` for guest wide strings. `long` is 64 bits on Linux and PS5, and structures shared with the guest must keep their Xbox 360 layout.
- **Guest globals** go through `REX_DATA_REFERENCE_DECLARE(address, type, name)`. Use `->` for members and `*name` for the value, and `extern REX_DATA_REFERENCE_EXTERN(type, name);` in headers. Never use a fixed host address.
- **No new Windows-only calls** in shared code. If something needs a Win32 function, add a portable equivalent to `cseries/cseries_win32_compat.h` the way `GetTickCount` and `Sleep` are done.
- **Hooks** for every platform go in `source/halo3_cache_debug_hooks.cpp` or the matching file under `halo3/source/`. Only the desktop app itself belongs in `source/main.cpp`.
- **A function reached only through a pointer** shows up as "Call to invalid or unregistered function at guest address ...". Add an entry for it to `config/functions.toml`.
- Say in the PR what you tested and on which platform. If you can't test something (a PS5, say), note that so someone who can will pick it up.

## Credits

- [twist84](https://github.com/twist84): the original recomp, its engine reimplementation and the hooks.
- ReXGlue SDK by Tom Clay and contributors; Xenia by Ben Vanik and contributors.
- [mcla-recomp](https://github.com/holdmysocks/mcla-recomp) by holdmysocks: the PS5 port this project's `ps5/` is based on.
- [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) by mihawk-99 (GPL-3): the RADV port and toolchain for PS5.

## Licence

BSD-3-Clause; see `LICENSE`. `ps5/` is GPL-3.0-or-later; see `ps5/LICENSE`. *Halo* is a trademark of Microsoft. This project is not affiliated with or endorsed by Microsoft, Bungie or 343 Industries.
