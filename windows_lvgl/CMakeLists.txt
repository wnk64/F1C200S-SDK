cmake_minimum_required(VERSION 3.20)
project(wiliwili_lvgl_windows LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(WILIWILI_WINDOWS_COLOR_DEPTH "32" CACHE STRING "Windows LVGL color depth: 16 or 32")
if(NOT WILIWILI_WINDOWS_COLOR_DEPTH MATCHES "^(16|32)$")
    message(FATAL_ERROR "WILIWILI_WINDOWS_COLOR_DEPTH must be 16 or 32")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(FREETYPE REQUIRED freetype2)

file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl/src/*.c")

add_library(wiliwili_h264_output STATIC src/h264_output_windows.cpp)
target_include_directories(wiliwili_h264_output PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(wiliwili_h264_output_test src/h264_output_test.cpp)
target_link_libraries(wiliwili_h264_output_test PRIVATE wiliwili_h264_output)
target_link_options(wiliwili_h264_output_test PRIVATE -municode -static-libgcc -static-libstdc++)

add_executable(wiliwili_lvgl
    src/main.cpp
    src/wiliwili_lvgl.rc)

add_library(wiliwili_lvgl_runtime STATIC src/platform_windows.cpp ${LVGL_SOURCES})

target_include_directories(wiliwili_lvgl_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl"
    "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/lvgl/src"
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS})
target_compile_definitions(wiliwili_lvgl_runtime PUBLIC LV_CONF_INCLUDE_SIMPLE LV_COLOR_DEPTH=${WILIWILI_WINDOWS_COLOR_DEPTH})
target_link_directories(wiliwili_lvgl_runtime PUBLIC ${SDL2_LIBRARY_DIRS} ${SDL2_IMAGE_LIBRARY_DIRS} ${FREETYPE_LIBRARY_DIRS})
target_link_libraries(wiliwili_lvgl_runtime PUBLIC wiliwili_h264_output SDL2 ${SDL2_IMAGE_LIBRARIES} ${FREETYPE_LIBRARIES} winhttp shell32 bcrypt)
target_link_libraries(wiliwili_lvgl PRIVATE wiliwili_lvgl_runtime)
target_link_options(wiliwili_lvgl PRIVATE -static-libgcc -static-libstdc++)

include(CTest)
if(BUILD_TESTING)
    add_executable(wiliwili_ui_logic_test src/ui_logic_test.cpp)
    target_link_libraries(wiliwili_ui_logic_test PRIVATE wiliwili_lvgl_runtime)
    target_link_options(wiliwili_ui_logic_test PRIVATE -static-libgcc -static-libstdc++)
    add_test(NAME wiliwili_ui_logic COMMAND wiliwili_ui_logic_test)
endif()

# The Windows proof-of-concept is launched directly from its build directory.
# Bundle the MinGW runtime closure there so it does not silently depend on the
# terminal that happened to start it having C:\\msys64\\mingw64\\bin in PATH.
if(MINGW)
    get_filename_component(MINGW_RUNTIME_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(WILIWILI_LVGL_RUNTIME_DLLS
        SDL2.dll SDL2_image.dll
        libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll
        libfreetype-6.dll libharfbuzz-0.dll libgraphite2.dll
        libbrotlicommon.dll libbrotlidec.dll libbrotlienc.dll libbz2-1.dll
        libpng16-16.dll zlib1.dll libavif-16.dll libjpeg-8.dll libjxl.dll
        libjxl_cms.dll libtiff-6.dll libwebp-7.dll libwebpdemux-2.dll
        libglib-2.0-0.dll libpcre2-8-0.dll libintl-8.dll libiconv-2.dll
        librav1e.dll libsharpyuv-0.dll libdav1d-7.dll libaom.dll
        libSvtAv1Enc-4.dll libyuv.dll libhwy.dll libjbig-0.dll libdeflate.dll
        libLerc.dll liblzma-5.dll libzstd.dll liblcms2-2.dll)
    foreach(runtime_dll IN LISTS WILIWILI_LVGL_RUNTIME_DLLS)
        add_custom_command(TARGET wiliwili_lvgl POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_RUNTIME_DIR}/${runtime_dll}" "$<TARGET_FILE_DIR:wiliwili_lvgl>")
    endforeach()
endif()
