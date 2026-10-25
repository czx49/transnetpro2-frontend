
# GDAL configuration function
# Usage: configure_gdal(PROJECT_NAME)
# Example: configure_gdal(tnp)
function(configure_gdal PROJECT_NAME)
    # Set cache variables for this project
    # set(GDAL_DIR "" CACHE PATH "Path to GDAL CMake configuration directory for ${PROJECT_NAME}")
    
    # Find GDAL package
    find_package(GDAL 3.12.0 PATHS ${GDAL_DIR} NO_DEFAULT_PATH REQUIRED)

    # Modern CMake: Link to GDAL::GDAL target (preferred method)
    if(TARGET GDAL::GDAL)
        target_link_libraries(${PROJECT_NAME} PRIVATE GDAL::GDAL)
        message(STATUS "GDAL configured for ${PROJECT_NAME} using modern CMake target GDAL::GDAL")
    else()
        # Fallback to traditional variables if target is not available
        if(DEFINED GDAL_INCLUDE_DIRS)
            target_include_directories(${PROJECT_NAME} PRIVATE ${GDAL_INCLUDE_DIRS})
        endif()
        if(DEFINED GDAL_LIBRARY_DIRS)
            target_link_directories(${PROJECT_NAME} PRIVATE ${GDAL_LIBRARY_DIRS})
        endif()
        if(DEFINED GDAL_LIBRARIES)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${GDAL_LIBRARIES})
        endif()
        message(STATUS "GDAL configured for ${PROJECT_NAME} using traditional CMake variables")
    endif()
    
    # Set binary directory for runtime (if GDAL_DIR is available)
    if(GDAL_DIR)
        set(GDAL_BIN_DIR ${GDAL_DIR}/../../../bin)
        set(GDAL_BIN_DIR ${GDAL_BIN_DIR} PARENT_SCOPE)
        message(STATUS "  GDAL_BIN_DIR: ${GDAL_BIN_DIR}")
    endif()
    
    # Optional: Print configuration info
    message(STATUS "GDAL configuration completed for ${PROJECT_NAME}")
    if(GDAL_DIR)
        message(STATUS "  GDAL_DIR: ${GDAL_DIR}")
    endif()
endfunction()
