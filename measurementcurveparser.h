/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef MEASUREMENTCURVEPARSER_H
#define MEASUREMENTCURVEPARSER_H

#include <QString>
#include <QStringList>
#include <QVector>

struct KFilterImportedMeasurementPoint
{
    double frequencyHz = 0.0;
    double levelDb = 0.0;
};

struct KFilterMeasurementParseResult
{
    QVector<KFilterImportedMeasurementPoint> points;
    QStringList warnings;
    QString errorMessage;
    int ignoredLineCount = 0;
    int duplicateFrequencyCount = 0;

    bool isValid() const;
};

KFilterMeasurementParseResult parseKFilterMeasurementFile(const QString& fileName);

#endif // MEASUREMENTCURVEPARSER_H
