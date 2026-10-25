# HomeCtrls library for Phase2
# Collects HomeCtrls headers and sources for IDE visibility and include propagation

file(GLOB HOMECTRLS_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB HOMECTRLS_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(homectrls INTERFACE)

target_sources(homectrls INTERFACE ${HOMECTRLS_HEADERS} ${HOMECTRLS_SOURCES})

target_include_directories(homectrls INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../publicctrls)

if(MSVC)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "HomeCtrls\\Header Files" FILES ${HOMECTRLS_HEADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "HomeCtrls\\Source Files" FILES ${HOMECTRLS_SOURCES})
endif()
