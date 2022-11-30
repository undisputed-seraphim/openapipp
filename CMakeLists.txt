cmake_minimum_required(VERSION 3.0)
project(OpenAPIpp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(simdjson 3.0 REQUIRED)

file(GLOB src "${CMAKE_CURRENT_SOURCE_DIR}/src/*")
add_executable(OpenAPIpp ${src})
target_include_directories(OpenAPIpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(OpenAPIpp PUBLIC simdjson::simdjson)
