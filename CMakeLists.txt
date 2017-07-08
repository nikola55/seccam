cmake_minimum_required(VERSION 2.8)
project(seccam)
include(Sources.cmake)

add_subdirectory(common)
add_subdirectory(video)
add_subdirectory(net)

include_directories(${CMAKE_SOURCE_DIR}/libevent/build/include)

add_definitions(-D__STDC_CONSTANT_MACROS)
add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} 
    video
    net
    common
    crypto
    ssl
    pthread
)
