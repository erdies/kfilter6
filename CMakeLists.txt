# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2002-2026 Martin Erdtmann

cmake_minimum_required(VERSION 3.21)

project(kfilter VERSION 0.1.0 LANGUAGES CXX)

set(KFILTER_PATCH_LEVEL "298")
set(KFILTER_LICENSE_STRING "GPL-3.0-or-later")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Core)

qt_standard_project_setup()
set(CMAKE_AUTORCC ON)

option(KFILTER_ENABLE_WIZARD "Build the optional legacy wizard component" OFF)
option(KFILTER_BUILD_DRIVER_SMOKETEST "Build a small compile/link test for the UI-independent driver core" ON)
option(KFILTER_BUILD_DRIVERIO_SMOKETEST "Build a complete KFD driver-slot round-trip smoke test" ON)
option(KFILTER_BUILD_PROJECTIO_SMOKETEST "Build a round-trip test for Qt6 project file serialization" ON)
option(KFILTER_BUILD_DRIVER_PARAMETER_VALIDATION_SMOKETEST "Build the Thiele/Small parameter validation test for all deserialization paths" ON)
option(KFILTER_BUILD_DOCUMENT_SMOKETEST "Build a KFilterDoc smoke test on top of the Qt6 core" ON)
option(KFILTER_BUILD_DEFAULTS_SMOKETEST "Build a regression test for historical driver defaults" ON)
option(KFILTER_BUILD_MEASUREMENT_CURVE_SMOKETEST "Build a measurement curve model test" ON)
option(KFILTER_BUILD_MEASUREMENT_IMPORT_SMOKETEST "Build a measurement parser and calibration test" ON)
option(KFILTER_BUILD_MEASUREMENT_EXPORT_SMOKETEST "Build an FRD correction-curve export test" ON)
option(KFILTER_BUILD_QT6_APP "Build the minimal Qt6 Widgets application shell" ON)
option(KFILTER_BUILD_DRIVER_PARAMETERS_DIALOG_SMOKETEST "Build the Driver Parameters enclosure-control state smoke test" ON)
option(KFILTER_BUILD_ACTIVE_FILTER_MODEL_SMOKETEST "Build the active-filter data-model smoke test" ON)
option(KFILTER_BUILD_ACTIVE_FILTER_DIALOG_SMOKETEST "Build the active-filter model/dialog round-trip smoke test" ON)
option(KFILTER_BUILD_ACTIVE_FILTER_RESPONSE_SMOKETEST "Build the active-filter complex transfer-function smoke test" ON)
option(KFILTER_BUILD_BAFFLE_RESPONSE_SMOKETEST "Build the baffle complex transfer-function smoke test" ON)
option(KFILTER_BUILD_BAFFLE_LF_HYBRID_DIAGNOSTIC "Build the LF hybrid baffle production/reference comparison tool" ON)
option(KFILTER_BUILD_FLOOR_REFLECTION_PROCESSING_SMOKETEST "Build the productive Floor Reflection integration smoke test" ON)
option(KFILTER_BUILD_FLOOR_REFLECTION_RESPONSE_SMOKETEST "Build the ideal-rigid floor-reflection response smoke test" ON)
option(KFILTER_BUILD_FLOOR_SURFACE_MODEL_SMOKETEST "Build the Miki porous floor-surface model smoke test" ON)
option(KFILTER_BUILD_FLOOR_SURFACE_DIAGNOSTIC "Build the Miki porous floor-surface comparison diagnostic" ON)
option(KFILTER_BUILD_FLOOR_REFLECTION_DIAGNOSTIC "Build the ideal-rigid floor-reflection Stage-F1 diagnostic tool" ON)
option(KFILTER_BUILD_FLOOR_BAFFLE_COMBINATION_DIAGNOSTIC "Build the Floor Reflection Stage-F2 baffle/placement combination diagnostic" ON)
option(KFILTER_BUILD_BAFFLE_FLOOR_DIAGNOSTIC "Build the rigid-floor/bottom-edge diagnostic tool" ON)
option(KFILTER_BUILD_BAFFLE_FLOOR_ROBUSTNESS_DIAGNOSTIC "Build the rigid-floor image-geometry robustness sweep" ON)
option(KFILTER_BUILD_BAFFLE_CHAMFER_RESPONSE_SMOKETEST "Build the 45-degree chamfer response smoke test" ON)
option(KFILTER_BUILD_BAFFLE_DIALOG_SMOKETEST "Build the Baffle / Diffraction dialog smoke test" ON)
option(KFILTER_BUILD_CIRCUITOUT_ACTIVE_FILTER_HIT_SMOKETEST "Build the Network Preview active-filter hit-zone smoke test" ON)
option(KFILTER_BUILD_CIRCUITOUT_NETWORK_PARAMETERS_HIT_SMOKETEST "Build the Network Preview AC-source hit-zone smoke test" ON)
option(KFILTER_BUILD_CIRCUITOUT_BAFFLE_PARAMETERS_HIT_SMOKETEST "Build the Network Preview Baffle / Diffraction wave-symbol hit-zone smoke test" ON)

add_library(kfilter_core STATIC
    activefiltermodel.cpp
    activefiltermodel.h
    activefilterresponse.cpp
    activefilterresponse.h
    bafflemodel.cpp
    bafflemodel.h
    floorreflectionmodel.h
    floorreflectionprocessing.cpp
    floorreflectionprocessing.h
    floorreflectionresponse.cpp
    floorreflectionresponse.h
    floorsurfacemodel.cpp
    floorsurfacemodel.h
    bafflechamferresponse.cpp
    bafflechamferresponse.h
    baffleresponse.cpp
    baffleresponse.h
    kfilterfrequencygrid.cpp
    kfilterfrequencygrid.h
    driver.cpp
    driver.h
    driverparametervalidation.cpp
    driverparametervalidation.h
    kfilterprojectio.cpp
    kfilterprojectio.h
    kfilterdriverio.cpp
    kfilterdriverio.h
    kfiltermeasurementcurve.cpp
    kfiltermeasurementcurve.h
    measurementcurveparser.cpp
    measurementcurveparser.h
    correctioncurveimport.cpp
    correctioncurveimport.h
    correctioncurveexport.cpp
    correctioncurveexport.h
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
        activefilterparametersdialog.cpp
        activefilterparametersdialog.h
        baffleparametersdialog.cpp
        baffleparametersdialog.h
        bafflegeometrypreview.cpp
        bafflegeometrypreview.h
        networkparametersdialog.cpp
        networkparametersdialog.h
        networksectioneditdialog.cpp
        networksectioneditdialog.h
        plotcolorsdialog.cpp
        plotcolorsdialog.h
        measurementimportdialog.cpp
        measurementimportdialog.h
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

    if(KFILTER_BUILD_DRIVER_PARAMETERS_DIALOG_SMOKETEST)
        add_executable(kfilter_driver_parameters_dialog_smoketest
            tools/driverparameters_dialog_smoketest.cpp
            driverparametersdialog.cpp
            driverparametersdialog.h
            networkvalueutils.cpp
            networkvalueutils.h
        )
        target_link_libraries(kfilter_driver_parameters_dialog_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_driver_parameters_dialog_smoketest
                 COMMAND kfilter_driver_parameters_dialog_smoketest)
        set_tests_properties(kfilter_driver_parameters_dialog_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    if(KFILTER_BUILD_ACTIVE_FILTER_DIALOG_SMOKETEST)
        add_executable(kfilter_active_filter_dialog_smoketest
            tools/activefilter_dialog_smoketest.cpp
            activefilterparametersdialog.cpp
            activefilterparametersdialog.h
            networkvalueutils.cpp
            networkvalueutils.h
        )
        target_link_libraries(kfilter_active_filter_dialog_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_active_filter_dialog_smoketest
                 COMMAND kfilter_active_filter_dialog_smoketest)
        set_tests_properties(kfilter_active_filter_dialog_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    if(KFILTER_BUILD_BAFFLE_DIALOG_SMOKETEST)
        add_executable(kfilter_baffle_dialog_smoketest
            tools/baffle_dialog_smoketest.cpp
            baffleparametersdialog.cpp
            baffleparametersdialog.h
            bafflegeometrypreview.cpp
            bafflegeometrypreview.h
            networkvalueutils.cpp
            networkvalueutils.h
        )
        target_link_libraries(kfilter_baffle_dialog_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_baffle_dialog_smoketest
                 COMMAND kfilter_baffle_dialog_smoketest)
        set_tests_properties(kfilter_baffle_dialog_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    if(KFILTER_BUILD_CIRCUITOUT_ACTIVE_FILTER_HIT_SMOKETEST)
        add_executable(kfilter_circuitout_active_filter_hit_smoketest
            tools/circuitout_active_filter_hit_smoketest.cpp
            circuitout.cpp
            circuitout.h
        )
        target_link_libraries(kfilter_circuitout_active_filter_hit_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_circuitout_active_filter_hit_smoketest
                 COMMAND kfilter_circuitout_active_filter_hit_smoketest)
        set_tests_properties(kfilter_circuitout_active_filter_hit_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    if(KFILTER_BUILD_CIRCUITOUT_NETWORK_PARAMETERS_HIT_SMOKETEST)
        add_executable(kfilter_circuitout_network_parameters_hit_smoketest
            tools/circuitout_network_parameters_hit_smoketest.cpp
            circuitout.cpp
            circuitout.h
        )
        target_link_libraries(kfilter_circuitout_network_parameters_hit_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_circuitout_network_parameters_hit_smoketest
                 COMMAND kfilter_circuitout_network_parameters_hit_smoketest)
        set_tests_properties(kfilter_circuitout_network_parameters_hit_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()

    if(KFILTER_BUILD_CIRCUITOUT_BAFFLE_PARAMETERS_HIT_SMOKETEST)
        add_executable(kfilter_circuitout_baffle_parameters_hit_smoketest
            tools/circuitout_baffle_parameters_hit_smoketest.cpp
            circuitout.cpp
            circuitout.h
        )
        target_link_libraries(kfilter_circuitout_baffle_parameters_hit_smoketest
            PRIVATE
                kfilter_core
                Qt6::Widgets
        )

        enable_testing()
        add_test(NAME kfilter_circuitout_baffle_parameters_hit_smoketest
                 COMMAND kfilter_circuitout_baffle_parameters_hit_smoketest)
        set_tests_properties(kfilter_circuitout_baffle_parameters_hit_smoketest
            PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    endif()
endif()

if(KFILTER_BUILD_ACTIVE_FILTER_MODEL_SMOKETEST)
    add_executable(kfilter_active_filter_model_smoketest
        tools/activefilter_model_smoketest.cpp
    )
    target_link_libraries(kfilter_active_filter_model_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_active_filter_model_smoketest
             COMMAND kfilter_active_filter_model_smoketest)
endif()

if(KFILTER_BUILD_ACTIVE_FILTER_RESPONSE_SMOKETEST)
    add_executable(kfilter_active_filter_response_smoketest
        tools/activefilter_response_smoketest.cpp
    )
    target_link_libraries(kfilter_active_filter_response_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_active_filter_response_smoketest
             COMMAND kfilter_active_filter_response_smoketest)
endif()

if(KFILTER_BUILD_BAFFLE_RESPONSE_SMOKETEST)
    add_executable(kfilter_baffle_response_smoketest
        tools/baffle_response_smoketest.cpp
    )
    target_link_libraries(kfilter_baffle_response_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_baffle_response_smoketest
             COMMAND kfilter_baffle_response_smoketest)
endif()

if(KFILTER_BUILD_BAFFLE_LF_HYBRID_DIAGNOSTIC)
    add_executable(kfilter_baffle_lf_hybrid_diagnostic
        tools/baffle_lf_hybrid_diagnostic.cpp
        tools/baffle_lf_hybrid_diagnostic_model.cpp
        tools/baffle_lf_hybrid_diagnostic_model.h
    )
    target_link_libraries(kfilter_baffle_lf_hybrid_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_FLOOR_REFLECTION_PROCESSING_SMOKETEST)
    add_executable(kfilter_floor_reflection_processing_smoketest
        tools/floor_reflection_processing_smoketest.cpp
    )
    target_link_libraries(kfilter_floor_reflection_processing_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_floor_reflection_processing_smoketest
             COMMAND kfilter_floor_reflection_processing_smoketest)
endif()

if(KFILTER_BUILD_FLOOR_REFLECTION_RESPONSE_SMOKETEST)
    add_executable(kfilter_floor_reflection_response_smoketest
        tools/floor_reflection_response_smoketest.cpp
    )
    target_link_libraries(kfilter_floor_reflection_response_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_floor_reflection_response_smoketest
             COMMAND kfilter_floor_reflection_response_smoketest)
endif()

if(KFILTER_BUILD_FLOOR_SURFACE_MODEL_SMOKETEST)
    add_executable(kfilter_floor_surface_model_smoketest
        tools/floor_surface_model_smoketest.cpp
    )
    target_link_libraries(kfilter_floor_surface_model_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_floor_surface_model_smoketest
             COMMAND kfilter_floor_surface_model_smoketest)
endif()

if(KFILTER_BUILD_FLOOR_SURFACE_DIAGNOSTIC)
    add_executable(kfilter_floor_surface_diagnostic
        tools/floor_surface_diagnostic.cpp
    )
    target_link_libraries(kfilter_floor_surface_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_FLOOR_REFLECTION_DIAGNOSTIC)
    add_executable(kfilter_floor_reflection_diagnostic
        tools/floor_reflection_diagnostic.cpp
    )
    target_link_libraries(kfilter_floor_reflection_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_FLOOR_BAFFLE_COMBINATION_DIAGNOSTIC)
    add_executable(kfilter_floor_baffle_combination_diagnostic
        tools/floor_baffle_combination_diagnostic.cpp
    )
    target_link_libraries(kfilter_floor_baffle_combination_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_BAFFLE_FLOOR_DIAGNOSTIC)
    add_executable(kfilter_baffle_floor_diagnostic
        tools/baffle_floor_diagnostic.cpp
    )
    target_link_libraries(kfilter_baffle_floor_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_BAFFLE_FLOOR_ROBUSTNESS_DIAGNOSTIC)
    add_executable(kfilter_baffle_floor_robustness_diagnostic
        tools/baffle_floor_robustness_diagnostic.cpp
    )
    target_link_libraries(kfilter_baffle_floor_robustness_diagnostic PRIVATE kfilter_core)
endif()

if(KFILTER_BUILD_BAFFLE_CHAMFER_RESPONSE_SMOKETEST)
    add_executable(kfilter_baffle_chamfer_response_smoketest
        tools/baffle_chamfer_response_smoketest.cpp
    )
    target_link_libraries(kfilter_baffle_chamfer_response_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_baffle_chamfer_response_smoketest
             COMMAND kfilter_baffle_chamfer_response_smoketest)
endif()

if(KFILTER_BUILD_DRIVER_SMOKETEST)
    add_executable(kfilter_driver_smoketest
        tools/driver_smoketest.cpp
    )
    target_link_libraries(kfilter_driver_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_driver_smoketest COMMAND kfilter_driver_smoketest)
endif()


if(KFILTER_BUILD_DRIVERIO_SMOKETEST)
    add_executable(kfilter_driverio_smoketest
        tools/driverio_smoketest.cpp
    )
    target_link_libraries(kfilter_driverio_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_driverio_smoketest COMMAND kfilter_driverio_smoketest)
endif()


if(KFILTER_BUILD_PROJECTIO_SMOKETEST)
    add_executable(kfilter_projectio_smoketest
        tools/projectio_smoketest.cpp
    )
    target_link_libraries(kfilter_projectio_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_projectio_smoketest COMMAND kfilter_projectio_smoketest)
endif()


if(KFILTER_BUILD_DRIVER_PARAMETER_VALIDATION_SMOKETEST)
    add_executable(kfilter_driver_parameter_validation_smoketest
        tools/driver_parameter_validation_smoketest.cpp
    )
    target_link_libraries(kfilter_driver_parameter_validation_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_driver_parameter_validation_smoketest
             COMMAND kfilter_driver_parameter_validation_smoketest)
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

if(KFILTER_BUILD_MEASUREMENT_CURVE_SMOKETEST)
    add_executable(kfilter_measurement_curve_smoketest
        tools/measurement_curve_smoketest.cpp
    )
    target_link_libraries(kfilter_measurement_curve_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_measurement_curve_smoketest COMMAND kfilter_measurement_curve_smoketest)
endif()


if(KFILTER_BUILD_MEASUREMENT_IMPORT_SMOKETEST)
    add_executable(kfilter_measurement_import_smoketest
        tools/measurement_import_smoketest.cpp
    )
    target_link_libraries(kfilter_measurement_import_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_measurement_import_smoketest COMMAND kfilter_measurement_import_smoketest)
endif()


if(KFILTER_BUILD_MEASUREMENT_EXPORT_SMOKETEST)
    add_executable(kfilter_measurement_export_smoketest
        tools/measurement_export_smoketest.cpp
    )
    target_link_libraries(kfilter_measurement_export_smoketest PRIVATE kfilter_core)

    enable_testing()
    add_test(NAME kfilter_measurement_export_smoketest COMMAND kfilter_measurement_export_smoketest)
endif()
