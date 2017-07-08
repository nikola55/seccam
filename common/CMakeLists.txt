cmake_minimum_required(VERSION 2.8)
project(common)
include(Sources.cmake)
add_library(${PROJECT_NAME} ${SOURCES})
