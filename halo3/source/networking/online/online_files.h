#ifndef __ONLINE_FILES_H__
#define __ONLINE_FILES_H__
#pragma once

/* ---------- headers */

#include "rex_macros.h"

#include "cseries/cseries_macros.h"

/* ---------- constants */

FORWARD_DECLARE_ENUM(e_map_memory_configuration);

/* ---------- definitions */

/* ---------- prototypes */

/* ---------- globals */

extern REX_DATA_REFERENCE_EXTERN(bool, g_online_is_connected_to_live);

/* ---------- public code */

extern void online_files_memory_dispose(void);
extern void online_files_memory_initialize(e_map_memory_configuration memory_configuration);

/* ---------- private code */

#endif // __ONLINE_FILES_H__
