/* ---------- headers */

#include "multithreading/threads.h"

#include "rex_macros.h"

#include "cseries/cseries_win32_compat.h"

/* ---------- constants */

/* ---------- definitions */

/* ---------- prototypes */

/* ---------- globals */

/* ---------- private variables */

/* ---------- ppc */

// exports

REX_PPC_EXTERN_IMPORT(initialize_thread_management);
REX_PPC_EXTERN_IMPORT(destroy_thread_management);

// hooks

/* ---------- public code */

void initialize_thread_management(void)
{
    REX_PPC_INVOKE(initialize_thread_management);
}

void destroy_thread_management(void)
{
    REX_PPC_INVOKE(destroy_thread_management);
}

void sleep_milliseconds(uns32 sleep_time_in_milliseconds)
{
    Sleep(sleep_time_in_milliseconds);
}

/* ---------- private code */
