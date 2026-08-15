/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionresponse.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <iostream>
#include <limits>

namespace
{
bool require(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

bool near(double actual, double expected, double tolerance = 1.0e-12)
{
    return std::abs(actual - expected) <= tolerance;
}

bool nearComplex(const std::complex<double>& actual,
                 const std::complex<double>& expected,
                 double tolerance = 1.0e-12)
{
    return near(actual.real(), expected.real(), tolerance) &&
           near(actual.imag(), expected.imag(), tolerance);
}

bool allFinite(const FloorReflectionResponse& response)
{
    for (const std::complex<double>& value : response.values) {
        if (!std::isfinite(value.real()) || !std::isfinite(value.imag())) {
            return false;
        }
    }
    return true;
}
}

int main()
{
    // Reference geometry from the floor-reflection concept examples.
    const FloorReflectionGeometry reference{0.72, 1.05, 2.50};
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(reference);

    if (!require(path.valid, "reference path geometry must be valid") ||
        !require(near(path.directDistanceM, 2.5216859439668533, 2.0e-13),
                 "reference direct distance mismatch") ||
        !require(near(path.imageDistanceM, 3.0631519714176770, 2.0e-13),
                 "reference image distance mismatch") ||
        !require(near(path.pathDifferenceM, 0.5414660274508236, 3.0e-13),
                 "reference path difference mismatch") ||
        !require(near(path.incidenceCosine, 0.5778361689253096, 2.0e-13),
                 "reference incidence cosine mismatch") ||
        !require(near(path.incidenceAngleRad, 0.9547213896171786, 2.0e-13),
                 "reference incidence angle mismatch")) {
        return 1;
    }

    // Gamma = 0 must remove the reflected path exactly at every grid point.
    FloorReflectionResponse response = calculateFloorReflectionResponseWithConstantCoefficient(
        reference, {0.0, 0.0});
    if (!require(response.status == FloorReflectionResponseStatus::Valid,
                 "Gamma=0 response must be valid")) {
        return 1;
    }
    for (const std::complex<double>& value : response.values) {
        if (!require(nearComplex(value, {1.0, 0.0}, 0.0),
                     "Gamma=0 must produce exactly 1+0j")) {
            return 1;
        }
    }

    // With the source on the ideal rigid plane, direct and image paths are
    // identical. Gamma=+1 therefore gives exactly H=2 (+6.0206 dB).
    const FloorReflectionGeometry equalPaths{0.0, 1.05, 2.50};
    response = calculateIdealRigidFloorReflectionResponse(equalPaths);
    const double sixDb = 20.0 * std::log10(2.0);
    if (!require(response.status == FloorReflectionResponseStatus::Valid,
                 "equal-path rigid response must be valid") ||
        !require(near(response.geometry.pathDifferenceM, 0.0, 0.0),
                 "source on floor must give zero path difference") ||
        !require(near(sixDb, 6.020599913279624, 1.0e-15),
                 "20 log10(2) reference changed")) {
        return 1;
    }
    for (const std::complex<double>& value : response.values) {
        if (!require(nearComplex(value, {2.0, 0.0}, 2.0e-15),
                     "equal-path rigid floor must produce H=2 exactly")) {
            return 1;
        }
    }

    // At the first nominal floor-bounce notch, exp(-j*pi) = -1.  The
    // remaining pressure is therefore exactly 1-r_direct/r_image for Gamma=1.
    const double nominalNotchHz = KFilterFloorReflectionSpeedOfSoundMPerS /
                                  (2.0 * path.pathDifferenceM);
    const std::complex<double> notch = calculateFloorReflectionSample(
        path, nominalNotchHz, {1.0, 0.0});
    const double expectedNotchPressure = 1.0 - path.directDistanceM / path.imageDistanceM;
    if (!require(nearComplex(notch, {expectedNotchPressure, 0.0}, 3.0e-15),
                 "nominal first floor-bounce notch mismatch")) {
        return 1;
    }

    // Fixed complex references protect the exp(-j*k*Delta-r) convention.
    response = calculateIdealRigidFloorReflectionResponse(reference);
    if (!require(response.status == FloorReflectionResponseStatus::Valid,
                 "reference rigid response must be valid") ||
        !require(nearComplex(response.values[0],
                             {1.8070872631702244, -0.16223973414532303},
                             2.0e-12),
                 "20 Hz complex rigid reference mismatch") ||
        !require(nearComplex(response.values[75],
                             {1.8231910989351632, 0.0082459893928146},
                             2.0e-12),
                 "grid-index 75 complex rigid reference mismatch") ||
        !require(nearComplex(response.values[149],
                             {1.4781225776288964, -0.6701569834513761},
                             2.0e-11),
                 "last-grid complex rigid reference mismatch") ||
        !require(allFinite(response), "valid rigid response must remain finite")) {
        return 1;
    }

    // A long-distance case exercises the r_direct/r_image -> 1 regime without
    // changing the analytic formula or requiring a special branch.
    const FloorReflectionGeometry longDistance{0.20, 1.30, 4000.0};
    response = calculateIdealRigidFloorReflectionResponse(longDistance);
    if (!require(response.status == FloorReflectionResponseStatus::Valid,
                 "long-distance geometry must be valid") ||
        !require(allFinite(response), "long-distance response must remain finite") ||
        !require(response.geometry.directDistanceM / response.geometry.imageDistanceM > 0.9999999,
                 "long-distance path amplitude ratio must approach unity")) {
        return 1;
    }

    // Invalid inputs bypass the standalone stage neutrally while reporting the
    // error. They must never inject NaN/Inf into a future complex driver path.
    const FloorReflectionGeometry invalidNegative{-0.1, 1.0, 2.0};
    response = calculateIdealRigidFloorReflectionResponse(invalidNegative);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "negative source height must be invalid") ||
        !require(nearComplex(response.values[0], {1.0, 0.0}, 0.0),
                 "invalid response must be neutral")) {
        return 1;
    }

    const FloorReflectionGeometry invalidDegenerate{1.0, 1.0, 0.0};
    response = calculateIdealRigidFloorReflectionResponse(invalidDegenerate);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "zero-length direct path must be invalid") ||
        !require(nearComplex(response.values[75], {1.0, 0.0}, 0.0),
                 "degenerate response must be neutral")) {
        return 1;
    }

    const FloorReflectionGeometry invalidNan{
        std::numeric_limits<double>::quiet_NaN(), 1.0, 2.0};
    response = calculateIdealRigidFloorReflectionResponse(invalidNan);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "NaN geometry must be invalid") ||
        !require(allFinite(response), "invalid neutral response must remain finite")) {
        return 1;
    }

    response = calculateFloorReflectionResponseWithConstantCoefficient(
        reference,
        {std::numeric_limits<double>::infinity(), 0.0});
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "non-finite reflection coefficient must be invalid") ||
        !require(allFinite(response), "invalid coefficient response must remain finite")) {
        return 1;
    }

    response = calculateIdealRigidFloorReflectionResponse(reference, 0.0);
    if (!require(response.status == FloorReflectionResponseStatus::InvalidParameters,
                 "non-positive speed of sound must be invalid") ||
        !require(allFinite(response), "invalid speed response must remain finite")) {
        return 1;
    }

    std::cout << "floor reflection response smoketest: PASS\n";
    return 0;
}
