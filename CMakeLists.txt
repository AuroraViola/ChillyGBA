cmake_minimum_required(VERSION 3.31.11)
project(ChillyGBA C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(third-party/Nuklear third-party/Nuklear/demo/sdl3_renderer)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3-shared)
find_package(SDL3_image REQUIRED CONFIG REQUIRED)
find_package(OpenMP REQUIRED)
add_executable(ChillyGBA 
		src/main.c
        src/nkui.h
)
target_link_libraries(ChillyGBA PRIVATE SDL3::SDL3 SDL3_image::SDL3_image m OpenMP::OpenMP_C)
