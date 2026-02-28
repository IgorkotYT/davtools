cmake_minimum_required(VERSION 3.20)
project(convertdav LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)
find_package(ImageMagick REQUIRED COMPONENTS Magick++)

add_executable(convertdav
    src/main.cpp
)

target_link_libraries(convertdav PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    pthread
)
target_compile_definitions(convertdav PRIVATE
    MAGICKCORE_QUANTUM_DEPTH=16
    MAGICKCORE_HDRI_ENABLE=1
)
# ImageMagick (CMake module style)
target_include_directories(convertdav PRIVATE ${ImageMagick_INCLUDE_DIRS})
target_link_libraries(convertdav PRIVATE ${ImageMagick_LIBRARIES})

# Some installs expose needed defs here (safe if empty)
if (ImageMagick_DEFINITIONS)
    target_compile_definitions(convertdav PRIVATE ${ImageMagick_DEFINITIONS})
endif()

target_compile_options(convertdav PRIVATE
    -Wall -Wextra -Wpedantic
)
