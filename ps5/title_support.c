/* Replacements for libc functions that are null inside an installed title.
 *
 * In a title, an import that no available system module exports is left
 * pointing at nothing, and the first call through it jumps to address 0.
 * ps5/probes/symcheck/title_main.cpp, run on the console (firmware 13.42) over
 * the 433 functions the runtime, the game and the title link import, found
 * six: isatty, link, mkstemp, pathconf, readlink, symlink. The first was hit
 * by the runtime's very first log line (spdlog asks whether standard output is
 * a terminal).
 *
 * ps5/title_build.sh binds the libc names to these with --defsym and keeps
 * the names local, as the driver project's link recipe does for its own
 * substitutes. A payload does not need them: it is given a different kernel
 * library.
 */

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>

int mkstemps(char* template_name, int suffix_length);

int ps5_title_isatty(int descriptor) {
  (void)descriptor;
  errno = ENOTTY;
  return 0;
}

int ps5_title_link(const char* existing, const char* created) {
  (void)existing;
  (void)created;
  errno = ENOSYS;
  return -1;
}

int ps5_title_symlink(const char* target, const char* created) {
  (void)target;
  (void)created;
  errno = ENOSYS;
  return -1;
}

/* Nothing the title touches is a symbolic link as far as it can tell. */
ssize_t ps5_title_readlink(const char* path, char* buffer, size_t size) {
  (void)path;
  (void)buffer;
  (void)size;
  errno = EINVAL;
  return -1;
}

long ps5_title_pathconf(const char* path, int name) {
  (void)path;
  switch (name) {
    case _PC_NAME_MAX:
      return 255;
    case _PC_PATH_MAX:
      return 1024;
    default:
      errno = EINVAL;
      return -1;
  }
}

/* mkstemps is bound to the platform layer's own by the driver's recipe. */
int ps5_title_mkstemp(char* template_name) {
  return mkstemps(template_name, 0);
}

/* Three more that the title link has no definition for at all (the payload
 * link gets them from its SDK's static libc): the linker reports them as
 * undefined once the whole runtime and the game are linked in. */

#include <time.h>

int ps5_title_getresuid(uid_t* real, uid_t* effective, uid_t* saved) {
  const uid_t id = getuid();
  if (real) *real = id;
  if (effective) *effective = id;
  if (saved) *saved = id;
  return 0;
}

int ps5_title_getresgid(gid_t* real, gid_t* effective, gid_t* saved) {
  const gid_t id = getgid();
  if (real) *real = id;
  if (effective) *effective = id;
  if (saved) *saved = id;
  return 0;
}

/* Broken-down UTC time to seconds since 1970, without time zones. The day
 * count is the usual civil-calendar formula with the year starting in March. */
time_t ps5_title_timegm(struct tm* value) {
  long year = value->tm_year + 1900L;
  long month = value->tm_mon + 1L;
  if (month <= 2) {
    year -= 1;
    month += 12;
  }
  const long era = (year >= 0 ? year : year - 399) / 400;
  const long year_of_era = year - era * 400;
  const long day_of_year = (153 * (month - 3) + 2) / 5 + value->tm_mday - 1;
  const long day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  const long days = era * 146097 + day_of_era - 719468;
  return (time_t)days * 86400 + value->tm_hour * 3600L + value->tm_min * 60L + value->tm_sec;
}

/* clock_gettime without the system call.
 *
 * On the console clock_gettime is a system call, and the Vulkan driver reads
 * the monotonic clock on its drawing and waiting paths (os_time_get_nano):
 * with 13,000 draws a frame the GPU command thread spent 19% of its time in
 * it. The monotonic clocks are answered here from the cycle counter, tied to
 * the real clock again every 100 ms so the two stay within microseconds of
 * each other (timed waits take absolute times on the kernel's clock). Every
 * other clock id, and everything if the counter's rate looks wrong, goes to
 * the system. */

#include <stdatomic.h>
#include <stdint.h>

int sceKernelClockGettime(int clock_id, struct timespec* value);

static uint64_t ps5_cycles(void) {
  uint32_t low, high;
  __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
  return ((uint64_t)high << 32) | low;
}

static uint64_t ps5_system_monotonic_ns(void) {
  struct timespec value;
  if (sceKernelClockGettime(CLOCK_MONOTONIC, &value) != 0) {
    return 0;
  }
  return (uint64_t)value.tv_sec * 1000000000u + (uint64_t)value.tv_nsec;
}

enum { kClockUnset, kClockStarting, kClockReady, kClockSystemOnly };
static _Atomic int ps5_clock_state = kClockUnset;
/* Written under ps5_clock_updating with ps5_clock_version odd; readers
 * retry if the version moved. */
static _Atomic uint64_t ps5_clock_version;
static atomic_flag ps5_clock_updating = ATOMIC_FLAG_INIT;
static uint64_t ps5_clock_cycles0, ps5_clock_ns0;  /* the current anchor */
static uint64_t ps5_clock_first_cycles, ps5_clock_first_ns;
static uint64_t ps5_clock_ns_per_cycle;  /* 32.32 fixed point */
static uint64_t ps5_clock_resync_cycles; /* cycles in 100 ms */
static _Atomic uint64_t ps5_clock_last_ns;

static void ps5_clock_start(void) {
  const uint64_t ns0 = ps5_system_monotonic_ns();
  const uint64_t cycles0 = ps5_cycles();
  uint64_t ns1 = ns0, cycles1 = cycles0;
  while (ns0 && ns1 && ns1 - ns0 < 5000000u) {
    ns1 = ps5_system_monotonic_ns();
    cycles1 = ps5_cycles();
  }
  const uint64_t cycles = cycles1 - cycles0;
  /* Between 100 MHz and 10 GHz, or not a counter to rely on. */
  if (!ns0 || !ns1 || cycles < 500000u || cycles > 50000000u) {
    atomic_store(&ps5_clock_state, kClockSystemOnly);
    return;
  }
  ps5_clock_first_cycles = cycles0;
  ps5_clock_first_ns = ns0;
  ps5_clock_cycles0 = cycles1;
  ps5_clock_ns0 = ns1;
  ps5_clock_ns_per_cycle = (uint64_t)((((unsigned __int128)(ns1 - ns0)) << 32) / cycles);
  ps5_clock_resync_cycles = cycles * 20u;
  atomic_store(&ps5_clock_last_ns, ns1);
  atomic_store(&ps5_clock_state, kClockReady);
}

static uint64_t ps5_clock_monotonic_ns(void) {
  uint64_t cycles0, ns0, ns_per_cycle, version;
  do {
    version = atomic_load(&ps5_clock_version);
    cycles0 = ps5_clock_cycles0;
    ns0 = ps5_clock_ns0;
    ns_per_cycle = ps5_clock_ns_per_cycle;
  } while ((version & 1u) || atomic_load(&ps5_clock_version) != version);
  const uint64_t elapsed = ps5_cycles() - cycles0;
  uint64_t ns = ns0 + (uint64_t)(((unsigned __int128)elapsed * ns_per_cycle) >> 32);
  if (elapsed > ps5_clock_resync_cycles) {
    if (!atomic_flag_test_and_set(&ps5_clock_updating)) {
      const uint64_t real = ps5_system_monotonic_ns();
      const uint64_t cycles = ps5_cycles();
      if (real) {
        /* The rate over the whole run so far: ever more exact. Unless the
         * counter or the clock jumped (the console was in rest mode, say):
         * a rate more than 1% from the one in use means the long baseline
         * is no longer one stretch, and it starts again from here. */
        uint64_t rate = ns_per_cycle;
        if (cycles > ps5_clock_first_cycles && real > ps5_clock_first_ns) {
          const uint64_t whole_run = (uint64_t)((((unsigned __int128)(real - ps5_clock_first_ns)) << 32) /
                                                (cycles - ps5_clock_first_cycles));
          const uint64_t difference = whole_run > rate ? whole_run - rate : rate - whole_run;
          if (difference < rate / 100) {
            rate = whole_run;
          } else {
            ps5_clock_first_cycles = cycles;
            ps5_clock_first_ns = real;
          }
        } else {
          ps5_clock_first_cycles = cycles;
          ps5_clock_first_ns = real;
        }
        atomic_fetch_add(&ps5_clock_version, 1);
        ps5_clock_cycles0 = cycles;
        ps5_clock_ns0 = real;
        ps5_clock_ns_per_cycle = rate;
        atomic_fetch_add(&ps5_clock_version, 1);
        ns = real;
      }
      atomic_flag_clear(&ps5_clock_updating);
    } else if (elapsed > ps5_clock_resync_cycles * 10) {
      /* Far past the anchor while another thread is re-tying it: do not
       * extrapolate across what may be a jump. */
      const uint64_t real = ps5_system_monotonic_ns();
      if (real) {
        ns = real;
      }
    }
  }
  /* Never backwards, whichever thread asks and whatever a resync did. */
  uint64_t last = atomic_load(&ps5_clock_last_ns);
  while (ns > last && !atomic_compare_exchange_weak(&ps5_clock_last_ns, &last, ns)) {
  }
  return ns > last ? ns : last;
}

int ps5_title_clock_gettime(clockid_t clock_id, struct timespec* value) {
  if (value && (clock_id == CLOCK_MONOTONIC || clock_id == CLOCK_MONOTONIC_PRECISE ||
                clock_id == CLOCK_MONOTONIC_FAST)) {
    int state = atomic_load(&ps5_clock_state);
    if (state == kClockUnset) {
      int expected = kClockUnset;
      if (atomic_compare_exchange_strong(&ps5_clock_state, &expected, kClockStarting)) {
        ps5_clock_start();
      }
      state = atomic_load(&ps5_clock_state);
    }
    if (state == kClockReady) {
      const uint64_t ns = ps5_clock_monotonic_ns();
      value->tv_sec = (time_t)(ns / 1000000000u);
      value->tv_nsec = (long)(ns % 1000000000u);
      return 0;
    }
  }
  return sceKernelClockGettime(clock_id, value) == 0 ? 0 : -1;
}
