// halo3_cache_debug - ReXGlue Recompiled Project
//
// Host setup shared by every front end: the desktop app (main.cpp) and the
// PS5 host (ps5/game/main_ps5.cpp), which drives rex::Runtime without ReXApp.

#pragma once

namespace rex {
class Runtime;
}

// The game executable, as the guest sees it.
inline constexpr const char* k_halo3_xex_image_path = "game:\\halo3_cache_debug.xex";

// Registers the `cache:` and `xstorage:` devices the game expects, both under
// the game data root. Call after rex::Runtime::Setup.
void halo3_register_host_devices(rex::Runtime* runtime);
