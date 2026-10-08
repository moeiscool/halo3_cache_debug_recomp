#ifndef __SYNCHRONIZED_VALUE_H__
#define __SYNCHRONIZED_VALUE_H__
#pragma once

/* ---------- headers */

#include "cseries/cseries_macros.h"

/* ---------- constants */

/* ---------- definitions */

class c_interlocked_long
{
private:
	int32 volatile m_value; // 0x0
public:
	c_interlocked_long(int32 starting_value);
	c_interlocked_long();
	~c_interlocked_long();
	operator int32() const;
	int32 set(int32 value);
	int32 set_if_equal(int32 value, int32 comperand);
	int32 peek() const;
	int32 add(int32 value);
	int32 increment() volatile;
	int32 decrement() volatile;
	int32 and_(int32 value);
	int32 or_(int32 value);
	int32 xor_(int32 value);
	int32 set_bit(int32 index, bool setting);
	bool test_bit(int32 index) const;
	c_interlocked_long& operator+=(int32 value);
	c_interlocked_long& operator-=(int32 value);
	c_interlocked_long& operator&=(int32 value);
	c_interlocked_long& operator|=(int32 value);
	c_interlocked_long& operator=(c_interlocked_long const& value);
	c_interlocked_long& operator=(bool value);
	c_interlocked_long& operator=(int32 value);
};
static_assert(sizeof(c_interlocked_long) == 0x4);

class c_synchronized_long
{
private:
	int32 volatile m_value; // 0x0
public:
	c_synchronized_long(int32 starting_value);
	c_synchronized_long();
	~c_synchronized_long();
	operator int32() const;
	int32 set(int32 value);
	int32 set_if_equal(int32 value, int32 comperand);
	int32 peek() const;
	int32 add(int32 value);
	int32 increment();
	int32 decrement();
	int32 and_(int32 value);
	int32 or_(int32 value);
	int32 xor_(int32 value);
	int32 set_bit(int32 index, bool setting);
	bool test_bit(int32 index) const;
	c_synchronized_long& operator+=(int32 value);
	c_synchronized_long& operator-=(int32 value);
	c_synchronized_long& operator&=(int32 value);
	c_synchronized_long& operator|=(int32 value);
	c_synchronized_long& operator=(c_synchronized_long const& value);
	c_synchronized_long& operator=(bool value);
	c_synchronized_long& operator=(int32 value);
};
static_assert(sizeof(c_synchronized_long) == 0x4);

/* ---------- prototypes */

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __SYNCHRONIZED_VALUE_H__
