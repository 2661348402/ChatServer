cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Network)

set(QT_CLIENT_SOURCES
    main.cpp
    network/FramedTcpClient.cpp
    network/ProtocolClient.cpp
    ui/LoginDialog.cpp
    ui/RegisterDialog.cpp
    ui/MainWindow.cpp
    ui/ContactPanel.cpp
    ui/ChatWidget.cpp
    ui/BubbleWidget.cpp
    ui/AddFriendDialog.cpp
    ui/CreateGroupDialog.cpp
    ui/JoinGroupDialog.cpp
)

set(QT_CLIENT_HEADERS
    network/FramedTcpClient.h
    network/ProtocolClient.h
    models/ChatData.h
    ui/LoginDialog.h
    ui/RegisterDialog.h
    ui/MainWindow.h
    ui/ContactPanel.h
    ui/ChatWidget.h
    ui/BubbleWidget.h
    ui/AddFriendDialog.h
    ui/CreateGroupDialog.h
    ui/JoinGroupDialog.h
)

add_executable(ChatClientQt ${QT_CLIENT_SOURCES} ${QT_CLIENT_HEADERS})

target_include_directories(ChatClientQt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ChatClientQt PRIVATE Qt5::Widgets Qt5::Network)

set_target_properties(ChatClientQt PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin"
)
