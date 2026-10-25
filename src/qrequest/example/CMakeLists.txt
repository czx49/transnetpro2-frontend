# Example项目的CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(example)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt5组件
find_package(Qt5 COMPONENTS Core Network REQUIRED)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)

# 创建可执行文件
add_executable(example
    main.cpp
)

# 链接库
target_link_libraries(example
    qrequest
    Qt5::Core
    Qt5::Network
)

# 设置输出目录
set_target_properties(example PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 在Windows上，确保动态库能被找到
if(WIN32)
    # 复制qrequest动态库到输出目录
    add_custom_command(TARGET example POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:qrequest>
        $<TARGET_FILE_DIR:example>
    )
endif()
