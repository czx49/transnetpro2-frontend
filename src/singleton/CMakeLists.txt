# Singleton INTERFACE library
# Provides header visibility and include directories without building a library
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/sourcegroup.cmake)

# Collect headers
set(SINGLETON_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/call_once.h
    ${CMAKE_CURRENT_SOURCE_DIR}/singleton.h
)

## Translation generation for Singleton removed.

# Create INTERFACE target
add_library(singleton INTERFACE)

# Expose headers to consumers
target_sources(singleton INTERFACE ${SINGLETON_HEADERS})
 

target_include_directories(singleton INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# No compiled sources; IDE organization
if(MSVC)
    source_group("Singleton\\Header Files" FILES ${SINGLETON_HEADERS})
    # Translation Files generation removed for Singleton
    set_interface_sources(singleton "${SINGLETON_HEADERS}" "" "")
endif()
