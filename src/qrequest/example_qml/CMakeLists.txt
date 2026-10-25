# Example QML项目的CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(example_qml)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用MOC处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# 查找Qt5组件
find_package(Qt5 COMPONENTS Core Network Qml Quick REQUIRED)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 添加QML定义
add_definitions(-DENABLED_QML)

# 创建可执行文件
add_executable(example_qml
    main.cpp
    qml.qrc
)

# 链接库
target_link_libraries(example_qml
    qrequest
    Qt5::Core
    Qt5::Network
    Qt5::Qml
    Qt5::Quick
)

# 设置输出目录
set_target_properties(example_qml PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 在Windows上，确保动态库能被找到
if(WIN32)
    # 复制qrequest动态库到输出目录
    add_custom_command(TARGET example_qml POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:qrequest>
        $<TARGET_FILE_DIR:example_qml>
    )
endif()
