# 公共控件基类库（tablemodelbase 是模板类，实现都在头文件中）
set(PUBLICCTRLS_SOURCES
    framelessdialog.cpp
    messagedialog.cpp
    numberdisplayutil.cpp
    phase2comboboxutil.cpp
    phase2dialogstyle.cpp
    phase2treeutil.cpp
    sqltablewithaliasnamemodel.cpp
    treemodelbase.cpp
)

set(PUBLICCTRLS_HEADERS
    framelessdialog.h
    messagedialog.h
    numberdisplayutil.h
    phase2comboboxutil.h
    phase2dialogstyle.h
    phase2treeutil.h
    sqltablewithaliasnamemodel.h
    treemodelbase.h
    tablemodelbase.h
    tableview.h
)

add_library(publicctrls STATIC ${PUBLICCTRLS_SOURCES} ${PUBLICCTRLS_HEADERS})

target_include_directories(publicctrls PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(publicctrls PUBLIC
    Qt5::Core
    Qt5::Widgets
    Qt5::Sql
)
