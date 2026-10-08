#ifndef __RENDER_WATER_H__
#define __RENDER_WATER_H__
#pragma once

/* ---------- headers */

#include "rex_macros.h"

/* ---------- constants */

/* ---------- definitions */

class c_water_renderer
{
public:
    static void render_shading(void);
};

/* ---------- prototypes */

/* ---------- globals */

extern REX_DATA_REFERENCE_EXTERN(bool, render_water_wireframe_enabled); // 0x182559570

/* ---------- public code */

/* ---------- private code */

#endif // __RENDER_WATER_H__
