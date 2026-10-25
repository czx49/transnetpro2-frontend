include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sourcegroup.cmake)

set(HTTPAPIS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/backendproxyfactory.h
    ${CMAKE_CURRENT_SOURCE_DIR}/appsession.h
    ${CMAKE_CURRENT_SOURCE_DIR}/grpcaistreamclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/grpcstreamclient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/httpapis.h
    ${CMAKE_CURRENT_SOURCE_DIR}/httpapidefines.h
    ${CMAKE_CURRENT_SOURCE_DIR}/httpserverconfig.h
)

set(HTTPAPIS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/appsession.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpcaistreamclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/grpcstreamclient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/httpapis.cpp
)

add_library(httpapis INTERFACE)

target_sources(httpapis INTERFACE
    ${HTTPAPIS_HEADERS}
    ${HTTPAPIS_SOURCES}
)

target_include_directories(httpapis INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(httpapis INTERFACE
    Qt5::Core
    Qt5::Network
    jsonreader
    qrequest
    singleton
)

if(MSVC)
    source_group("HttpApis\\Header Files" FILES ${HTTPAPIS_HEADERS})
    source_group("HttpApis\\Source Files" FILES ${HTTPAPIS_SOURCES})
    set_interface_sources(httpapis "${HTTPAPIS_HEADERS}" "${HTTPAPIS_SOURCES}" "")
endif()
