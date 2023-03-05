cmake_minimum_required(VERSION 3.16)
project(simple_example)
set(CMAKE_CXX_STANDARD 17)

add_compile_options(-Wall -Wextra -Werror)

set(SOURCE_FILES example.cpp)

set(SFML_STATIC_LIBRARIES TRUE)
set(SFML_DIR $ENV{SFML_DIR})
find_package(SFML 2.5 COMPONENTS graphics REQUIRED)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-Ofast)
endif ()

add_executable(simple_example ${SOURCE_FILES})
target_link_libraries(simple_example sfml-graphics)
