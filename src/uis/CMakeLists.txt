# UIS library for Phase2
# Collects UI headers and sources for IDE visibility and include propagation

file(GLOB UIS_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.h")
file(GLOB UIS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp")
file(GLOB UIS_FORMS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.ui" "${CMAKE_CURRENT_SOURCE_DIR}/*/*.ui")

add_library(uis INTERFACE)

target_sources(uis INTERFACE ${UIS_HEADERS} ${UIS_SOURCES} ${UIS_FORMS})

target_include_directories(uis INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/publicctrls)

if(MSVC)
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UIS\\Header Files" FILES ${UIS_HEADERS})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UIS\\Source Files" FILES ${UIS_SOURCES})
    source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UIS\\Form Files" FILES ${UIS_FORMS})
endif()
