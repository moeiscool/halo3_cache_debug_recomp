# halo3_cache_debug - ReXGlue Recompiled Project
#
# This file is yours to edit. 'rexglue migrate' will NOT overwrite it.
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(halo3_cache_debug LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(HALO3CACHEDEBUG_SOURCES
    source/main.cpp
    source/halo3_cache_debug_hooks.cpp
)

if(WIN32)
    add_executable(halo3_cache_debug WIN32 ${HALO3CACHEDEBUG_SOURCES})
else()
    add_executable(halo3_cache_debug ${HALO3CACHEDEBUG_SOURCES})
endif()

file(GLOB_RECURSE HALO3_GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/halo3/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/halo3/source/*.c"
)
target_sources(halo3_cache_debug PRIVATE
    ${HALO3_GAME_SOURCES}
)
target_include_directories(halo3_cache_debug PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/halo3/source
)

# The Xenos GPU emulation is a plugin loaded at start; stage it next to the executable.
rexglue_setup_target(halo3_cache_debug GPU_PLUGINS xenos)

if(WIN32)
    # timeBeginPeriod in source/main.cpp.
    target_link_libraries(halo3_cache_debug PRIVATE winmm)
endif()

if(UNIX AND NOT APPLE)
    # The executable finds the runtime next to itself ($ORIGIN), but the SDK
    # only stages it there on Windows and macOS.
    add_custom_command(TARGET halo3_cache_debug POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:rex::runtime> $<TARGET_FILE_DIR:halo3_cache_debug>
        VERBATIM)
endif()

