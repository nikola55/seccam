cmake_minimum_required(VERSION 2.8)
project(net)
include(Sources.cmake)

include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libevent/build/include
    ${CMAKE_SOURCE_DIR}/jsoncpp/build/include
)

add_library(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} 
    ${CMAKE_SOURCE_DIR}/libevent/build/lib/libevent.so
    ${CMAKE_SOURCE_DIR}/libevent/build/lib/libevent_openssl.so
    ${CMAKE_SOURCE_DIR}/jsoncpp/build/lib64/libjsoncpp.a
)
