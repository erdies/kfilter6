/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfiltermeasurementcurve.h"

#include <algorithm>
#include <cmath>

bool KFilterMeasurementCurve::isEmpty() const
{
    return points.isEmpty();
}

qsizetype KFilterMeasurementCurve::size() const
{
    return points.size();
}

void KFilterMeasurementCurve::clear()
{
    points.clear();
}

bool KFilterMeasurementCurve::appendPoint(double frequencyHz, double value)
{
    if (!std::isfinite(frequencyHz) || !std::isfinite(value) || frequencyHz <= 0.0) {
        return false;
    }

    if (!points.isEmpty() && frequencyHz <= points.constLast().frequencyHz) {
        return false;
    }

    points.append(KFilterMeasurementPoint{frequencyHz, value});
    return true;
}

bool KFilterMeasurementCurve::removeLastPoint()
{
    if (points.isEmpty()) {
        return false;
    }

    points.removeLast();
    return true;
}

bool KFilterMeasurementCurve::interpolatedValueAt(double frequencyHz, double& value) const
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0 || points.size() < 2) {
        return false;
    }

    const KFilterMeasurementPoint& first = points.constFirst();
    const KFilterMeasurementPoint& last = points.constLast();
    if (frequencyHz < first.frequencyHz || frequencyHz > last.frequencyHz) {
        return false;
    }

    const auto upper = std::lower_bound(
        points.cbegin(),
        points.cend(),
        frequencyHz,
        [](const KFilterMeasurementPoint& point, double frequency) {
            return point.frequencyHz < frequency;
        });

    if (upper == points.cend()) {
        value = last.value;
        return true;
    }

    if (upper->frequencyHz == frequencyHz) {
        value = upper->value;
        return true;
    }

    if (upper == points.cbegin()) {
        value = first.value;
        return true;
    }

    const KFilterMeasurementPoint& right = *upper;
    const KFilterMeasurementPoint& left = *(upper - 1);
    const double logarithmicSpan = std::log(right.frequencyHz) - std::log(left.frequencyHz);
    if (!std::isfinite(logarithmicSpan) || logarithmicSpan <= 0.0) {
        return false;
    }

    const double fraction =
        (std::log(frequencyHz) - std::log(left.frequencyHz)) / logarithmicSpan;
    const double interpolatedValue = left.value + fraction * (right.value - left.value);
    if (!std::isfinite(interpolatedValue)) {
        return false;
    }

    value = interpolatedValue;
    return true;
}
