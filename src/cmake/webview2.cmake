# WebView2 configuration helper for Windows (MSVC)

if (NOT WIN32)
    message(STATUS "WebView2 is only available on Windows; skipping configuration.")
    return()
endif()

# Allow users to provide SDK root explicitly
set(WEBVIEW2_SDK_DIR "" CACHE PATH "Path to Microsoft WebView2 SDK root directory")

function(configure_webview2 target)
    if (NOT WIN32)
        return()
    endif()

    # Determine architecture-specific subdir (x64 vs x86)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_arch "x64")
    else()
        set(_arch "x86")
    endif()

    # Candidate include and lib locations
    set(_include_hints)
    set(_lib_hints)

    if (WEBVIEW2_SDK_DIR)
        list(APPEND _include_hints
            ${WEBVIEW2_SDK_DIR}/include
            ${WEBVIEW2_SDK_DIR}/build/native/include
        )
        list(APPEND _lib_hints
            ${WEBVIEW2_SDK_DIR}/build/native/${_arch}
            ${WEBVIEW2_SDK_DIR}/build/native/${_arch}/Release
            ${WEBVIEW2_SDK_DIR}/lib
        )
    endif()

    # Common NuGet layout: packages/Microsoft.Web.WebView2.x.y.z/build/native
    #list(APPEND _include_hints
    #    ${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native/include
    #    ${CMAKE_SOURCE_DIR}/../packages/Microsoft.Web.WebView2/build/native/include
    #)
    #list(APPEND _lib_hints
    #    ${CMAKE_SOURCE_DIR}/packages/Microsoft.Web.WebView2/build/native/${_arch}
    #    ${CMAKE_SOURCE_DIR}/../packages/Microsoft.Web.WebView2/build/native/${_arch}
    #)

    # Try to locate headers and loader libraries
    find_path(WEBVIEW2_INCLUDE_DIR
        NAMES WebView2.h
        HINTS ${_include_hints}
    )

    find_library(WEBVIEW2_LOADER_STATIC
        NAMES WebView2LoaderStatic
        HINTS ${_lib_hints}
    )

    # Optional: dynamic loader import lib (less common)
    find_library(WEBVIEW2_LOADER_DLL_LIB
        NAMES WebView2Loader.dll
        HINTS ${_lib_hints}
    )

    if (WEBVIEW2_INCLUDE_DIR)
        target_include_directories(${target} PRIVATE ${WEBVIEW2_INCLUDE_DIR})
    endif()

    set(_linked FALSE)
    if (WEBVIEW2_LOADER_STATIC)
        target_link_libraries(${target} PRIVATE ${WEBVIEW2_LOADER_STATIC})
        set(_linked TRUE)
    elseif(WEBVIEW2_LOADER_DLL_LIB)
        target_link_libraries(${target} PRIVATE ${WEBVIEW2_LOADER_DLL_LIB})
        set(_linked TRUE)
    endif()

    if (_linked)
        target_compile_definitions(${target} PRIVATE WEBVIEW2_AVAILABLE)
        message(STATUS "WebView2 SDK found: include='${WEBVIEW2_INCLUDE_DIR}', lib='${WEBVIEW2_LOADER_STATIC}${WEBVIEW2_LOADER_DLL_LIB}'")
    else()
        message(WARNING "WebView2 SDK not found; WEBVIEW2_AVAILABLE will not be defined.")
    endif()
endfunction()