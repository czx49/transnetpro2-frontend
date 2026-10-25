cmake_minimum_required(VERSION 3.16)

function(setup_msvc_envs PROJECT_NAME)
    #############################################################
    # enable warnings
    if(MSVC)
        set(_warnings "")
        if (NOT USING_NMAKE AND NOT USING_NINJA)
            set(_warnings "${_warnings} /W4" )
        endif()

        # disable warnings
        set(_warnings "${_warnings} /wd4100 ")  # unused formal parameters
        set(_warnings "${_warnings} /wd4251 ")  # needs to have dll-interface to be used by clients of class (occurs in Qt template classes)
        set(_warnings "${_warnings} /wd4458 ")  # declaration of 'data' hides class member
        set(_warnings "${_warnings} /wd4702 ")  # unreachable code
        set(_warnings "${_warnings} /wd4706 ")  # assignment within conditional expression (pal)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_warnings}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_warnings}" PARENT_SCOPE)
    endif()

    if(MSVC)
        set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS}\ /source-charset:utf-8\ /execution-charset:gbk\ /std:c++17 PARENT_SCOPE)
        add_compile_options("/std:c++17")
        # disable macros that offend std::numeric_limits<T>::min()/max()        禁用违反 std::numeric_limits<T>::min()/max() 的宏
    endif()


    ######################使用多处理器编译########################
    IF (WIN32)
        if(MSVC)
            OPTION(USE_MP "use multiple" ON)
            OPTION(ProjectConfig_Global_COMPILE_FLAGS_WITH_MP 
                "Set The Global Option COMPILE_FLAGS /MP to target." ON)
            if(ProjectConfig_Global_COMPILE_FLAGS_WITH_MP OR USE_MP)
                set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP" PARENT_SCOPE)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP" PARENT_SCOPE)
            endif()
            set(VS_STARTUP_PROJECT ${PROJECT_NAME} PARENT_SCOPE)
        ENDIF(MSVC)
        add_definitions(-DNOMINMAX)
        add_definitions(-D_USE_MATH_DEFINES)
    ENDIF()


    # 设置调试环境：把第三方 bin 目录加入到调试器的 PATH 中（不复制 DLL）
    # 使用 $ENV{PATH} 在配置阶段捕获当前系统 PATH（Windows）
    set(RUN_ENV_DEBUG "PATH=${QT_BIN_DIR};${GDAL_BIN_DIR};${QGIS_BIN_DIR};${WEBVIEW2_SDK_DIR};$ENV{PATH}")
    if(MSVC)
        # VS_DEBUGGER_ENVIRONMENT 接受类似 "VAR=VALUE;VAR=VALUE" 的字符串
        # 把 RUN_ENV_DEBUG 直接传给调试器，这样启动调试时会使用包含第三方 bin 的 PATH
        set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "${RUN_ENV_DEBUG}")
        set_target_properties(${PROJECT_NAME} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${DEFAULT_BIN_SUBDIR}")
    endif()
endfunction()
