cmake_minimum_required(VERSION 3.16)
set(APP_NAME "tnp2_app")
project(${APP_NAME} VERSION 0.1 LANGUAGES CXX)

option(THEMES_DEBUG_MODE "Enable themes debug mode" OFF)
option(ENABLE_ZOOM_TO_LAYER "Enable zoom-to-layer context menu in layer trees" OFF)
option(ENABLE_COMMUNITY_GEN_DEBUG_SHP "Export intermediate shapefiles for community generation" OFF)
option(ENABLE_COMMUNITY_GEN_FILL_HOLES "Fill holes of the clip mask in community generation" ON)
option(ENABLE_AUTO_WFS_CAPS_REFRESH
    "Auto force-refresh WFS GetCapabilities after layer list changes" OFF)

# 设置C++标准
if (NOT CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# Qt MOC/UIC/RCC 自动处理
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Debug模式下，库的后缀名带d
set(CMAKE_DEBUG_POSTFIX d)

#############################################################
# MSVC 编译器配置
#############################################################
if(MSVC)
    # 禁用特定警告
    set(_warnings " ")
    if (NOT USING_NMAKE AND NOT USING_NINJA)
        set(_warnings "${_warnings} /W4 ")
    endif()
    set(_warnings "${_warnings} /wd4100 ")  # unused formal parameters
    set(_warnings "${_warnings} /wd4251 ")  # needs to have dll-interface to be used by clients of class
    set(_warnings "${_warnings} /wd4458 ")  # declaration of 'data' hides class member
    set(_warnings "${_warnings} /wd4702 ")  # unreachable code
    set(_warnings "${_warnings} /wd4706 ")  # assignment within conditional expression
    set(_warnings "${_warnings} /wd4819 ")  # code page issues
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_warnings}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_warnings}")

    # 源代码字符集为 UTF-8，执行字符集为 GBK
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /source-charset:utf-8 /execution-charset:gbk /std:c++17")
    add_compile_options("/std:c++17")

    # 禁用宏，避免与 std::numeric_limits 冲突
    add_definitions(-DNOMINMAX)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

###################### 使用多处理器编译 ########################
IF (WIN32)
    if(MSVC)
        OPTION(USE_MP "use multiple" ON)
        OPTION(ProjectConfig_Global_COMPILE_FLAGS_WITH_MP
            "Set The Global Option COMPILE_FLAGS /MP to target." ON)
        if(ProjectConfig_Global_COMPILE_FLAGS_WITH_MP OR USE_MP)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
        endif()
        set(VS_STARTUP_PROJECT ${PROJECT_NAME})
    endif()
ENDIF()
#############################################################

# 设置开发环境路径（根据实际情况调整）
set(GDAL_DIR "" CACHE PATH "Path to GDAL CMake configuration directory for ${PROJECT_NAME}")
set(QGIS_DIR "" CACHE PATH "Path to QGIS development installation for ${PROJECT_NAME}")

# 包含cmake配置文件
include(cmake/msvcenvs.cmake)
include(cmake/variants.cmake)
include(cmake/gdal.cmake)
include(cmake/qgis.cmake)
include(cmake/webview2.cmake)
include(cmake/sourcegroup.cmake)

# 查找Qt
find_package(Qt5 COMPONENTS Core Widgets Network Xml Svg Concurrent Sql REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt5 REQUIRED COMPONENTS Core)

if(NOT DEFINED QT_BIN_DIR)
    # 从 Qt 目标自动获取二进制目录
    get_target_property(QT_CORE_LOCATION Qt5::Core LOCATION)
    get_filename_component(QT_BIN_DIR ${QT_CORE_LOCATION} DIRECTORY)
endif()
set(ENV{PATH} "${QT_BIN_DIR};$ENV{PATH}")

# 项目源文件
set(PROJECT_SOURCES
    main.cpp
    icons.qrc
)

add_executable(${APP_NAME} WIN32 ${PROJECT_SOURCES})

# 配置GDAL
configure_gdal(${APP_NAME})

# 配置QGIS
configure_qgis(${APP_NAME})

# 配置WebView2（仅Windows）
configure_webview2(${APP_NAME})

# 设置MSVC环境
setup_msvc_envs(${APP_NAME})

target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})

# THEMES_DEBUG_MODE
if(THEMES_DEBUG_MODE)
    target_compile_definitions(${APP_NAME} PRIVATE THEMES_DEBUG_MODE)
    target_compile_definitions(${APP_NAME} PRIVATE THEMES_PATH="${CMAKE_SOURCE_DIR}")
endif()

# 图层右键「缩放至图层」
if(ENABLE_ZOOM_TO_LAYER)
    target_compile_definitions(${APP_NAME} PRIVATE TNP_ENABLE_ZOOM_TO_LAYER)
endif()

if(ENABLE_COMMUNITY_GEN_DEBUG_SHP)
    target_compile_definitions(${APP_NAME} PRIVATE TNP_COMMUNITY_GEN_DEBUG_SHP)
endif()

# 生成交通小区：缓冲掩膜填洞开关
if(ENABLE_COMMUNITY_GEN_FILL_HOLES)
    target_compile_definitions(${APP_NAME} PRIVATE TNP_COMMUNITY_GEN_FILL_HOLES)
endif()

if(ENABLE_AUTO_WFS_CAPS_REFRESH)
    target_compile_definitions(${APP_NAME} PRIVATE TNP_ENABLE_AUTO_WFS_CAPS_REFRESH)
endif()

include(GNUInstallDirs)
install(TARGETS ${APP_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 添加子目录（核心模块）
add_subdirectory(publicctrls)
add_subdirectory(qrequest)
add_subdirectory(jsonreader)
add_subdirectory(singleton)
add_subdirectory(themechange)
add_subdirectory(utils)
add_subdirectory(tnmapwidget)
add_subdirectory(httpapis)
add_subdirectory(log4qt)
add_subdirectory(logprinterex)
add_subdirectory(uis)

# 添加子目录（首页模块）
add_subdirectory(homectrls)

# 添加子目录（方案模块）
add_subdirectory(casectrls)

# 启动器（tnp2_launcher + tnp2.env）
add_subdirectory(launcher)

# 源码分目录显示（MSVC）
if(MSVC)
    group_interface_source(qrequest)
    group_interface_source(jsonreader)
    group_interface_source(singleton)
    group_interface_source(themechange)
    group_interface_source(utils)
    group_interface_source(tnmap)
    group_interface_source(httpapis)
    group_interface_source(homectrls)
    group_interface_source(casectrls)
endif()

# 链接库
target_link_libraries(${APP_NAME} PRIVATE publicctrls qrequest jsonreader singleton themechange utils tnmap httpapis logprinterex uis homectrls casectrls)
target_link_libraries(${APP_NAME} PRIVATE Qt5::Core Qt5::Widgets Qt5::Network Qt5::Xml Qt5::Svg Qt5::Concurrent Qt5::Sql)
