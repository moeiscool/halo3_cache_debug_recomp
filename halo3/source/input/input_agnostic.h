#ifndef __INPUT_AGNOSTIC_H__
#define __INPUT_AGNOSTIC_H__
#pragma once

/* ---------- headers */

#include "cseries/platform.h"

#include "rex_macros.h"

/* ---------- constants */

/* ---------- definitions */

/* ---------- prototypes */

extern inline void update_button(unsigned char* frames, unsigned short* msec, bool down, uns32 elapsed_msec);
extern inline void update_button(unsigned char* frames, rex::be<unsigned short>* msec, bool down, uns32 elapsed_msec);

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __INPUT_AGNOSTIC_H__
