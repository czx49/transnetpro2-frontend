add_executable(tnp2_launcher WIN32
    tnp2_launcher.cpp
    envfile.cpp
    tnp2_launcher.rc
)

set_target_properties(tnp2_launcher PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

if(MSVC)
    set_property(TARGET tnp2_launcher PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_link_libraries(tnp2_launcher PRIVATE kernel32 user32)

add_custom_command(TARGET tnp2_launcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/tnp2.env.in
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/tnp2.env
    COMMENT "Copy tnp2.env to output directory"
)
