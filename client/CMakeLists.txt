
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(client)

set(CMAKE_CXX_STANDARD 20)

set(BOOST_MIN_VERSION "1.72.0")

find_package(Boost 1.72.0 REQUIRED COMPONENTS context chrono log log_setup system thread)

add_executable(${PROJECT_NAME} main.cpp client.hpp client.cpp socks5.hpp)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::context Boost::chrono Boost::log Boost::system Boost::thread)

get_target_property(_boost_log_lib Boost::log IMPORTED_LOCATION)

if (_boost_log_lib MATCHES "${CMAKE_SHARED_LIBRARY_SUFFIX}$")
    target_link_libraries(${PROJECT_NAME} PRIVATE Boost::dynamic_linking)
endif ()