# UIS INTERFACE library
# Collects UI headers, sources and ui forms for IDE visibility and include propagation
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sourcegroup.cmake)

file(GLOB TNMAP_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h")
file(GLOB TNMAP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp")
file(GLOB TNMAP_UI FORMS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.ui")

add_library(tnmap INTERFACE)

target_sources(tnmap INTERFACE ${TNMAP_HEADERS} ${TNMAP_SOURCES} ${TNMAP_UI})

target_include_directories(tnmap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files\\tnmap" FILES ${TNMAP_HEADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files\\tnmap" FILES ${TNMAP_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Form Files\\tnmap" FILES ${TNMAP_UI})
    set_interface_sources(tnmap "${TNMAP_HEADERS}" "${TNMAP_SOURCES}" "${TNMAP_UI}")
endif()
