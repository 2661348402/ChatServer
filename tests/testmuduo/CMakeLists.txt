cmake_minimum_required(VERSION 3.16)

project(server)

#添加头文件
# target_include_directories()
#添加库文件
# target_link_directories()



add_executable(server ./muduo_server.cpp)

target_link_libraries(server muduo_net muduo_base pthread)