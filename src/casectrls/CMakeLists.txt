# CaseCtrls library for Phase2
# Collects CaseCtrls headers and sources for IDE visibility and include propagation

file(GLOB CASECTRLS_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB CASECTRLS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_library(casectrls INTERFACE)

target_sources(casectrls INTERFACE ${CASECTRLS_HEADERS} ${CASECTRLS_SOURCES})

target_include_directories(casectrls INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../publicctrls)

if(MSVC)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "CaseCtrls\\Header Files" FILES ${CASECTRLS_HEADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "CaseCtrls\\Source Files" FILES ${CASECTRLS_SOURCES})
endif()
