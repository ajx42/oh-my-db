cmake_minimum_required(VERSION 3.25.1)
project(ohmy_tools LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

add_executable(readstore readstore.cpp)
add_executable(writestore writestore.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(tester PRIVATE Threads::Threads)


install(TARGETS readstore writestore DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
