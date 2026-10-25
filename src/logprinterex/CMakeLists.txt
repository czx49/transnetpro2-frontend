cmake_minimum_required(VERSION 3.16)

project(logprinterex LANGUAGES CXX)
set(LOGPRINTEREX_HEADERS
    logprinter_ex.h
)

add_library(logprinterex INTERFACE)

# Expose headers and sources for IDEs and consumers
target_include_directories(logprinterex
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)
target_sources(logprinterex INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/logprinter_ex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/logprinter_ex.h
)

# Propagate dependency on log4qt to consumers (public export)
target_link_libraries(logprinterex
    INTERFACE
        log4qt
)

if(MSVC)
    # keep compile option hint for consumers
    target_compile_options(logprinterex INTERFACE /wd4819)
endif()
