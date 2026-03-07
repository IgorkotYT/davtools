cmake_minimum_required(VERSION 3.20)
project(convertdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_executable(convertdav
    src/main.cpp
    src/server.cpp

    src/converters/common.cpp
    src/converters/registry.cpp
    src/converters/png_jpg.cpp
    src/converters/invert.cpp
    src/converters/img_gif.cpp
    src/converters/pdf_png.cpp
    src/converters/mp4_gif.cpp
    src/converters/virustest.cpp
    src/converters/sha256.cpp
    src/converters/base64_encode.cpp
)

target_include_directories(convertdav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Boost.Asio/Beast without libboost_system
target_compile_definitions(convertdav PRIVATE
    BOOST_ERROR_CODE_HEADER_ONLY=1
)

target_link_libraries(convertdav PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

target_compile_options(convertdav PRIVATE
    -Wall -Wextra -Wpedantic
)
