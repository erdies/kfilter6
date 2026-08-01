/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERMEASUREMENTCURVE_H
#define KFILTERMEASUREMENTCURVE_H

#include <QVector>

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
    KFilterMeasurementCurveType type = KFilterMeasurementCurveType::SplMagnitude;
    QVector<KFilterMeasurementPoint> points;

    bool isEmpty() const;
    qsizetype size() const;
    void clear();
    bool appendPoint(double frequencyHz, double value);
    bool removeLastPoint();
    bool interpolatedValueAt(double frequencyHz, double& value) const;
};

#endif // KFILTERMEASUREMENTCURVE_H
