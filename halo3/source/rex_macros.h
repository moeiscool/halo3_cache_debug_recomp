#ifndef __REX_MACROS_H__
#define __REX_MACROS_H__
#pragma once

/* ---------- headers */

#include "generated/halo3_cache_debug_init.h"

#include "cseries/platform.h"

#include <rex/rex_app.h>
#include <rex/ppc/function.h>
#include <rex/runtime.h>
#include <rex/system/thread_state.h>
#include <rex/system/xthread.h>

#include <atomic>
#include <cstdint>
#include <type_traits>

/* ---------- constants */

#define REX_PPC_EXTERN_IMPORT_CLASS(class_name, function) \
	REX_EXTERN(__imp__rex_##class_name##_##function)

#define REX_PPC_INVOKE_CLASS(class_name, function, ...) \
	rex::ppc::GuestToHostFunction<function_return_t<decltype(function)>>(__imp__rex_##class_name##_##function __VA_OPT__(,) __VA_ARGS__)

#define REX_PPC_HOOK_CLASS(class_name, function) \
	REX_HOOK(rex_##class_name##_##function, class_name::function)

#define REX_PPC_EXTERN_IMPORT(function) \
	REX_EXTERN(__imp__rex_##function)

#define REX_PPC_INVOKE(function, ...) \
	rex::ppc::GuestToHostFunction<function_return_t<decltype(function)>>(__imp__rex_##function __VA_OPT__(,) __VA_ARGS__)

#define REX_PPC_INVOKE2(return_type, function, ...) \
	GuestToHostFunction<return_type>(__imp__rex_##function __VA_OPT__(,) __VA_ARGS__)

#define REX_PPC_HOOK(function) \
	REX_HOOK(rex_##function, function)

#define REX_PPC_STUB(function) \
	REX_STUB(rex_##function)

#define REX_PPC_STUB_LOG(function, msg) \
	REX_STUB_LOG(rex_##function, msg)

#define REX_PPC_STUB_RETURN(function, value) \
	REX_STUB_RETURN(rex_##function, value)

// A guest global, reached through wherever the runtime mapped guest memory
// (not always 0x100000000: the PS5 host maps it at an address the kernel
// picks). Use `->` for members, or `*name` / `name.get()` for the value.
#define REX_DATA_REFERENCE_DECLARE(address, type, name) \
	c_guest_data_reference<type> name(address)

#define REX_DATA_REFERENCE_DECLARE_ARRAY(address, type, name, count) \
	c_guest_data_reference<type[count]> name(address)

// for headers: `extern REX_DATA_REFERENCE_EXTERN(type, name);`
#define REX_DATA_REFERENCE_EXTERN(type, name) \
	c_guest_data_reference<type> name

#define REX_PPC_CONTEXT_REF(name) \
	auto current_thread = rex::system::XThread::GetCurrentThread(); \
	assert(current_thread != nullptr); \
	auto context = current_thread->thread_state()->context(); \
	assert(context != nullptr); \
	PPCContext& __restrict name = *context

#define REX_PPC_MEMBASE_PTR(name) \
	auto runtime = rex::Runtime::instance(); \
	auto memory = runtime->memory(); \
	auto name = memory->virtual_membase()

#define REX_PPC_HEAP_ALLOC(type, name, size) \
	assert(memory != nullptr); \
	auto name##_guest = memory->SystemHeapAlloc(size + 1); \
	type* name = memory->TranslateVirtual<type*>(name##_guest)

#define REX_PPC_HEAP_FREE(name) \
	assert(memory != nullptr); \
	memory->SystemHeapFree(name##_guest)

/* ---------- definitions */

// guest memory does not move once mapped, so the base is looked up once
inline uint8_t* rex_guest_virtual_membase()
{
	static std::atomic<uint8_t*> membase{nullptr};
	uint8_t* result = membase.load(std::memory_order_relaxed);
	if (!result)
	{
		result = rex::Runtime::instance()->memory()->virtual_membase();
		membase.store(result, std::memory_order_relaxed);
	}
	return result;
}

template <typename t_type>
class c_guest_data_reference
{
public:
	constexpr explicit c_guest_data_reference(uint32_t guest_address) :
		m_guest_address(guest_address)
	{
	}

	t_type& get() const
	{
		return *reinterpret_cast<t_type*>(rex_guest_virtual_membase() + m_guest_address);
	}

	uint32_t guest_address() const { return m_guest_address; }

	operator t_type&() const { return get(); }
	t_type& operator*() const { return get(); }
	t_type* operator->() const { return &get(); }
	t_type* operator&() const { return &get(); }

	template <typename t_index>
	decltype(auto) operator[](t_index index) const
	{
		return get()[index];
	}

	template <typename t_value>
	c_guest_data_reference const& operator=(t_value&& value) const
	{
		get() = static_cast<t_value&&>(value);
		return *this;
	}

private:
	uint32_t m_guest_address;
};

template <typename t_type>
struct function_t;

template <typename t_return_type, typename... t_args>
struct function_t<t_return_type(t_args...)>
{
	using return_type = t_return_type;
};

template <typename t_return_type, typename... t_args>
struct function_t<t_return_type(*)(t_args...)>
{
	using return_type = t_return_type;
};

template <typename t_return_type, typename t_class, typename... t_args>
struct function_t<t_return_type(t_class::*)(t_args...)>
{
	using return_type = t_return_type;
};

template <typename t_return_type, typename t_class, typename... t_args>
struct function_t<t_return_type(t_class::*)(t_args...) const>
{
	using return_type = t_return_type;
};

template <typename t_function>
using function_return_t =
typename function_t<std::remove_cvref_t<t_function>>::return_type;

/* ---------- prototypes */

/* ---------- globals */

/* ---------- public code */

/* ---------- private code */

#endif // __REX_MACROS_H__
