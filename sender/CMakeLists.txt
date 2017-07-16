cmake_minimum_required(VERSION 2.8)
project(seccam)
include(Sources.cmake)

include_directories(${CMAKE_SOURCE_DIR}/libevent/build/include)
include_directories(${CMAKE_SOURCE_DIR})

add_definitions(-D__STDC_CONSTANT_MACROS)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} 
    logging
    video
    net
    common
    crypto
    ssl
    pthread
)
