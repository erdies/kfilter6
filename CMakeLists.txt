# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2002-2026 Martin Erdtmann

cmake_minimum_required(VERSION 3.21)

project(kfilter VERSION 0.1.0 LANGUAGES CXX)

set(KFILTER_PATCH_LEVEL "106")
set(KFILTER_LICENSE_STRING "GPL-3.0-or-later")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

option(KFILTER_ENABLE_WIZARD "Build the optional legacy wizard component" OFF)
option(KFILTER_BUILD_DRIVER_SMOKETEST "Build a small compile/link test for the UI-independent driver core" ON)
option(KFILTER_BUILD_PROJECTIO_SMOKETEST "Build a round-trip test for Qt6 project file serialization" ON)
option(KFILTER_BUILD_DOCUMENT_SMOKETEST "Build a KFilterDoc smoke test on top of the Qt6 core" ON)
option(KFILTER_BUILD_DEFAULTS_SMOKETEST "Build a regression test for historical driver defaults" ON)
option(KFILTER_BUILD_QT6_APP "Build the minimal Qt6 Widgets application shell" ON)

add_library(kfilter_core STATIC
    driver.cpp
    driver.h
    kfilterprojectio.cpp
    kfilterprojectio.h
    kfilterdoc.cpp
    kfilterdoc.h
)

target_link_libraries(kfilter_core
    PUBLIC
        Qt6::Core
)

target_include_directories(kfilter_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(KFILTER_ENABLE_WIZARD)
    target_compile_definitions(kfilter_core PUBLIC KFILTER_ENABLE_WIZARD)
endif()


if(KFILTER_BUILD_QT6_APP)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)

    add_executable(kfilter_qt6
        mainqt6.cpp
        kfilterqt6app.cpp
        kfilterqt6app.h
        driverparametersdialog.cpp
        driverparametersdialog.h
        networkparametersdialog.cpp
        networkparametersdialog.h
        networksectioneditdialog.cpp
        networksectioneditdialog.h
        plotcolorsdialog.cpp
        plotcolorsdialog.h
        networkvalueutils.cpp
        networkvalueutils.h
        circuitout.cpp
        circuitout.h
        kfilterview.cpp
        kfilterview.h
        resources/kfilter6.qrc
    )
    target_link_libraries(kfilter_qt6
        PRIVATE
            kfilter_core
            Qt6::Widgets
    )
    target_compile_definitions(kfilter_qt6
        PRIVATE
            KFILTER_VERSION_STRING="${PROJECT_VERSION}"
            KFILTER_PATCH_LEVEL_STRING="${KFILTER_PATCH_LEVEL}"
            KFILTER_LICENSE_STRING="${KFILTER_LICENSE_STRING}"
    )
endif()

if(KFILTER_BUILD_DRIVER_SMOKETEST)
    add_executable(kfilter_driver_smoketest
        tools/driver_smoketest.cpp
    )
    target_link_libraries(kfilter_driver_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_driver_smoketest COMMAND kfilter_driver_smoketest)
endif()


if(KFILTER_BUILD_PROJECTIO_SMOKETEST)
    add_executable(kfilter_projectio_smoketest
        tools/projectio_smoketest.cpp
    )
    target_link_libraries(kfilter_projectio_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_projectio_smoketest COMMAND kfilter_projectio_smoketest)
endif()


if(KFILTER_BUILD_DOCUMENT_SMOKETEST)
    add_executable(kfilter_doc_smoketest
        tools/kfilterdoc_smoketest.cpp
    )
    target_link_libraries(kfilter_doc_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_doc_smoketest COMMAND kfilter_doc_smoketest)
endif()

if(KFILTER_BUILD_DEFAULTS_SMOKETEST)
    add_executable(kfilter_defaults_smoketest
        tools/defaults_smoketest.cpp
    )
    target_link_libraries(kfilter_defaults_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_defaults_smoketest COMMAND kfilter_defaults_smoketest)
endif()
