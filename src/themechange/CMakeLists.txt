# ThemeChange INTERFACE library
# Provides headers and include directories without building a library
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sourcegroup.cmake)

set(THEMECHANGE_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/themechange.h
)

set(THEMECHANGE_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/themechange.cpp
)

add_library(themechange INTERFACE)

target_sources(themechange INTERFACE ${THEMECHANGE_HEADERS} ${THEMECHANGE_SOURCES})

# Translation generation for ThemeChange is disabled in this project.

target_include_directories(themechange INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    source_group("ThemeChange\\Header Files" FILES ${THEMECHANGE_HEADERS})
    source_group("ThemeChange\\Source Files" FILES ${THEMECHANGE_SOURCES})
    # Translation Files generation removed for ThemeChange
    set_interface_sources(themechange "${THEMECHANGE_HEADERS}" "${THEMECHANGE_SOURCES}" "")
endif()
