cmake_minimum_required(VERSION 3.16)
project(qrequest_tests)

# 设置C++标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找Qt5包
find_package(Qt5 REQUIRED COMPONENTS Core Network Test)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/qrequest)

# 添加基础测试可执行文件
add_executable(basic_test
    basic_test.cpp
)

# 链接库 - 基础测试
target_link_libraries(basic_test
    Qt5::Core
    Qt5::Network
    Qt5::Test
    qrequest
)

# 启用测试
enable_testing()

# 添加测试
add_test(NAME BasicTest COMMAND basic_test)
