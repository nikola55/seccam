cmake_minimum_required(VERSION 2.8)

project(logging)

set(SOURCES
    logging.cpp
    log.cpp
    writer.cpp
    std_err_writer.cpp
    )

add_library(${PROJECT_NAME} ${SOURCES})

