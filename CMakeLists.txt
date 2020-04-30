cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(socks5_libuv)

set(CMAKE_CXX_STANDARD 20)

add_executable(socks5_libuv main.cpp server.hpp server.cpp session.hpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(uv REQUIRED IMPORTED_TARGET libuv)
target_link_libraries(socks5_libuv PRIVATE PkgConfig::uv)