#ifndef __THREADS_H__
#define __THREADS_H__
#pragma once

/* ---------- headers */

#include "cseries/platform.h"

/* ---------- constants */

/* ---------- definitions */

/* ---------- prototypes */

extern void initialize_thread_management(void);
extern void destroy_thread_management(void);
extern void sleep_milliseconds(uns32 sleep_time_in_milliseconds);

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __THREADS_H__
