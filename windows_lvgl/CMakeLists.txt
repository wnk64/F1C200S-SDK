cmake_minimum_required(VERSION 3.20)
project(wiliwili_lvgl_windows LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl/src/*.c")

add_executable(wiliwili_lvgl
    src/main.cpp
    ${LVGL_SOURCES})

target_include_directories(wiliwili_lvgl PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl/src"
    ${SDL2_INCLUDE_DIRS})
target_compile_definitions(wiliwili_lvgl PRIVATE LV_CONF_INCLUDE_SIMPLE)
target_link_directories(wiliwili_lvgl PRIVATE ${SDL2_LIBRARY_DIRS})
target_link_libraries(wiliwili_lvgl PRIVATE SDL2)
target_link_options(wiliwili_lvgl PRIVATE -static-libgcc -static-libstdc++)
