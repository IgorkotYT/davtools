cmake_minimum_required(VERSION 3.20)
project(convertdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(MAGICKPP REQUIRED IMPORTED_TARGET Magick++)

add_executable(convertdav
    src/main.cpp
    src/server.cpp
    src/converters/registry.cpp
    src/converters/png_jpg.cpp
    src/converters/invert.cpp
    src/converters/img_gif.cpp
    src/converters/pdf_png.cpp
)

target_include_directories(convertdav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(convertdav PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    PkgConfig::MAGICKPP
)

# Keep these for Arch Q16HDRI builds (safe even if pkg-config also provides them)
target_compile_definitions(convertdav PRIVATE
    MAGICKCORE_QUANTUM_DEPTH=16
    MAGICKCORE_HDRI_ENABLE=1
)

target_compile_options(convertdav PRIVATE
    -Wall -Wextra -Wpedantic
)
