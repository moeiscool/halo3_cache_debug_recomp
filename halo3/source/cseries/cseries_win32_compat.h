#ifndef __CSERIES_WIN32_COMPAT_H__
#define __CSERIES_WIN32_COMPAT_H__
#pragma once

/* ---------- headers */

#include "cseries/platform.h"

#if !defined(_WIN32)
#include <sched.h>
#include <time.h>
#endif

/* ---------- constants */

/* ---------- definitions */

// On Windows the engine and the hooks in source/main.cpp call these Win32
// functions directly. Other hosts (Linux, PS5) get equivalents with the same
// signatures, layouts and return values, so both builds behave the same.
#if !defined(_WIN32)

struct _SYSTEMTIME
{
	uns16 wYear;
	uns16 wMonth;
	uns16 wDayOfWeek;
	uns16 wDay;
	uns16 wHour;
	uns16 wMinute;
	uns16 wSecond;
	uns16 wMilliseconds;
};
static_assert(sizeof(_SYSTEMTIME) == 0x10);

typedef _SYSTEMTIME SYSTEMTIME;
typedef int64 __time64_t;

/* ---------- prototypes */

namespace win32_compat
{
	inline void fill_system_time(SYSTEMTIME* system_time, struct tm const& calendar, int32 milliseconds)
	{
		system_time->wYear = static_cast<uns16>(calendar.tm_year + 1900);
		system_time->wMonth = static_cast<uns16>(calendar.tm_mon + 1);
		system_time->wDayOfWeek = static_cast<uns16>(calendar.tm_wday);
		system_time->wDay = static_cast<uns16>(calendar.tm_mday);
		system_time->wHour = static_cast<uns16>(calendar.tm_hour);
		system_time->wMinute = static_cast<uns16>(calendar.tm_min);
		system_time->wSecond = static_cast<uns16>(calendar.tm_sec);
		system_time->wMilliseconds = static_cast<uns16>(milliseconds);
	}
}

/* ---------- public code */

inline uns32 GetTickCount(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return static_cast<uns32>(static_cast<uns64>(now.tv_sec) * 1000 + static_cast<uns64>(now.tv_nsec) / 1000000);
}

inline void GetSystemTime(SYSTEMTIME* system_time)
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct tm calendar;
	gmtime_r(&now.tv_sec, &calendar);
	win32_compat::fill_system_time(system_time, calendar, static_cast<int32>(now.tv_nsec / 1000000));
}

inline void GetLocalTime(SYSTEMTIME* system_time)
{
	struct timespec now;
	clock_gettime(CLOCK_REALTIME, &now);
	struct tm calendar;
	localtime_r(&now.tv_sec, &calendar);
	win32_compat::fill_system_time(system_time, calendar, static_cast<int32>(now.tv_nsec / 1000000));
}

inline int32 SwitchToThread(void)
{
	return sched_yield() == 0;
}

inline uns32 SleepEx(uns32 milliseconds, int32 alertable)
{
	(void)(alertable);
	if (milliseconds == 0)
	{
		sched_yield();
		return 0;
	}
	struct timespec duration;
	duration.tv_sec = milliseconds / 1000;
	duration.tv_nsec = static_cast<int64>(milliseconds % 1000) * 1000000;
	while (nanosleep(&duration, &duration) != 0)
	{
	}
	return 0;
}

inline void Sleep(uns32 milliseconds)
{
	SleepEx(milliseconds, 0);
}

inline __time64_t _time64(__time64_t* destination)
{
	__time64_t result = static_cast<__time64_t>(time(nullptr));
	if (destination)
	{
		*destination = result;
	}
	return result;
}

#endif // !defined(_WIN32)

/* ---------- globals */

/* ---------- private code */

#endif // __CSERIES_WIN32_COMPAT_H__
