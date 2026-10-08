// halo3_cache_debug - ReXGlue Recompiled Project

#include "generated/halo3_cache_debug_init.h"

#include "halo3_cache_debug_app.h"
#include "halo3_cache_debug_host.h"

// Halo3CacheDebugApp

REX_DEFINE_APP(halo3_cache_debug, Halo3CacheDebugApp::Create)

#include <rex/cvar.h>
#include <rex/input/flags.h>

#if defined(_WIN32)
#include <timeapi.h>
#endif

void Halo3CacheDebugApp::OnPreSetup(rex::RuntimeConfig& config)
{
	// the GPU emulation is a plugin; load it unless --gpu_plugin names another
	if (config.gpu_plugin.empty())
	{
		config.gpu_plugin = "xenos";
	}

	REXCVAR_SET(allow_game_relative_writes, true);
#if defined(_WIN32)
	REXCVAR_SET(input_backend, "xinput");
#endif
	//REXCVAR_SET(fullscreen, true);
	//REXCVAR_SET(vsync, false);
	//REXCVAR_SET(resolution_scale, 2);

#if defined(_WIN32)
	timeBeginPeriod(1);
#endif
}

void Halo3CacheDebugApp::OnLoadXexImage(std::string& xex_image)
{
	xex_image = k_halo3_xex_image_path;
}

void Halo3CacheDebugApp::OnPostSetup()
{
	// defined in the GPU plugin, so it exists only once the plugin has loaded
	rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");

	halo3_register_host_devices(rex::ReXApp::ReXApp::runtime());
}

void Halo3CacheDebugApp::OnConfigurePaths(rex::PathConfig& paths)
{
	if (!rex::debug::IsDebuggerAttached())
	{
		// for user deployments not a developer debugging
		paths.game_data_root = ".";
		paths.user_data_root = ".";
		paths.update_data_root = ".";
		paths.cache_root = ".";
		paths.config_path = std::filesystem::path(".") / "halo3_cache_debug.toml";
	}
}

void Halo3CacheDebugApp::OnShutdown()
{
#if defined(_WIN32)
	timeBeginPeriod(0);
#endif
}

// Halo3CacheDebugApp end
