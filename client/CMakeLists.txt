
cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(client)

set(BOOST_MIN_VERSION "1.72.0")

add_executable(${PROJECT_NAME} main.cpp client.hpp client.cpp socks5.hpp spawn.hpp socks5Client.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

find_package(Boost 1.72.0 REQUIRED COMPONENTS context chrono system thread)

target_link_libraries(${PROJECT_NAME} PRIVATE Boost::context Boost::chrono Boost::system Boost::thread)