#ifndef __PLATFORM_H__
#define __PLATFORM_H__
#pragma once

/* ---------- headers */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

/* ---------- constants */

#define _PRAGMA(...) _Pragma(#__VA_ARGS__)
#define _WARNINGS_PUSH() _PRAGMA(clang diagnostic push)
#define _WARNINGS_POP() _PRAGMA(clang diagnostic pop)
#define WARNINGS_PUSH() _WARNINGS_PUSH() (void)(0)
#define WARNINGS_POP() _WARNINGS_POP() (void)(0)

#define _IGNORE_WARNING(warning) _PRAGMA(clang diagnostic ignored warning)
#define IGNORE_WARNING(warning) _IGNORE_WARNING(warning); (void)(0)

#define _IGNORE_WARNING_PUSH(warning) _WARNINGS_PUSH(); _PRAGMA(clang diagnostic ignored warning)
#define IGNORE_WARNING_PUSH(warning) _IGNORE_WARNING_PUSH(warning); (void)(0)
#define _IGNORE_WARNING_POP() _WARNINGS_POP();
#define IGNORE_WARNING_POP() _IGNORE_WARNING_POP(); (void)(0)

#define _IGNORE_ALL_WARNINGS_PUSH() _IGNORE_WARNING_PUSH("-Weverything")
#define IGNORE_ALL_WARNINGS_PUSH() _IGNORE_ALL_WARNINGS_PUSH(); (void)(0)
#define _IGNORE_ALL_WARNINGS_POP() _WARNINGS_POP()
#define IGNORE_ALL_WARNINGS_POP() _IGNORE_ALL_WARNINGS_POP(); (void)(0)

// the definitions of these enums are not known; an untyped forward declaration is a
// Microsoft extension, so give them the `int` underlying type MSVC assumes
#define FORWARD_DECLARE_ENUM(_name) \
_IGNORE_ALL_WARNINGS_PUSH(); \
enum _name : int; \
_IGNORE_ALL_WARNINGS_POP();


#define ALWAYS_INLINE __attribute__((always_inline))
#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline ALWAYS_INLINE
#else
#define FORCE_INLINE inline ALWAYS_INLINE
#endif

// the engine was written against a 32-bit `long` (Xbox 360 and Win64 are LLP64);
// use explicit widths so structures shared with the guest keep their layout on LP64 hosts (Linux, PS5)
typedef int8_t int8;
typedef uint8_t uns8;
typedef int16_t int16;
typedef uint16_t uns16;
typedef int32_t int32;
typedef uint32_t uns32;
typedef int64_t int64;
typedef uint64_t uns64;
static_assert(sizeof(int32) == 4 && sizeof(uns32) == 4);

// Microsoft "secure" CRT functions the engine code uses, for hosts without them
#if !defined(_WIN32)
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#endif

inline int strncpy_s(char* dst, size_t dst_size, char const* src, size_t count)
{
	if (!dst || dst_size == 0)
		return EINVAL;
	if (!src)
	{
		dst[0] = 0;
		return EINVAL;
	}
	size_t length = 0;
	while (length < count && src[length])
		length++;
	if (length >= dst_size)
	{
		if (count == _TRUNCATE)
		{
			memcpy(dst, src, dst_size - 1);
			dst[dst_size - 1] = 0;
			return 80; // STRUNCATE
		}
		dst[0] = 0;
		return ERANGE;
	}
	memcpy(dst, src, length);
	dst[length] = 0;
	return 0;
}

template <size_t k_size>
inline int strncpy_s(char (&dst)[k_size], char const* src, size_t count)
{
	return strncpy_s(dst, k_size, src, count);
}

inline int vsnprintf_s(char* buffer, size_t size, size_t count, char const* format, va_list arglist)
{
	if (!buffer || size == 0)
		return -1;
	size_t limit = (count == _TRUNCATE || count >= size) ? size : count + 1;
	int result = vsnprintf(buffer, limit, format, arglist);
	return (result < 0 || static_cast<size_t>(result) >= limit) ? -1 : result;
}

inline int fopen_s(FILE** file, char const* path, char const* mode)
{
	if (!file)
		return EINVAL;
	*file = fopen(path, mode);
	return *file ? 0 : errno;
}
#endif

template <typename t_type>
constexpr bool is_enum = __is_enum(t_type);

/* ---------- definitions */

/* ---------- prototypes */

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __PLATFORM_H__
