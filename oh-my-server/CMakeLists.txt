cmake_minimum_required(VERSION 3.5)

project(oh-my-server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the LevelDB library
# find_package(LevelDB REQUIRED)
add_subdirectory(../leveldb ${CMAKE_CURRENT_BINARY_DIR}/leveldb EXCLUDE_FROM_ALL)
message(STATUS "Using gRPC leveldb add_subdirectory.")


# Add the executable
add_executable(server main.cpp)

# Link against LevelDB and other required libraries
target_link_libraries(server PUBLIC leveldb)
