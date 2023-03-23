cmake_minimum_required(VERSION 3.5)

project(oh-my-db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")


set(OH_MY_SERVER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/oh-my-server")

add_subdirectory(oh-my-server "${OH_MY_SERVER_BINARY_DIR}")
