#ifndef __RUNTIME_STATE_H__
#define __RUNTIME_STATE_H__
#pragma once

/* ---------- headers */

#include "rex_macros.h"

#include "cseries/platform.h"

/* ---------- constants */

/* ---------- definitions */

struct s_runtime_state_globals;

/* ---------- prototypes */

extern void runtime_state_shell_initialize();
extern void runtime_state_shell_dispose();
extern void runtime_state_initialize();
extern void runtime_state_dispose();
extern void runtime_state_initialize_for_new_map();
extern void runtime_state_dispose_from_old_map();
extern uns32 runtime_state_get_buffer_address(int32* buffer_size); //

// runtime state windows

extern uns32 runtime_state_allocate_buffer(int32 size); // void*
extern void runtime_state_free_buffer(uns32 buffer); // void*

/* ---------- globals */

//extern s_runtime_state_globals g_runtime_state_globals;
extern REX_DATA_REFERENCE_EXTERN(s_runtime_state_globals, g_runtime_state_globals);

/* ---------- public code */

/* ---------- private code */

#endif // __RUNTIME_STATE_H__
