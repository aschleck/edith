cmake_minimum_required(VERSION 2.6)

if (APPLE)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
	SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lc++")
elseif (UNIX)
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x -mmmx -msse2")
endif ()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/Modules/")

project(edith)

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIR})

find_package(Snappy)
include_directories(${SNAPPY_INCLUDE_DIRS})

file(GLOB edith_PROTOS proto/*.proto)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${edith_PROTOS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB edith_SOURCES *.cpp)

add_executable(edith ${edith_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(edith ${PROTOBUF_LIBRARY} ${SNAPPY_LIBRARY})
