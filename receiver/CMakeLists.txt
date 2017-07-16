cmake_minimum_required(VERSION 2.8)
project(receiver)
include(Sources.cmake)

include_directories(${CMAKE_SOURCE_DIR})

add_definitions(-D__STDC_CONSTANT_MACROS -Wno-deprecated-declarations)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} video logging)
