cmake_minimum_required(VERSION 2.8)
project(video)
add_definitions(-Wno-deprecated-declarations)
include(Sources.cmake)
include_directories(${CMAKE_SOURCE_DIR})
add_library(${PROJECT_NAME} ${SOURCES})
target_link_libraries(${PROJECT_NAME} 
    avformat 
    avdevice 
    avcodec 
    avutil 
    swscale
    X11
    va-x11
    va
)
