cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS=ON)

set(CMAKE_SYSTEM_NAME "NintendoWii" CACHE STRING "Target system" FORCE)
set(NINTENDO_WII TRUE CACHE BOOL "Target platform is Wii")
set(OGC_CONSOLE wii CACHE STRING "libogc console" FORCE)

set(CMAKE_TOOLCHAIN_FILE $ENV{DEVKITPRO}/cmake/devkitPPC.cmake)
include($ENV{DEVKITPRO}/cmake/ogc-common.cmake)

project(app LANGUAGES C VERSION 1.0.0)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)

ogc_create_dol(${PROJECT_NAME})
