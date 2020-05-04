
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(client)

set(CMAKE_CXX_STANDARD 20)

set(BOOST_MIN_VERSION "1.72.0")

find_package(Boost 1.72.0 COMPONENTS context)
include_directories(${Boost_INCLUDE_DIRS})

add_executable(client main.cpp client.hpp client.cpp)

target_link_libraries(client Boost::context)
