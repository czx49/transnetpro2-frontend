
function(setup_launchers PROJECT_NAME)
    # Generate Qt Creator launchers.json for debugger environment injection
    set(LAUNCHER_FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.launchers.json")
    set(PROJECT_NAME "tnp_app")
    file(WRITE ${LAUNCHER_FILE} "
    {
        \"version\": 2,
        \"configurations\": [
            {
                \"type\": \"qtapplication\",
                \"name\": \"${PROJECT_NAME} (Debug)\",
                \"project\": \"${PROJECT_NAME}\",
                \"projectTarget\": \"${PROJECT_NAME}\",
                \"buildType\": \"Debug\",
                \"environment\": [
                    { \"name\": \"PATH\", \"value\": \"${QT_BIN_DIR};${GDAL_BIN_DIR};${QGIS_BIN_DIR};%PATH%\" }
                ],
                \"workingDirectory\": \"${DEFAULT_BIN_SUBDIR}\",
                \"program\": \"${DEFAULT_BIN_SUBDIR}/${PROJECT_NAME}.exe\"
            },
            {
                \"type\": \"qtapplication\",
                \"name\": \"${PROJECT_NAME} (Release)\",
                \"project\": \"${PROJECT_NAME}\",
                \"projectTarget\": \"${PROJECT_NAME}\",
                \"buildType\": \"Release\",
                \"environment\": [
                    { \"name\": \"PATH\", \"value\": \"${QT_BIN_DIR};${GDAL_BIN_DIR};${QGIS_BIN_DIR};%PATH%\" }
                ],
                \"workingDirectory\": \"${DEFAULT_BIN_SUBDIR}\",
                \"program\": \"${DEFAULT_BIN_SUBDIR}/${PROJECT_NAME}.exe\"
            }
        ]
    }
    ")


    # Tell Qt Creator to load this launchers.json
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
    set_property(TARGET ${PROJECT_NAME} PROPERTY QT_CREATOR_LAUNCH_CONFIG_FILE ${LAUNCHER_FILE})
endfunction()