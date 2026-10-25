
# QGIS configuration function
# Usage: configure_qgis(PROJECT_NAME)
# Example: configure_qgis(tnp)
function(configure_qgis PROJECT_NAME)
    # Set cache variables for this project
    # set(QGIS_DIR "" CACHE PATH "Path to QGIS development installation for ${PROJECT_NAME}")
    
    if(NOT QGIS_DIR)
        message(FATAL_ERROR "QGIS_DIR must be set to configure QGIS for ${PROJECT_NAME}")
    endif()
    
    # find_package(Qt5 COMPONENTS Core Widgets WebKit REQUIRED)
    
    # Add QGIS paths to CMake search paths
    list(APPEND CMAKE_PREFIX_PATH ${QGIS_DIR})
    list(APPEND CMAKE_MODULE_PATH ${QGIS_DIR})
    
    # Try to include QGIS CMake configuration
    if(EXISTS "${QGIS_DIR}/FindQGIS.cmake")
        include(${QGIS_DIR}/FindQGIS.cmake)
        message(STATUS "QGIS configured for ${PROJECT_NAME} using FindQGIS.cmake")
    else()
        # Try modern CMake config approach
        find_package(Qgis QUIET PATHS ${QGIS_DIR})
        if(TARGET Qgis::Core)
            message(STATUS "QGIS configured for ${PROJECT_NAME} using modern CMake targets")
        else()
            message(WARNING "Could not find QGIS configuration. Please check QGIS_DIR path.")
        endif()
    endif()
    
    # Set binary directory for runtime
    if(QGIS_DIR)
        set(QGIS_BIN_DIR ${QGIS_DIR}/bin)
        set(QGIS_BIN_DIR ${QGIS_BIN_DIR} PARENT_SCOPE)
    endif()
    
    # Link QGIS libraries to the target
    if(TARGET Qgis::Core)
        # Modern CMake target approach
        target_link_libraries(${PROJECT_NAME} PRIVATE Qgis::Core)
        if(TARGET Qgis::Gui)
            target_link_libraries(${PROJECT_NAME} PRIVATE Qgis::Gui)
        endif()
        if(TARGET Qgis::Analysis)
            target_link_libraries(${PROJECT_NAME} PRIVATE Qgis::Analysis)
        endif()
        message(STATUS "QGIS libraries linked using modern CMake targets")
    else()
        # Fallback to traditional variables
        if(DEFINED QGIS_CORE_LIBRARY)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${QGIS_CORE_LIBRARY})
        endif()
        if(DEFINED QGIS_GUI_LIBRARY)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${QGIS_GUI_LIBRARY})
        endif()
        if(DEFINED QGIS_ANALYSIS_LIBRARY)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${QGIS_ANALYSIS_LIBRARY})
        endif()
        
        # Add include directories if available
        if(DEFINED QGIS_INCLUDE_DIR)
            target_include_directories(${PROJECT_NAME} PRIVATE ${QGIS_INCLUDE_DIR})
        endif()
        if(DEFINED QGIS_UI_INCLUDE_DIR)
            target_include_directories(${PROJECT_NAME} PRIVATE ${QGIS_UI_INCLUDE_DIR})
        endif()
        message(STATUS "QGIS libraries linked using traditional CMake variables")
    endif()
    
    # Optional: Print configuration info
    message(STATUS "QGIS configuration completed for ${PROJECT_NAME}")
    message(STATUS "  QGIS_DIR: ${QGIS_DIR}")
    if(QGIS_BIN_DIR)
        message(STATUS "  QGIS_BIN_DIR: ${QGIS_BIN_DIR}")
    endif()
endfunction()
