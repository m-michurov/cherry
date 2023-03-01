cmake_minimum_required(VERSION 3.16)
project(simple_example)
set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES example.cpp)

set(SFML_STATIC_LIBRARIES TRUE)

set(SFML_DIR "C:/DKs/SFML-2.5.1/lib/cmake/SFML")
find_package(SFML 2.5 COMPONENTS graphics REQUIRED)

add_executable(simple_example ${SOURCE_FILES})
target_link_libraries(simple_example sfml-graphics)
