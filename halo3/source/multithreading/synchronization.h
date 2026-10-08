#ifndef __SYNCHRONIZATION_H__
#define __SYNCHRONIZATION_H__
#pragma once

/* ---------- headers */

#include "cseries/platform.h"

/* ---------- constants */

/* ---------- definitions */

/* ---------- prototypes */

extern void initialize_synchronization_objects(void);
extern bool synchronization_objects_initialized(void);
extern void destroy_synchronization_objects(void);

extern void internal_critical_section_enter(int32 critical_section_id);
extern bool internal_critical_section_try_and_enter(int32 critical_section_id);
extern void internal_critical_section_leave(int32 critical_section_id);
extern void internal_mutex_take(int32 mutex_id);
extern bool internal_mutex_take_timeout(int32 mutex_id, uns32 timeout_in_milliseconds);
extern void internal_mutex_release(int32 mutex_id);
extern void internal_event_wait(int32 event_id);
extern bool internal_event_wait_timeout(int32 event_id, uns32 timeout_in_milliseconds);
extern void internal_event_set(int32 event_id);
extern void internal_event_reset(int32 event_id);
extern bool event_has_automatic_reset(int32 event_id);

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __SYNCHRONIZATION_H__
