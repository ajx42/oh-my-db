cmake_minimum_required(VERSION 3.5)

project(ohmyserver LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add the LevelDB library
# find_package(LevelDB REQUIRED)
add_subdirectory(../leveldb ${CMAKE_CURRENT_BINARY_DIR}/leveldb EXCLUDE_FROM_ALL)
message(STATUS "Using leveldb via add_subdirectory.")

add_subdirectory(../grpc ${CMAKE_CURRENT_BINARY_DIR}/grpc EXCLUDE_FROM_ALL)
message(STATUS "Using gRPC via add_subdirectory.")

set(_PROTOBUF_LIBPROTOBUF libprotobuf)
set(_REFLECTION grpc++_reflection)

if(CMAKE_CROSSCOMPILING)
    find_program(_PROTOBUF_PROTOC protoc)
else()
    set(_PROTOBUF_PROTOC $<TARGET_FILE:protobuf::protoc>)
endif()
    set(_GRPC_GRPCPP grpc++)
if(CMAKE_CROSSCOMPILING)
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
    set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)
endif()

# Proto file
get_filename_component(db_proto "db.proto" ABSOLUTE)
get_filename_component(db_proto_path "${db_proto}" PATH)

# Generated sources
set(db_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/db.pb.cc")
set(db_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/db.pb.h")
set(db_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/db.grpc.pb.cc")
set(db_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/db.grpc.pb.h")
add_custom_command(
      OUTPUT "${db_proto_srcs}" "${db_proto_hdrs}" "${db_grpc_srcs}" "${db_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${db_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${db_proto}"
      DEPENDS "${db_proto}")

add_library(db_grpc_proto
  ${db_grpc_srcs}
  ${db_grpc_hdrs}
  ${db_proto_srcs}
  ${db_proto_hdrs})

target_link_libraries(db_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Add the executable
add_executable(server server.cpp )
add_executable(replica replica.cpp )

# Proto file
get_filename_component(raft_proto "raft.proto" ABSOLUTE)
get_filename_component(raft_proto_path "${raft_proto}" PATH)

# Generated sources
set(raft_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/raft.pb.cc")
set(raft_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/raft.pb.h")
set(raft_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/raft.grpc.pb.cc")
set(raft_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/raft.grpc.pb.h")
add_custom_command(
      OUTPUT "${raft_proto_srcs}" "${raft_proto_hdrs}" "${raft_grpc_srcs}" "${raft_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${raft_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${raft_proto}"
      DEPENDS "${raft_proto}")

add_library(raft_grpc_proto
  ${raft_grpc_srcs}
  ${raft_grpc_hdrs}
  ${raft_proto_srcs}
  ${raft_proto_hdrs})

target_link_libraries(raft_grpc_proto 
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_library(ohmyraftrpc "${CMAKE_CURRENT_SOURCE_DIR}/RaftServiceImpl.C")

target_link_libraries(ohmyraftrpc
  ${raft_grpc_proto}
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

add_library(dbserverrpc "${CMAKE_CURRENT_SOURCE_DIR}/DatabaseServiceImpl.C")

target_link_libraries(dbserverrpc
  ${raft_grpc_proto}
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Link against LevelDB and other required libraries
target_link_libraries(server leveldb)
target_link_libraries(server ohmyraftrpc)
target_link_libraries(server dbserverrpc)
target_link_libraries(server db_grpc_proto)
target_link_libraries(server raft_grpc_proto)

target_link_libraries(replica leveldb)
target_link_libraries(replica ohmyraftrpc)
target_link_libraries(replica dbserverrpc)
target_link_libraries(replica db_grpc_proto)
target_link_libraries(replica raft_grpc_proto)

add_executable(client client.cpp)
target_link_libraries(client db_grpc_proto)


