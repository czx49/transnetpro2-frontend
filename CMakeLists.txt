###########################################################################
cmake_minimum_required(VERSION 3.16.0)        # CMake的最低版本设置
set(CMAKE_COLOR_MAKEFILE ON)                # 启用后，生成的 Makefile 将产生颜色输出

set(ProjectName "TransNetPro2")
project(${ProjectName})
enable_testing()

add_subdirectory(src)
