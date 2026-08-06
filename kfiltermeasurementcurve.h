/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERMEASUREMENTCURVE_H
#define KFILTERMEASUREMENTCURVE_H

#include <QVector>

#include <cstdint>

enum class KFilterMeasurementCurveType
{
    SplMagnitude
};

struct KFilterMeasurementPoint
{
    double frequencyHz = 0.0;
    double value = 0.0;
};

class KFilterMeasurementCurve
{
public:
    KFilterMeasurementCurve();
    KFilterMeasurementCurve(const KFilterMeasurementCurve& other);
    KFilterMeasurementCurve& operator=(const KFilterMeasurementCurve& other);
    KFilterMeasurementCurve(KFilterMeasurementCurve&& other) noexcept;
    KFilterMeasurementCurve& operator=(KFilterMeasurementCurve&& other) noexcept;

    KFilterMeasurementCurveType type = KFilterMeasurementCurveType::SplMagnitude;

    bool isEmpty() const;
    qsizetype size() const;
    const QVector<KFilterMeasurementPoint>& points() const;
    std::uint64_t revision() const;

    void clear();
    bool appendPoint(double frequencyHz, double value);
    bool setPointValue(qsizetype pointIndex, double value);
    bool removeLastPoint();
    bool isNeutral() const;
    bool overlapsFrequencyRange(double minimumFrequencyHz, double maximumFrequencyHz) const;
    bool interpolatedValueAt(double frequencyHz, double& value) const;

private:
    QVector<KFilterMeasurementPoint> m_points;
    std::uint64_t m_revision = 0;

    void markChanged();
};

#endif // KFILTERMEASUREMENTCURVE_H
