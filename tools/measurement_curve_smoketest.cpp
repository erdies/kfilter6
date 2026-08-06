/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfiltermeasurementcurve.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <utility>

namespace
{
bool require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "measurement curve smoke test failed: " << message << '\n';
        return false;
    }
    return true;
}

bool approximatelyEqual(double actual, double expected, double tolerance = 1.0e-9)
{
    return std::abs(actual - expected) <= tolerance;
}
}

int main()
{
    KFilterMeasurementCurve curve;
    const std::uint64_t initialRevision = curve.revision();

    if (!require(curve.isEmpty(), "new curve must be empty") ||
        !require(curve.size() == 0, "new curve size must be zero") ||
        !require(!curve.removeLastPoint(), "removing from an empty curve must fail") ||
        !require(curve.revision() == initialRevision,
                 "failed mutations must not change the curve revision")) {
        return 1;
    }

    KFilterMeasurementCurve neutralCurve;
    if (!require(neutralCurve.isNeutral(), "an empty curve must be neutral") ||
        !require(!neutralCurve.overlapsFrequencyRange(20.0, 20000.0),
                 "an empty curve must not overlap a frequency range") ||
        !require(neutralCurve.appendPoint(20.0, 0.0),
                 "first neutral point must be accepted") ||
        !require(neutralCurve.appendPoint(20000.0, -0.0),
                 "second neutral point must be accepted") ||
        !require(neutralCurve.isNeutral(), "zero-valued points must form a neutral curve") ||
        !require(neutralCurve.overlapsFrequencyRange(20.0, 20000.0),
                 "matching frequency ranges must overlap") ||
        !require(neutralCurve.overlapsFrequencyRange(100.0, 1000.0),
                 "an enclosed frequency range must overlap") ||
        !require(!neutralCurve.overlapsFrequencyRange(20000.001, 30000.0),
                 "a range above the curve must not overlap") ||
        !require(!neutralCurve.overlapsFrequencyRange(1.0, 19.999),
                 "a range below the curve must not overlap") ||
        !require(!neutralCurve.overlapsFrequencyRange(0.0, 1000.0),
                 "a non-positive range start must be rejected") ||
        !require(!neutralCurve.overlapsFrequencyRange(1000.0, 100.0),
                 "a reversed frequency range must be rejected")) {
        return 1;
    }

    const std::uint64_t neutralRevision = neutralCurve.revision();
    if (!require(neutralCurve.setPointValue(1, 0.001), "point value update must succeed") ||
        !require(neutralCurve.revision() != neutralRevision,
                 "point value update must advance the curve revision")) {
        return 1;
    }
    const std::uint64_t changedNeutralRevision = neutralCurve.revision();
    if (!require(neutralCurve.setPointValue(1, 0.001),
                 "setting the existing point value must succeed") ||
        !require(neutralCurve.revision() == changedNeutralRevision,
                 "an unchanged point value must preserve the curve revision")) {
        return 1;
    }
    if (!require(!neutralCurve.isNeutral(),
                 "a non-zero correction point must make the curve active")) {
        return 1;
    }

    if (!require(curve.appendPoint(20.0, -12.5), "first valid point must be accepted") ||
        !require(curve.appendPoint(100.0, -6.0), "increasing frequency must be accepted") ||
        !require(curve.appendPoint(1000.0, 1.5), "third increasing point must be accepted") ||
        !require(curve.size() == 3, "three accepted points expected")) {
        return 1;
    }

    if (!require(!curve.appendPoint(1000.0, 2.0), "duplicate frequency must be rejected") ||
        !require(!curve.appendPoint(999.0, 2.0), "decreasing frequency must be rejected") ||
        !require(!curve.appendPoint(0.0, 0.0), "zero frequency must be rejected") ||
        !require(!curve.appendPoint(-1.0, 0.0), "negative frequency must be rejected") ||
        !require(!curve.appendPoint(std::numeric_limits<double>::quiet_NaN(), 0.0), "NaN frequency must be rejected") ||
        !require(!curve.appendPoint(std::numeric_limits<double>::infinity(), 0.0), "infinite frequency must be rejected") ||
        !require(!curve.appendPoint(2000.0, std::numeric_limits<double>::quiet_NaN()), "NaN value must be rejected") ||
        !require(!curve.appendPoint(2000.0, std::numeric_limits<double>::infinity()), "infinite value must be rejected") ||
        !require(curve.size() == 3, "rejected points must not alter the curve")) {
        return 1;
    }

    double interpolatedValue = 1234.0;
    const double logarithmicMidpoint = std::sqrt(100.0 * 1000.0);
    if (!require(curve.interpolatedValueAt(20.0, interpolatedValue), "first endpoint must be interpolatable") ||
        !require(approximatelyEqual(interpolatedValue, -12.5), "first endpoint value must be exact") ||
        !require(curve.interpolatedValueAt(100.0, interpolatedValue), "interior waypoint must be interpolatable") ||
        !require(approximatelyEqual(interpolatedValue, -6.0), "interior waypoint value must be exact") ||
        !require(curve.interpolatedValueAt(logarithmicMidpoint, interpolatedValue), "logarithmic midpoint must be interpolatable") ||
        !require(approximatelyEqual(interpolatedValue, -2.25), "logarithmic midpoint must average the adjacent dB values") ||
        !require(curve.interpolatedValueAt(1000.0, interpolatedValue), "last endpoint must be interpolatable") ||
        !require(approximatelyEqual(interpolatedValue, 1.5), "last endpoint value must be exact")) {
        return 1;
    }

    interpolatedValue = 1234.0;
    if (!require(!curve.interpolatedValueAt(19.999, interpolatedValue), "frequency below the curve must not be extrapolated") ||
        !require(!curve.interpolatedValueAt(1000.001, interpolatedValue), "frequency above the curve must not be extrapolated") ||
        !require(!curve.interpolatedValueAt(0.0, interpolatedValue), "zero frequency must not be interpolated") ||
        !require(!curve.interpolatedValueAt(std::numeric_limits<double>::quiet_NaN(), interpolatedValue), "NaN frequency must not be interpolated") ||
        !require(approximatelyEqual(interpolatedValue, 1234.0), "failed interpolation must leave the output value unchanged")) {
        return 1;
    }

    if (!require(curve.removeLastPoint(), "last point must be removable") ||
        !require(curve.size() == 2, "two points must remain after undo") ||
        !require(curve.points().constLast().frequencyHz == 100.0, "undo must expose the previous point")) {
        return 1;
    }

    if (!require(curve.interpolatedValueAt(std::sqrt(20.0 * 100.0), interpolatedValue),
                 "a two-point curve must be mergeable") ||
        !require(approximatelyEqual(interpolatedValue, -9.25),
                 "two-point logarithmic midpoint must be correct")) {
        return 1;
    }

    if (!require(curve.removeLastPoint(), "second point must be removable") ||
        !require(!curve.interpolatedValueAt(20.0, interpolatedValue),
                 "a one-point curve must not be mergeable")) {
        return 1;
    }

    curve.clear();
    if (!require(curve.isEmpty(), "clear must remove all points")) {
        return 1;
    }

    KFilterMeasurementCurve sourceCurve;
    sourceCurve.appendPoint(100.0, -1.0);
    sourceCurve.appendPoint(1000.0, 2.0);
    KFilterMeasurementCurve copiedCurve(sourceCurve);
    if (!require(copiedCurve.revision() != sourceCurve.revision(),
                 "a copied curve must receive an independent revision") ||
        !require(copiedCurve.points().size() == sourceCurve.points().size(),
                 "copy construction must preserve all points")) {
        return 1;
    }

    const std::uint64_t assignmentRevision = curve.revision();
    curve = sourceCurve;
    if (!require(curve.revision() != assignmentRevision,
                 "curve assignment must advance the target revision") ||
        !require(curve.points().size() == sourceCurve.points().size(),
                 "curve assignment must preserve all points")) {
        return 1;
    }

    const std::uint64_t copiedSourceRevision = copiedCurve.revision();
    KFilterMeasurementCurve movedCurve(std::move(copiedCurve));
    if (!require(movedCurve.size() == sourceCurve.size(),
                 "move construction must preserve all points") ||
        !require(copiedCurve.revision() != copiedSourceRevision,
                 "move construction must advance the source revision")) {
        return 1;
    }

    KFilterMeasurementCurve moveAssignedCurve;
    const std::uint64_t moveTargetRevision = moveAssignedCurve.revision();
    const std::uint64_t moveSourceRevision = movedCurve.revision();
    moveAssignedCurve = std::move(movedCurve);
    if (!require(moveAssignedCurve.revision() != moveTargetRevision,
                 "move assignment must advance the target revision") ||
        !require(movedCurve.revision() != moveSourceRevision,
                 "move assignment must advance the source revision") ||
        !require(moveAssignedCurve.size() == sourceCurve.size(),
                 "move assignment must preserve all points")) {
        return 1;
    }

    std::cout << "measurement curve smoke test passed\n";
    return 0;
}
