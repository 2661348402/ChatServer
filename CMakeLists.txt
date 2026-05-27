cmake_minimum_required(VERSION 3.16)

project(chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")

#设置可执行文件存放位置
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

#添加头文件
include_directories(include)
include_directories(include/server)
include_directories(include/server/db)
include_directories(include/server/model)
include_directories(include/server/redis)
include_directories(include/client)
include_directories(third_party)

#添加子文件夹
add_subdirectory(tests/testmuduo)
add_subdirectory(src)
