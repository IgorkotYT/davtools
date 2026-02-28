cmake_minimum_required(VERSION 3.20)
project(convertdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(ImageMagick REQUIRED COMPONENTS Magick++)

add_executable(convertdav
    src/main.cpp
    src/server.cpp
    src/converters.cpp
)

target_include_directories(convertdav PRIVATE
    ${ImageMagick_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(convertdav PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
    ${ImageMagick_LIBRARIES}
)

# Arch ImageMagick (Q16HDRI) ABI defines
target_compile_definitions(convertdav PRIVATE
    MAGICKCORE_QUANTUM_DEPTH=16
    MAGICKCORE_HDRI_ENABLE=1
)

target_compile_options(convertdav PRIVATE
    -Wall -Wextra -Wpedantic
)
