cmake_minimum_required(VERSION 3.16)
project(conver_db LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(NOT DEFINED CMAKE_PREFIX_PATH AND EXISTS "$ENV{QT_ROOT}")
    list(APPEND CMAKE_PREFIX_PATH "$ENV{QT_ROOT}")
endif()

find_package(Qt5 COMPONENTS Core Gui Widgets Sql REQUIRED)

add_library(mdblib STATIC
    src/mdblib.cpp
)
target_include_directories(mdblib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(mdblib PUBLIC Qt5::Core)

add_executable(conver_db
    src/main.cpp
    src/converter.cpp
    src/converter.h
    src/mainwindow.cpp
    src/mainwindow.h
)
target_link_libraries(conver_db PRIVATE mdblib Qt5::Widgets Qt5::Sql)
