cmake_minimum_required(VERSION 3.16)
project(excel_mowan_gui LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(SOURCES
    analysisbridge.cpp
    main.cpp
    mainwindow.cpp
    spreadsheetmodel.cpp
    spreadsheetview.cpp
    ../app/table_file_io.cpp
    ../core/basic.cpp
    ../core/formula.cpp
    ../core/spreadsheet.cpp
    ../io/csv_file.cpp
    ../io/dat_file.cpp
)

add_executable(excel_mowan_gui ${SOURCES})

target_include_directories(excel_mowan_gui PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../app
    ${CMAKE_CURRENT_SOURCE_DIR}/../core
    ${CMAKE_CURRENT_SOURCE_DIR}/../io
)

target_link_libraries(excel_mowan_gui PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
