cmake_minimum_required(VERSION 3.5)

project(oh-my-db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

#include_directories("${CMAKE_CURRENT_SOURCE_DIR}/oh-my-server")

set(OH_MY_SERVER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ohmyserver")

# include generated source files (proto)
include_directories("${OH_MY_SERVER_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/argparse/include")

add_subdirectory(ohmyserver "${OH_MY_SERVER_BINARY_DIR}")

