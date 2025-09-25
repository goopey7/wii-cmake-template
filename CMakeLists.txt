cmake_minimum_required(VERSION 3.13)

set(CMAKE_TOOLCHAIN_FILE $ENV{DEVKITPRO}/cmake/Wii.cmake)

project(app LANGUAGES C VERSION 1.0.0)

add_executable(${PROJECT_NAME}
    src/main.c
)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)

ogc_create_dol(${PROJECT_NAME})
