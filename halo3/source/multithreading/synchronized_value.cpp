/* ---------- headers */

#include "multithreading/synchronized_value.h"

/* ---------- constants */

/* ---------- interlocked helpers */

// portable equivalents of the Win32 interlocked intrinsics (same return values),
// operating on 32-bit values on every host
static inline int32 interlocked_exchange(int32 volatile* target, int32 value) { return __atomic_exchange_n(target, value, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_compare_exchange(int32 volatile* target, int32 value, int32 comperand) { __atomic_compare_exchange_n(target, &comperand, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); return comperand; }
static inline int32 interlocked_exchange_add(int32 volatile* target, int32 value) { return __atomic_fetch_add(target, value, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_increment(int32 volatile* target) { return __atomic_add_fetch(target, 1, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_decrement(int32 volatile* target) { return __atomic_sub_fetch(target, 1, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_and(int32 volatile* target, int32 value) { return __atomic_fetch_and(target, value, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_or(int32 volatile* target, int32 value) { return __atomic_fetch_or(target, value, __ATOMIC_SEQ_CST); }
static inline int32 interlocked_xor(int32 volatile* target, int32 value) { return __atomic_fetch_xor(target, value, __ATOMIC_SEQ_CST); }

/* ---------- definitions */

/* ---------- prototypes */

/* ---------- globals */

/* ---------- private variables */

/* ---------- public code */

c_interlocked_long::c_interlocked_long() :
    m_value(0)
{
}

c_interlocked_long::c_interlocked_long(int32 starting_value) :
    m_value(starting_value)
{
}

c_interlocked_long::~c_interlocked_long()
{
}

c_interlocked_long::operator int32() const
{
    return m_value;
}

c_interlocked_long &c_interlocked_long::operator+=(int32 value)
{
    add(value);
    return *this;
}

c_interlocked_long &c_interlocked_long::operator-=(int32 value)
{
    add(-value);
    return *this;
}

c_interlocked_long &c_interlocked_long::operator&=(int32 value)
{
    and_(value);
    return *this;
}

c_interlocked_long &c_interlocked_long::operator|=(int32 value)
{
    or_(value);
    return *this;
}

c_interlocked_long& c_interlocked_long::operator=(c_interlocked_long const& value)
{
    set(value);
    return *this;
}

c_interlocked_long &c_interlocked_long::operator=(int32 value)
{
    set(value);
    return *this;
}

c_interlocked_long &c_interlocked_long::operator=(bool value)
{
    set(value);
    return *this;
}

int32 c_interlocked_long::peek() const
{
    return m_value;
}

c_synchronized_long::c_synchronized_long() :
    m_value(0)
{
}

c_synchronized_long::c_synchronized_long(int32 starting_value) :
    m_value(starting_value)
{
}

c_synchronized_long::~c_synchronized_long()
{
}

c_synchronized_long::operator int32() const
{
    return peek();
}

c_synchronized_long &c_synchronized_long::operator+=(int32 value)
{
    add(value);
    return *this;
}

c_synchronized_long &c_synchronized_long::operator-=(int32 value)
{
    add(-value);
    return *this;
}

c_synchronized_long &c_synchronized_long::operator&=(int32 value)
{
    and_(value);
    return *this;
}

c_synchronized_long &c_synchronized_long::operator|=(int32 value)
{
    or_(value);
    return *this;
}

c_synchronized_long& c_synchronized_long::operator=(c_synchronized_long const& value)
{
    set(value);
    return *this;
}

c_synchronized_long &c_synchronized_long::operator=(int32 value)
{
    set(value);
    return *this;
}

c_synchronized_long &c_synchronized_long::operator=(bool value)
{
    set(value);
    return *this;
}

int32 c_interlocked_long::set(int32 value)
{
    int32 result = interlocked_exchange(&m_value, value);
    return result;
}

int32 c_interlocked_long::set_if_equal(int32 value, int32 comperand)
{
    int32 result = interlocked_compare_exchange(&m_value, value, comperand);
    return result;
}

int32 c_interlocked_long::add(int32 value)
{
    int32 result = interlocked_exchange_add(&m_value, value);
    return result;
}

int32 c_interlocked_long::increment() volatile
{
    int32 result = interlocked_increment(&m_value);
    return result;
}

int32 c_interlocked_long::decrement() volatile
{
    int32 result = interlocked_decrement(&m_value);
    return result;
}

int32 c_interlocked_long::and_(int32 value)
{
    int32 before = interlocked_and(&m_value, value);
    int32 result = before & value;
    return result;
}

int32 c_interlocked_long::or_(int32 value)
{
    int32 before = interlocked_or(&m_value, value);
    int32 result = before | value;
    return result;
}

int32 c_interlocked_long::xor_(int32 value)
{
    int32 before = interlocked_xor(&m_value, value);
    int32 result = before ^ value;
    return result;
}

int32 c_interlocked_long::set_bit(int32 index, bool setting)
{
    int32 result;
    if (setting)
    {
        int32 value = static_cast<int32>(FLAG(index));
        int32 before = interlocked_or(&m_value, value);
        result = before | value;
    }
    else
    {
        int32 value = static_cast<int32>(~FLAG(index));
        int32 before = interlocked_and(&m_value, value);
        result = before & value;
    }
    return result;
}

bool c_interlocked_long::test_bit(int32 index) const
{
    int32 current_value = m_value;
    bool result = TEST_BIT(current_value, index);
    return result;
}

int32 c_synchronized_long::set(int32 value)
{
    int32 result = interlocked_exchange(&m_value, value);
    return result;
}

int32 c_synchronized_long::set_if_equal(int32 value, int32 comperand)
{
    int32 result = interlocked_compare_exchange(&m_value, value, comperand);
    return result;
}

int32 c_synchronized_long::peek() const
{
    int32 result = m_value;
    return result;
}

int32 c_synchronized_long::add(int32 value)
{
    int32 result = interlocked_exchange_add(&m_value, value);
    return result;
}

int32 c_synchronized_long::increment()
{
    int32 result = interlocked_increment(&m_value);
    return result;
}

int32 c_synchronized_long::decrement()
{
    int32 result = interlocked_decrement(&m_value);
    return result;
}

int32 c_synchronized_long::and_(int32 value)
{
    int32 before = interlocked_and(&m_value, value);
    int32 result = before & value;
    return result;
}

int32 c_synchronized_long::or_(int32 value)
{
    int32 before = interlocked_or(&m_value, value);
    int32 result = before | value;
    return result;
}

int32 c_synchronized_long::xor_(int32 value)
{
    int32 before = interlocked_xor(&m_value, value);
    int32 result = before ^ value;
    return result;
}

int32 c_synchronized_long::set_bit(int32 index, bool setting)
{
    int32 result;
    if (setting)
    {
        int32 value = static_cast<int32>(FLAG(index));
        int32 before = interlocked_or(&m_value, value);
        result = before | value;
    }
    else
    {
        int32 value = static_cast<int32>(~FLAG(index));
        int32 before = interlocked_and(&m_value, value);
        result = before & value;
    }
    return result;
}

bool c_synchronized_long::test_bit(int32 index) const
{
    int32 current_value = m_value;
    bool result = TEST_BIT(current_value, index);
    return result;
}

/* ---------- private code */
