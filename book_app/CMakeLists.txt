cmake_minimum_required(VERSION 3.16)
project(book_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "$ENV{QT_ROOT}")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_ROOT}")
endif()

find_package(Qt5 COMPONENTS Core Gui Widgets Sql REQUIRED)

add_executable(book_app WIN32
    src/main.cpp
    src/configloader.cpp
    src/configloader.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/addbookdialog.cpp
    src/addbookdialog.h
    src/editbookdialog.cpp
    src/editbookdialog.h
    src/langtable.h
)
target_link_libraries(book_app PRIVATE Qt5::Widgets Qt5::Sql)

# --- Deployment: copy runtime files next to the executable ---

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
get_target_property(_qmake_path Qt5::qmake IMPORTED_LOCATION)

if(WIN32 AND _qmake_path)
    get_filename_component(QT_BIN_DIR "${_qmake_path}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT_BIN_DIR}")
    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET book_app POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations
                    --compiler-runtime "$<TARGET_FILE:book_app>"
            VERBATIM)
    endif()
endif()

add_custom_command(TARGET book_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:book_app>/db"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_ROOT}/config.ini" "$<TARGET_FILE_DIR:book_app>/config.ini"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_ROOT}/no.jpg" "$<TARGET_FILE_DIR:book_app>/no.jpg"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_ROOT}/db/book.sqlite" "$<TARGET_FILE_DIR:book_app>/db/book.sqlite"
    VERBATIM)

