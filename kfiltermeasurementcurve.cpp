/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfiltermeasurementcurve.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <utility>

namespace
{
std::uint64_t nextCurveRevision()
{
    static std::atomic<std::uint64_t> nextRevision{1};
    return nextRevision.fetch_add(1, std::memory_order_relaxed);
}
}

KFilterMeasurementCurve::KFilterMeasurementCurve()
    : m_revision(nextCurveRevision())
{
}

KFilterMeasurementCurve::KFilterMeasurementCurve(const KFilterMeasurementCurve& other)
    : type(other.type),
      m_points(other.m_points),
      m_revision(nextCurveRevision())
{
}

KFilterMeasurementCurve& KFilterMeasurementCurve::operator=(const KFilterMeasurementCurve& other)
{
    if (this != &other) {
        type = other.type;
        m_points = other.m_points;
        markChanged();
    }
    return *this;
}

KFilterMeasurementCurve::KFilterMeasurementCurve(KFilterMeasurementCurve&& other) noexcept
    : type(other.type),
      m_points(std::move(other.m_points)),
      m_revision(nextCurveRevision())
{
    other.markChanged();
}

KFilterMeasurementCurve& KFilterMeasurementCurve::operator=(KFilterMeasurementCurve&& other) noexcept
{
    if (this != &other) {
        type = other.type;
        m_points = std::move(other.m_points);
        markChanged();
        other.markChanged();
    }
    return *this;
}

bool KFilterMeasurementCurve::isEmpty() const
{
    return m_points.isEmpty();
}

qsizetype KFilterMeasurementCurve::size() const
{
    return m_points.size();
}

const QVector<KFilterMeasurementPoint>& KFilterMeasurementCurve::points() const
{
    return m_points;
}

std::uint64_t KFilterMeasurementCurve::revision() const
{
    return m_revision;
}

void KFilterMeasurementCurve::clear()
{
    if (m_points.isEmpty()) {
        return;
    }

    m_points.clear();
    markChanged();
}

bool KFilterMeasurementCurve::appendPoint(double frequencyHz, double value)
{
    if (!std::isfinite(frequencyHz) || !std::isfinite(value) || frequencyHz <= 0.0) {
        return false;
    }

    if (!m_points.isEmpty() && frequencyHz <= m_points.constLast().frequencyHz) {
        return false;
    }

    m_points.append(KFilterMeasurementPoint{frequencyHz, value});
    markChanged();
    return true;
}

bool KFilterMeasurementCurve::setPointValue(qsizetype pointIndex, double value)
{
    if (pointIndex < 0 || pointIndex >= m_points.size() || !std::isfinite(value)) {
        return false;
    }

    KFilterMeasurementPoint& point = m_points[pointIndex];
    if (point.value == value) {
        return true;
    }

    point.value = value;
    markChanged();
    return true;
}

bool KFilterMeasurementCurve::removeLastPoint()
{
    if (m_points.isEmpty()) {
        return false;
    }

    m_points.removeLast();
    markChanged();
    return true;
}

bool KFilterMeasurementCurve::isNeutral() const
{
    return std::all_of(m_points.cbegin(),
                       m_points.cend(),
                       [](const KFilterMeasurementPoint& point) { return point.value == 0.0; });
}

bool KFilterMeasurementCurve::overlapsFrequencyRange(double minimumFrequencyHz,
                                                       double maximumFrequencyHz) const
{
    if (m_points.isEmpty() ||
        !std::isfinite(minimumFrequencyHz) || !std::isfinite(maximumFrequencyHz) ||
        minimumFrequencyHz <= 0.0 || maximumFrequencyHz < minimumFrequencyHz) {
        return false;
    }

    return m_points.constLast().frequencyHz >= minimumFrequencyHz &&
           m_points.constFirst().frequencyHz <= maximumFrequencyHz;
}

bool KFilterMeasurementCurve::interpolatedValueAt(double frequencyHz, double& value) const
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0 || m_points.size() < 2) {
        return false;
    }

    const KFilterMeasurementPoint& first = m_points.constFirst();
    const KFilterMeasurementPoint& last = m_points.constLast();
    if (frequencyHz < first.frequencyHz || frequencyHz > last.frequencyHz) {
        return false;
    }

    const auto upper = std::lower_bound(
        m_points.cbegin(),
        m_points.cend(),
        frequencyHz,
        [](const KFilterMeasurementPoint& point, double frequency) {
            return point.frequencyHz < frequency;
        });

    if (upper == m_points.cend()) {
        value = last.value;
        return true;
    }

    if (upper->frequencyHz == frequencyHz) {
        value = upper->value;
        return true;
    }

    if (upper == m_points.cbegin()) {
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

void KFilterMeasurementCurve::markChanged()
{
    m_revision = nextCurveRevision();
}
