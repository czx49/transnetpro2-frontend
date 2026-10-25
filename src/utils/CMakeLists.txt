# UTILS library
# Utility classes and functions
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sourcegroup.cmake)

file(GLOB UTILS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB UTILS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(utils INTERFACE)

target_sources(utils INTERFACE ${UTILS_HEADERS} ${UTILS_SOURCES})

target_include_directories(utils INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Utils\\Header Files" FILES ${UTILS_HEADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Utils\\Source Files" FILES ${UTILS_SOURCES})
    set_interface_sources(utils "${UTILS_HEADERS}" "${UTILS_SOURCES}" "")
endif()
