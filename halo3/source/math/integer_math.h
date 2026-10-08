#ifndef __INTEGER_MATH_H__
#define __INTEGER_MATH_H__
#pragma once

/* ---------- headers */

#include "cseries/platform.h"

#include "rex_macros.h"

/* ---------- constants */

enum
{
	k_first_diagonal_direction = 0,
	_direction_northeast = 0,
	_direction_southeast = 1,
	_direction_southwest = 2,
	_direction_northwest = 3,
	k_diagonals_count = 4,
	k_last_diagonal_direction = 3,
	k_first_orthogonal_direction = 4,
	_direction_north = 4,
	_direction_east = 5,
	_direction_south = 6,
	_direction_west = 7,
	k_directions_count = 8,
	k_orthogonal_directions_count = 4,
	k_last_orthogonal_direction = 7,
	_direction_none = 8,
	k_total_directions_count = 9,
};

enum
{
	_adjust_rectangle_center = 0,
	_adjust_rectangle_alert = 1,
	adjust_rectangle_modes_count = 2,
};

/* ---------- definitions */

// do these go here?

typedef unsigned char byte;
typedef unsigned short word;
typedef uns32 dword;
typedef unsigned long long qword;
typedef __int128 oword;

struct tiny_point2d
{
	char x;
	char y;
};
static_assert(sizeof(tiny_point2d) == 0x2);

union short_bounds2d
{
	short n[2];
	struct
	{
		short min;
		short max;
	};
};
static_assert(sizeof(short_bounds2d) == 0x4);

union point2d
{
	short n[2];
	struct
	{
		short x;
		short y;
	};
};
static_assert(sizeof(point2d) == 0x4);

union point2d_be
{
	rex::be<short> n[2];
	struct
	{
		rex::be<short> x;
		rex::be<short> y;
	};
};
static_assert(sizeof(point2d) == 0x4);

union long_point2d
{
	int32 n[2];
	struct
	{
		int32 x;
		int32 y;
	};
};
static_assert(sizeof(long_point2d) == 0x8);

union long_point3d
{
	int32 n[3];
	struct
	{
		int32 x;
		int32 y;
		int32 z;
	};
};
static_assert(sizeof(long_point3d) == 0xC);

union long_point4d
{
	int32 n[4];
	struct
	{
		int32 x;
		int32 y;
		int32 z;
		int32 w;
	};
};
static_assert(sizeof(long_point4d) == 0x10);

union byte_rectangle3d
{
	byte n[6];
	struct
	{
		byte x0;
		byte x1;
		byte y0;
		byte y1;
		byte z0;
		byte z1;
	};
};
static_assert(sizeof(byte_rectangle3d) == 0x6);

union rectangle2d
{
	short n[4];
	short m[2][2];
	struct
	{
		short y0;
		short x0;
		short y1;
		short x1;
	};
};
static_assert(sizeof(rectangle2d) == 0x8);

union long_rectangle2d
{
	int32 n[4];
	int32 m[2][2];
	struct
	{
		int32 y0;
		int32 x0;
		int32 y1;
		int32 x1;
	};
};
static_assert(sizeof(long_rectangle2d) == 0x10);

union long_rectangle3d
{
	int32 n[6];
	struct
	{
		int32 x0;
		int32 y0;
		int32 z0;
		int32 x1;
		int32 y1;
		int32 z1;
	};
};
static_assert(sizeof(long_rectangle3d) == 0x18);

/* ---------- prototypes */

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __INTEGER_MATH_H__
