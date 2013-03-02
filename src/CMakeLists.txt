cmake_minimum_required(VERSION 2.6)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/Modules/")

project(dota)

find_package(ProtocolBuffers REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})

find_package(Snappy)
include_directories(${SNAPPY_INCLUDE_DIRS})

#include_directories(${CMAKE_CURRENT_BINARY_DIR})
#PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS foo.proto)

file(GLOB dota_SOURCES *.cpp generated_proto/*.cc)

add_executable(dota ${dota_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(dota ${PROTOBUF_LIBRARY} ${SNAPPY_LIBRARY})
