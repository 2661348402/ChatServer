cmake_minimum_required(VERSION 3.16)

project(chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

include_directories(include)
include_directories(include/server)
include_directories(include/server/db)
include_directories(include/server/model)
include_directories(include/server/redis)
include_directories(include/server/config)
include_directories(include/server/util)
include_directories(include/client)
include_directories(third_party)

add_subdirectory(src)
