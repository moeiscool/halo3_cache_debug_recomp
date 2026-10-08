/* ---------- headers */

#include "rex_macros.h"
#include "cseries/runtime_state.h"

#include "cache/physical_memory_map.h"

/* ---------- constants */

/* ---------- definitions */

/* ---------- prototypes */

/* ---------- globals */

/* ---------- private variables */

/* ---------- ppc */

// exports

REX_PPC_EXTERN_IMPORT(runtime_state_allocate_buffer);

// hooks

/* ---------- public code */

uns32 runtime_state_allocate_buffer(int32 size) // void*
{
#if 1
    return REX_PPC_INVOKE(runtime_state_allocate_buffer, size);
#else
    uns32 result = _physical_memory_malloc(
        _memory_stage_game_initialize,
        "runtime state (not game state)",
        size,
        1,
        __FILE__,
        __LINE__);
    return result;
#endif
}

void runtime_state_free_buffer(uns32 buffer) // void*
{
    physical_memory_free(buffer);
}

/* ---------- private code */
