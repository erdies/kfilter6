#!/bin/bash

cmake -S . -B build \
  -DKFILTER_BUILD_DRIVER_SMOKETEST=ON \
  -DKFILTER_BUILD_PROJECTIO_SMOKETEST=ON \
  -DKFILTER_BUILD_DOCUMENT_SMOKETEST=ON \
  -DKFILTER_BUILD_DEFAULTS_SMOKETEST=ON \
  -DKFILTER_BUILD_QT6_APP=ON

cmake --build build
ctest --test-dir build --output-on-failure
./build/kfilter_qt6

