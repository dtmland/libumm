cmake_minimum_required(VERSION 3.24)
project(umm_consumer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(umm 0.1 CONFIG REQUIRED)

add_executable(umm_consumer main.cpp)
target_link_libraries(umm_consumer PRIVATE umm::umm)
