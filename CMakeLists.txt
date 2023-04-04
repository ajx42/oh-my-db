cmake_minimum_required(VERSION 3.5)

project(oh-my-db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# removed -Wextra, leveldb breaks down with CLANG otherwise
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(OH_MY_SERVER_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ohmyserver")
set(OH_MY_RAFT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/ohmyraft")

# include generated source files (proto)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/wowUtils")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ohmyserver")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ohmyraft")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/ohmydb")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/leveldb/include")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/argparse/include")

include_directories("${OH_MY_SERVER_BINARY_DIR}")

add_subdirectory(ohmyserver "${OH_MY_SERVER_BINARY_DIR}")
add_subdirectory(ohmyraft "${OH_MY_RAFT_BINARY_DIR}")

