# 设置 CMake 最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称，你可以根据实际情况修改
project(qrequest VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt5 相关模块
find_package(Qt5 COMPONENTS Core Network REQUIRED)

# 检查是否包含 Qml 模块，如果包含则添加定义和打印消息
find_package(Qt5 COMPONENTS Qml Quick QUIET)
if(Qt5Qml_FOUND AND Qt5Quick_FOUND)
    add_definitions(-DENABLED_QML)
    message(STATUS "QML module is enabled.")
endif()


# 收集头文件
set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qrequest2.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/qrequest_global.h
)

# 收集源文件
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/qrequest/qrequest2.cpp
)


option(BUILD_SHARED_LIBS "Build qrequest as a shared library project" OFF)

# 根据选项设置宏定义
if(BUILD_SHARED_LIBS)
    add_library(${PROJECT_NAME} SHARED  ${SOURCES} ${HEADERS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE QREQUEST_EXPORTS)
    # 确保MOC生成的符号也被导出
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS ON
    )
else()
    # 定义库类型（这里是静态库）
    add_library(${PROJECT_NAME} STATIC  ${SOURCES} ${HEADERS})
    target_compile_definitions(${PROJECT_NAME} PUBLIC QREQUEST_STATIC_LIBRARY)
endif()

# 设置包含路径
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# 链接 Qt5 相关库
target_link_libraries(${PROJECT_NAME}
    Qt5::Core
    Qt5::Network
)

if(Qt5Qml_FOUND AND Qt5Quick_FOUND)
    target_link_libraries(${PROJECT_NAME}
        Qt5::Qml
        Qt5::Quick
    )
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

install(DIRECTORY
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION .
    FILES_MATCHING PATTERN "*.h"
)
install(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/qrequest.prf
    DESTINATION .
)

# 添加示例项目选项
option(BUILD_EXAMPLES "Build example programs" OFF)
# 添加测试项目选项
option(BUILD_TESTS "Build test programs and test server" OFF)

if(BUILD_EXAMPLES)
    # 添加example子目录
    add_subdirectory(example)

    # 添加example_qml子目录（如果Qt QML模块可用）
    if(Qt5Qml_FOUND AND Qt5Quick_FOUND)
        add_subdirectory(example_qml)
    else()
        message(STATUS "Qt QML/Quick modules not found, skipping QML example")
    endif()
endif()

if(BUILD_TESTS)
    # 添加tests子目录
    add_subdirectory(tests)
    message(STATUS "Tests are enabled")
else()
    message(STATUS "Tests are disabled")
endif()
