cmake_minimum_required(VERSION 3.16)

project(jsonreader LANGUAGES CXX)

set(JSONREADER_HEADERS
    qjsonreader.h
    httpapijson.h
)

set(JSONREADER_SOURCES
    qjsonreader.cpp
)

add_library(jsonreader STATIC ${JSONREADER_SOURCES} ${JSONREADER_HEADERS})

target_include_directories(jsonreader
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Expose sources for IDE visibility; INTERFACE libraries don't compile sources
target_sources(jsonreader INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/qjsonreader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/qjsonreader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/httpapijson.h
    ${CMAKE_CURRENT_SOURCE_DIR}/algorithmconfigjson.h
)

# Qt-based JSON implementation using Qt5::Core
target_compile_features(jsonreader INTERFACE cxx_std_17)

target_link_libraries(jsonreader
    PUBLIC
        Qt5::Core
        logprinterex
)

if(MSVC)
    # keep warnings consistent for consumers
    target_compile_options(jsonreader INTERFACE /wd4819)
else()
    # Standard C++ compilation options
endif()
