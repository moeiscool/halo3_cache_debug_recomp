// halo3_cache_debug on PS5: the recompiled game linked with the ReXGlue
// runtime and the RADV Vulkan driver into one static executable.
//
// Adapted from mcla-recomp's ps5/game/main_ps5.cpp (holdmysocks/mcla-recomp,
// GPL-3.0-or-later); see ps5/README.md for what was taken and what differs.
//
// The desktop host goes through rex::ReXApp, which is built around a window
// and a loadable GPU plugin. Neither exists on the console, so this drives
// rex::Runtime directly in the order ReXApp does: setup, XEX load, guest
// heap, host devices, main thread. The Xenos GPU plugin and the Vulkan driver
// are linked in; presentation goes to the display through VK_KHR_display.
//
// Stops after PS5_STAGE, so a first bring-up on a console can go one stage at
// a time (every stage is announced on the log before it starts):
//   1  construct the runtime and Setup() (memory, kernel, file systems)
//   2  ... and load the XEX image into guest memory
//   3  ... and create the guest heap and the suspended main thread
//   4  ... and resume the main thread, then watch it for PS5_RUN_SECONDS
//      (no graphics: the game stops at its video setup)
//   5  graphics only (title): create the Xenos GPU system on Vulkan, its
//      device and its presenter; nothing else
//   6  presentation (title): stage 5 with a window on SDL's offscreen video
//      driver standing for the display; the message loop runs for
//      PS5_RUN_SECONDS with a repaint every second. No guest code
//   7  the game (title): stage 6's window and graphics system handed to the
//      runtime, then the game runs with the message loop on the main thread

#include "generated/halo3_cache_debug_init.h"

#include "halo3_cache_debug_host.h"

#include <signal.h>
#include <ucontext.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/input/device_assignment.h>
#include <rex/input/input_system.h>
#include <rex/input/nop/nop_input_driver.h>
#include <rex/kernel/crt/heap.h>
#include <rex/kernel/init.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/runtime.h>
#include <rex/system/gpu_plugin.h>
#include <rex/system/kernel_state.h>
#include <rex/system/util/object_table.h>
#include <rex/system/xthread.h>
#include <rex/thread.h>
#include <rex/ui/presenter.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context_sdl.h>

#include <pthread.h>

// FreeBSD's thread id call; its header is not in every SDK.
extern "C" int pthread_getthreadid_np(void);

// Counters added to the runtime by ps5/patches/rexglue-v0.10.0-ps5.patch.
namespace rex::arch {
uint64_t Ps5FaultCount();
}
namespace rex::ui::vulkan {
uint64_t Ps5PresentCount();
}

// As a title (-DPS5_TITLE) the log goes over a TCP connection from the PC, or
// with -DPS5_PLAY to a file on the console; as a payload g_ps5_log_fd is
// standard output, the ELF loader's socket.
#include "title_log.h"
#include "log_fd_sink.h"
#include "ps5_audio.h"
#ifdef PS5_TITLE
#include "ps5_pad_input.h"
#endif

#ifndef PS5_STAGE
#define PS5_STAGE 7
#endif
#ifndef PS5_RUN_SECONDS
#define PS5_RUN_SECONDS 20
#endif

REXCVAR_DECLARE(uint32_t, rexcrt_heap_size_mb);

// The console's shell draws its launch splash over a title until the title
// asks for it to be hidden. A plain import, and only in a title: the title
// converter leaves a weak import unbound.
#ifdef PS5_TITLE
extern "C" int sceSystemServiceHideSplashScreen(void);
#endif

// The Xenos GPU plugin's factory. On the desktop the plugin is a shared
// library found by name at run time; here it is linked in.
extern "C" rex::system::IGraphicsSystem* rex_gpu_create(uint32_t abi_version,
                                                        const rex::system::GpuCreateInfo* info);

// --- Early crash reporter -------------------------------------------------------
//
// Installed from a constructor that runs ahead of the runtime's static
// objects. Prints the signal, fault address, instruction pointer and code
// addresses on the stack straight to the log and exits; it never returns into
// the context. The runtime's own fault handler takes over SIGSEGV/SIGBUS/SIGILL
// later and hands a fault nobody claims back to this one. Symbolise the
// addresses on the PC with ps5/symdump.py.

namespace {

void EarlyWrite(const char* text) {
  (void)!write(g_ps5_log_fd, text, std::strlen(text));
}

void EarlyHex(const char* label, uint64_t value) {
  char buffer[96];
  static const char digits[] = "0123456789abcdef";
  size_t n = 0;
  while (*label) buffer[n++] = *label++;
  buffer[n++] = '0';
  buffer[n++] = 'x';
  for (int shift = 60; shift >= 0; shift -= 4) buffer[n++] = digits[(value >> shift) & 0xF];
  buffer[n++] = '\n';
  (void)!write(g_ps5_log_fd, buffer, n);
}

int EarlyAnchor() { return 0; }

// The machine context is 0x40 bytes into the signal context on firmware 13.42,
// whatever the SDK's ucontext_t says (measured by mcla-recomp).
mcontext_t* MachineContext(void* context) {
  return reinterpret_cast<mcontext_t*>(static_cast<uint8_t*>(context) + 0x40);
}

// Code addresses are taken to be within 512 MiB of this executable's code. A
// title is loaded at 0x400000: do not let the lower bound wrap.
bool IsCodeAddress(uint64_t word) {
  const uint64_t anchor = reinterpret_cast<uint64_t>(&EarlyAnchor);
  const uint64_t low = anchor > 0x20000000 ? anchor - 0x20000000 : 0x1000;
  return word > low && word < anchor + 0x20000000;
}

void EarlyCrash(int signal_number, siginfo_t* info, void* context) {
  EarlyWrite("CRASH\n");
  EarlyHex("  signal ", static_cast<uint64_t>(signal_number));
  EarlyHex("  fault address ", reinterpret_cast<uint64_t>(info->si_addr));
  auto* machine = MachineContext(context);
  EarlyHex("  instruction pointer ", static_cast<uint64_t>(machine->mc_rip));
  EarlyHex("  address of EarlyAnchor ", reinterpret_cast<uint64_t>(&EarlyAnchor));
  EarlyHex("  rsp ", static_cast<uint64_t>(machine->mc_rsp));
  const uint64_t* stack = reinterpret_cast<const uint64_t*>(machine->mc_rsp);
  int printed = 0;
  for (int i = 0; i < 1024 && printed < 16; ++i) {
    if (IsCodeAddress(stack[i])) {
      EarlyHex("  stack code pointer ", stack[i]);
      ++printed;
    }
  }
  _exit(100 + signal_number);
}

void ReportExit() {
  EarlyWrite("EXIT: the process is leaving through exit()\n");
}

__attribute__((constructor(101))) void InstallEarlyCrashReporter() {
  Ps5TitleLogConnect();
  atexit(ReportExit);
  EarlyWrite("early constructor: installing the crash reporter\n");
  struct sigaction action;
  std::memset(&action, 0, sizeof action);
  action.sa_sigaction = EarlyCrash;
  action.sa_flags = SA_SIGINFO;
  sigemptyset(&action.sa_mask);
  // Beyond the faults: anything else catchable that would end the process
  // without a word (a refused system call arrives as SIGSYS).
  for (int signal_number : {SIGSEGV, SIGBUS, SIGILL, SIGABRT, SIGFPE, SIGSYS, SIGTRAP, SIGTERM,
                            SIGHUP, SIGQUIT, SIGPIPE, SIGXFSZ}) {
    sigaction(signal_number, &action, nullptr);
  }
}

// --- Thread dump and sampling profile -------------------------------------------
//
// When the game stalls there is no debugger to ask why. This signals each of
// the runtime's threads in turn; the handler prints where that thread is and
// the code addresses on its stack, and returns without touching the context.

constexpr int kDumpSignal = SIGXCPU;

// Set while sampling: the handler then writes one compact line per sample.
std::atomic<bool> g_profile_sampling{false};

size_t AppendHex(char* buffer, size_t n, uint64_t value) {
  static const char digits[] = "0123456789abcdef";
  int shift = 60;
  while (shift > 0 && ((value >> shift) & 0xF) == 0) shift -= 4;
  for (; shift >= 0; shift -= 4) buffer[n++] = digits[(value >> shift) & 0xF];
  return n;
}

void DumpHandler(int, siginfo_t*, void* context) {
  auto* machine = MachineContext(context);
  const uint64_t* stack = reinterpret_cast<const uint64_t*>(machine->mc_rsp);
  if (g_profile_sampling.load(std::memory_order_relaxed)) {
    // "S <thread> <pc> <code address on the stack> ..." in one write.
    char line[256];
    size_t n = 0;
    line[n++] = 'S';
    line[n++] = ' ';
    n = AppendHex(line, n, static_cast<uint64_t>(pthread_getthreadid_np()));
    line[n++] = ' ';
    n = AppendHex(line, n, static_cast<uint64_t>(machine->mc_rip));
    int found = 0;
    for (int i = 0; i < 512 && found < 8; ++i) {
      if (IsCodeAddress(stack[i])) {
        line[n++] = ' ';
        n = AppendHex(line, n, stack[i]);
        ++found;
      }
    }
    line[n++] = '\n';
    (void)!write(g_ps5_log_fd, line, n);
    return;
  }
  EarlyHex("  tid ", static_cast<uint64_t>(pthread_getthreadid_np()));
  EarlyHex("  pc ", static_cast<uint64_t>(machine->mc_rip));
  int printed = 0;
  for (int i = 0; i < 1024 && printed < 14; ++i) {
    if (IsCodeAddress(stack[i])) {
      EarlyHex("  stack ", stack[i]);
      ++printed;
    }
  }
}

void InstallDumpHandler() {
  struct sigaction action;
  std::memset(&action, 0, sizeof action);
  action.sa_sigaction = DumpHandler;
  action.sa_flags = SA_SIGINFO | SA_RESTART;
  sigemptyset(&action.sa_mask);
  sigaction(kDumpSignal, &action, nullptr);
}

void Line(const char* format, ...) {
  char text[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(text, sizeof text - 1, format, args);
  va_end(args);
  const size_t length = std::strlen(text);
  text[length] = '\n';
  (void)!write(g_ps5_log_fd, text, length + 1);
}

// Printed before an operation, so the last "NEXT" line names what was running
// if the output stops.
#define NEXT(...) Line("NEXT " __VA_ARGS__)

[[maybe_unused]] void DumpRuntimeThreads(rex::system::KernelState* kernel_state) {
  auto threads = kernel_state->object_table()->GetObjectsByType<rex::system::XThread>();
  Line("THREAD DUMP: %d runtime threads, code anchor %p", static_cast<int>(threads.size()),
       reinterpret_cast<void*>(&EarlyAnchor));
  for (auto& thread : threads) {
    if (!thread || !thread->thread()) continue;
    Line(" thread '%s' id %u%s", thread->name().c_str(), static_cast<unsigned>(thread->thread_id()),
         thread->is_guest_thread() ? " (guest)" : "");
    pthread_kill(reinterpret_cast<pthread_t>(thread->thread()->native_handle()), kDumpSignal);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
  }
  Line("THREAD DUMP ends");
}

// Every runtime thread, `hertz` times a second for `seconds`, one line per
// sample (mcla-recomp's ps5/profile_report.py turns them into per-thread
// function counts).
[[maybe_unused]] void ProfileRuntimeThreads(rex::system::KernelState* kernel_state, int seconds,
                                            int hertz) {
  Line("PROFILE begins: %d s at %d Hz, code anchor %p", seconds, hertz,
       reinterpret_cast<void*>(&EarlyAnchor));
  g_profile_sampling.store(true);
  const auto period = std::chrono::microseconds(1000000 / hertz);
  std::vector<rex::system::object_ref<rex::system::XThread>> threads;
  for (int sample = 0; sample < seconds * hertz; ++sample) {
    // Re-read the list now and then: the game creates and ends threads.
    if (sample % hertz == 0) {
      threads = kernel_state->object_table()->GetObjectsByType<rex::system::XThread>();
    }
    for (auto& thread : threads) {
      if (thread && thread->thread()) {
        pthread_kill(reinterpret_cast<pthread_t>(thread->thread()->native_handle()), kDumpSignal);
      }
    }
    std::this_thread::sleep_for(period);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  g_profile_sampling.store(false);
  Line("PROFILE ends");
}

int Finish(const char* what, int code) {
  Line("halo3-ps5 stops: %s", what);
  rex::FlushLogging();
  std::fflush(stdout);
  // No teardown: guest threads may be running, and the runtime's shutdown
  // path has not been tried on the console.
  _exit(code);
}

#define PS5_STRINGIZE_(x) #x
#define PS5_STRINGIZE(x) PS5_STRINGIZE_(x)

// Settings for the console. Halo 3's own (as on the desktop, see
// Halo3CacheDebugApp::OnPreSetup), then the runtime tuning mcla-recomp
// measured on a PS5 Pro, where each changed the frame rate; they are runtime
// settings, not game ones, and a halo3_cache_debug.toml next to the game data
// can override any of them.
void ApplyConsoleSettings() {
  rex::cvar::SetFlagByName("allow_game_relative_writes", "true");
  rex::cvar::SetFlagByName("gpu_allow_invalid_fetch_constants", "true");

  // SDL's offscreen video driver stands for the display; the presenter makes
  // a VK_KHR_display surface for it.
  rex::cvar::SetFlagByName("video_driver", "offscreen");
  // The SDL input driver looks for an optional controller mapping file by a
  // relative path; in a title that lookup throws. There is no such file here.
  rex::cvar::SetFlagByName("hid_mappings_file", "");
  rex::cvar::SetFlagByName("vsync", "false");
  // A protection change costs 26 us on the console whatever its size, and
  // write-watch faults dominated the GPU thread: widen requests to 64 KiB,
  // treat pages that keep faulting as "hot" (uploaded when used, not
  // watched), and keep the GPU buffer's page state across frames.
#ifndef PS5_WATCH_GRANULARITY
#define PS5_WATCH_GRANULARITY 0
#endif
#ifndef PS5_REQUEST_GRANULARITY_LOG2
#define PS5_REQUEST_GRANULARITY_LOG2 16
#endif
#ifndef PS5_HOT_PAGE_FAULTS
#define PS5_HOT_PAGE_FAULTS 4
#endif
#ifndef PS5_CLEAR_PAGE_STATE
#define PS5_CLEAR_PAGE_STATE false
#endif
#ifndef PS5_SUBMIT_ON_BUFFER_END
#define PS5_SUBMIT_ON_BUFFER_END false
#endif
  rex::cvar::SetFlagByName("physical_watch_granularity", PS5_STRINGIZE(PS5_WATCH_GRANULARITY));
  rex::cvar::SetFlagByName("shared_memory_request_granularity_log2",
                           PS5_STRINGIZE(PS5_REQUEST_GRANULARITY_LOG2));
  rex::cvar::SetFlagByName("shared_memory_hot_page_faults", PS5_STRINGIZE(PS5_HOT_PAGE_FAULTS));
  rex::cvar::SetFlagByName("shared_memory_hot_page_ms", "10000");
  rex::cvar::SetFlagByName("shared_memory_invalidation_pages_log2", "4");
  rex::cvar::SetFlagByName("clear_memory_page_state", PS5_STRINGIZE(PS5_CLEAR_PAGE_STATE));
  // Submitting at every end of the guest's primary buffer is a blocking call
  // into the console's driver several times a frame; submit once a frame.
  rex::cvar::SetFlagByName("vulkan_submit_on_primary_buffer_end",
                           PS5_STRINGIZE(PS5_SUBMIT_ON_BUFFER_END));
}

}  // namespace

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  Line("halo3-ps5 starts, pid %d, stage %d", getpid(), PS5_STAGE);
#ifndef PS5_PLAY
  alarm(PS5_RUN_SECONDS + 120);
#endif

  const std::filesystem::path root = PS5_DATA_ROOT;
  const std::filesystem::path game_root = root / "game";
  const std::filesystem::path user_root = root / "user";
  const std::filesystem::path cache_root = root / "cache";

  NEXT("check the game folder %s", game_root.c_str());
  std::error_code ec;
  if (!std::filesystem::is_regular_file(game_root / "halo3_cache_debug.xex", ec)) {
    return Finish("halo3_cache_debug.xex is not in the game folder", 2);
  }
  std::filesystem::create_directories(user_root, ec);
  std::filesystem::create_directories(cache_root, ec);
  std::filesystem::create_directories(game_root / "xstorage", ec);

  NEXT("initialise cvars and logging");
  char program[] = "halo3_cache_debug";
  char* arguments[] = {program, nullptr};
  rex::cvar::Init(1, arguments);
  rex::InitLoggingEarly();
  // The runtime's log goes where this file's own lines go: the loader socket
  // in a payload, the PC's connection or the play log in a title (standard
  // output cannot be redirected in a title, see title_log.h).
  rex::LogConfig log_config;
  log_config.log_to_console = false;
  log_config.extra_sinks.push_back(std::make_shared<Ps5FdSink>(g_ps5_log_fd));
#ifdef PS5_LOG_LEVEL
  log_config.default_level = spdlog::level::from_str(PS5_LOG_LEVEL);
#endif
  log_config.flush_level = spdlog::level::trace;
  rex::InitLogging(log_config);

#ifdef PS5_TITLE
  // The Vulkan driver reports on standard error, which in a title goes
  // nowhere. Point the C library's stderr at the log.
  if (FILE* log_stream = fdopen(g_ps5_log_fd, "w")) {
    setvbuf(log_stream, nullptr, _IOLBF, 0);
    stderr = log_stream;
  }
#endif

  ApplyConsoleSettings();
  {
    // The same settings file the desktop build reads, if there is one.
    const std::filesystem::path settings = root / "halo3_cache_debug.toml";
    if (std::filesystem::is_regular_file(settings, ec)) {
      try {
        rex::cvar::LoadConfig(settings);
        Line("settings: %s read", settings.c_str());
      } catch (const std::exception& e) {
        Line("settings: %s not read: %s", settings.c_str(), e.what());
      }
    }
  }

#if PS5_STAGE >= 5
  NEXT("create the Xenos graphics system (Vulkan backend)");
  rex::system::GpuCreateInfo gpu_create_info;
  gpu_create_info.struct_size = sizeof gpu_create_info;
  gpu_create_info.backend = "vulkan";
  std::unique_ptr<rex::system::IGraphicsSystem> graphics(
      rex_gpu_create(rex::system::kGpuPluginAbiVersion, &gpu_create_info));
  if (!graphics) {
    return Finish("the GPU plugin returned no graphics system", 7);
  }
#if PS5_STAGE == 5
  NEXT("SetupPresentation: Vulkan instance, device and presenter, without a window");
  if (XFAILED(graphics->SetupPresentation(nullptr))) {
    return Finish("graphics setup failed", 8);
  }
  Line("PASS Vulkan device and presenter created");
  return Finish("stage 5 complete", 0);
#else
  NEXT("SDL application context on the offscreen video driver");
  rex::ui::SDLWindowedAppContext app_context;
  if (!app_context.Initialize()) {
    return Finish("the SDL application context did not initialise", 9);
  }
  NEXT("SetupPresentation with the application context");
  if (XFAILED(graphics->SetupPresentation(&app_context)) || !graphics->presenter()) {
    return Finish("graphics setup failed", 8);
  }
  NEXT("create and open the window, attach the presenter (display surface and swapchain)");
  auto window = rex::ui::Window::Create(app_context, "halo3_cache_debug", 1280, 720);
  if (!window || !window->Open()) {
    return Finish("no window", 10);
  }
  window->SetPresenter(graphics->presenter());
#ifdef PS5_TITLE
  NEXT("hide the console's launch splash");
  Line("sceSystemServiceHideSplashScreen returned 0x%08X",
       static_cast<unsigned>(sceSystemServiceHideSplashScreen()));
#endif
#if PS5_STAGE == 6
  NEXT("run the message loop for %d s, requesting a repaint every second", PS5_RUN_SECONDS);
  std::thread repainter([&app_context, &window]() {
    for (int second = 1; second <= PS5_RUN_SECONDS; ++second) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      Line("alive: %d s", second);
      app_context.CallInUIThread([&window]() { window->RequestPaint(); });
    }
    app_context.RequestDeferredQuit();
  });
  app_context.RunMainMessageLoop();
  repainter.join();
  Line("PASS message loop ended normally");
  return Finish("stage 6 complete", 0);
#endif
#endif
#endif

  NEXT("construct rex::Runtime");
  auto runtime = std::make_unique<rex::Runtime>(game_root, user_root, std::filesystem::path(),
                                                cache_root, std::filesystem::path());
#if PS5_STAGE >= 7
  runtime->set_app_context(&app_context);
  runtime->set_display_window(window.get());
#endif

  rex::RuntimeConfig config;
  config.audio_factory = &Ps5AudioSystem::Create;
  config.kernel_init = rex::kernel::InitializeKernel;
#if PS5_STAGE >= 7
  config.graphics = std::move(graphics);
#ifdef PS5_TITLE
  // The console's own pad library (ps5_pad_input.h); SDL has no gamepad
  // backend here. If no controller can be opened, the stand-in driver keeps
  // an idle one present so the game does not wait for a controller forever.
  config.input_factory = [](bool) -> std::unique_ptr<rex::system::IInputSystem> {
    auto input = std::make_unique<rex::input::InputSystem>(nullptr);
    auto pad = std::make_unique<Ps5PadInputDriver>();
    if (pad->Setup() == rex::X_STATUS(0)) {
      input->AddDriver(std::move(pad));
    } else {
      input->AddDriver(std::make_unique<rex::input::nop::NopInputDriver>(nullptr, 0));
    }
    input->SetDeviceAssignment(std::make_unique<rex::input::SlotAssignment>());
    return input;
  };
#else
  config.input_factory = REX_INPUT_BACKEND(rex::input::CreateDefaultInputSystem);
#endif
#endif

  rex::PPCImageInfo image = PPCImageConfig;
  NEXT("Runtime::Setup (guest memory, function table, kernel state, file systems)");
  auto status = runtime->Setup(image, std::move(config));
  if (XFAILED(status)) {
    Line("Runtime::Setup failed: %08X", static_cast<unsigned>(status));
    return Finish("setup failed", 3);
  }
  if (image.register_modules) {
    NEXT("register guest modules");
    image.register_modules(runtime->kernel_state());
  }
#if PS5_STAGE >= 7
  if (runtime->input_system()) {
    static_cast<rex::input::InputSystem*>(runtime->input_system())->AttachWindow(window.get());
  }
#endif
  Line("PASS Runtime::Setup, guest memory at %p",
       static_cast<void*>(runtime->memory()->virtual_membase()));
  if (PS5_STAGE <= 1) return Finish("stage 1 complete", 0);

  NEXT("Runtime::LoadXexImage %s", k_halo3_xex_image_path);
  status = runtime->LoadXexImage(k_halo3_xex_image_path);
  if (XFAILED(status)) {
    Line("LoadXexImage failed: %08X", static_cast<unsigned>(status));
    return Finish("XEX load failed", 4);
  }
  Line("PASS XEX image loaded, title id %08X",
       static_cast<unsigned>(runtime->kernel_state()->title_id()));
  if (PS5_STAGE <= 2) return Finish("stage 2 complete", 0);

  if (image.rexcrt_heap) {
    NEXT("create the guest heap (%u MiB)", static_cast<unsigned>(REXCVAR_GET(rexcrt_heap_size_mb)));
    if (!rex::kernel::crt::InitHeap(REXCVAR_GET(rexcrt_heap_size_mb), runtime->memory())) {
      return Finish("guest heap creation failed", 5);
    }
  }
  NEXT("register the cache: and xstorage: devices");
  halo3_register_host_devices(runtime.get());

  NEXT("Runtime::PrepareModuleLaunch (suspended main guest thread)");
  auto main_thread = runtime->PrepareModuleLaunch();
  if (!main_thread) {
    return Finish("could not create the main guest thread", 6);
  }
  Line("PASS main guest thread created");
  if (PS5_STAGE <= 3) return Finish("stage 3 complete", 0);

#if PS5_STAGE >= 7
  if (runtime->graphics_system()) {
    NEXT("initialise shader storage under %s", cache_root.c_str());
    runtime->graphics_system()->InitializeShaderStorage(cache_root, runtime->kernel_state()->title_id(),
                                                        true);
  }
#endif

  NEXT("resume the main guest thread; guest code runs from here");
  main_thread->Resume();
  InstallDumpHandler();

  // A second thread watches the run; with graphics the message loop needs this one.
  std::thread ticker([&runtime]() {
    [[maybe_unused]] int next_profile_second = 15;
    for (int second = 1; second <= PS5_RUN_SECONDS; ++second) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
#ifdef PS5_PLAY
      // A build for playing: no diagnostics and no end to the run.
      --second;
      continue;
#endif
      if (second <= 30 || second % 30 == 0) Line("alive: %d s", second);
      if (second % 5 == 0) {
        static uint64_t last_protects = 0, last_faults = 0, last_presents = 0;
        const uint64_t protects = rex::memory::Ps5ProtectCallCount();
        const uint64_t faults = rex::arch::Ps5FaultCount();
        const uint64_t presents = PS5_STAGE >= 7 ? rex::ui::vulkan::Ps5PresentCount() : 0;
        Line("STATS %d s: %.1f presents/s, %llu protection changes/s, %llu faults/s", second,
             static_cast<double>(presents - last_presents) / 5.0,
             static_cast<unsigned long long>((protects - last_protects) / 5),
             static_cast<unsigned long long>((faults - last_faults) / 5));
        last_protects = protects;
        last_faults = faults;
        last_presents = presents;
      }
      // Twice, a few seconds apart: a thread at the same place both times is
      // stuck there, one that has moved is running.
      if (second == 4 || second == 8) {
        DumpRuntimeThreads(runtime->kernel_state());
      }
#ifdef PS5_TITLE
      // A profile on request from the controller (L3 + R3 + touchpad), at most
      // one every 40 s: whoever is playing picks the moment.
      if (second >= next_profile_second &&
          g_ps5_profile_request.exchange(false, std::memory_order_relaxed)) {
        Line("PROFILE requested from the controller at %d s", second);
        DumpRuntimeThreads(runtime->kernel_state());
        ProfileRuntimeThreads(runtime->kernel_state(), 20, 25);
        next_profile_second = second + 40;
        g_ps5_profile_request.store(false, std::memory_order_relaxed);
      }
#endif
    }
    Finish("run time reached", 0);
  });
#if PS5_STAGE >= 7
  app_context.RunMainMessageLoop();
  ticker.join();
  return Finish("the message loop ended", 0);
#else
  ticker.join();
  return Finish("run time reached", 0);
#endif
}
