cmake_minimum_required(VERSION 3.13)

set(CMAKE_TOOLCHAIN_FILE $ENV{DEVKITPRO}/cmake/Wii.cmake)
set(CMAKE_CXX_STANDARD 20)

project(app LANGUAGES CXX VERSION 1.0.0)

add_executable(${PROJECT_NAME}
    src/main.cpp
	#third_party/tracy/public/TracyClient.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE third_party/tracy/public/)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall)
target_compile_definitions(${PROJECT_NAME} PRIVATE __Wii__)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ENABLE)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ONLY_IPV4)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_NO_CALLSTACK)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_NO_CALLSTACK_INLINES)
#target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_ON_DEMAND)
target_compile_definitions(${PROJECT_NAME} PRIVATE TRACY_FIBERS)

ogc_create_dol(${PROJECT_NAME})
