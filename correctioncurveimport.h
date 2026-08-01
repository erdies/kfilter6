/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef CORRECTIONCURVEIMPORT_H
#define CORRECTIONCURVEIMPORT_H

#include "kfiltermeasurementcurve.h"
#include "measurementcurveparser.h"

#include <QString>
#include <QStringList>
#include <QVector>

struct KFilterCorrectionImportSettings
{
    double calibrationMinHz = 0.0;
    double calibrationMaxHz = 0.0;
    double manualOffsetDb = 0.0;

    double correctionMinHz = 0.0;
    double correctionMaxHz = 0.0;

    bool lowerFadeEnabled = true;
    double lowerFadeOctaves = 1.0 / 3.0;

    bool upperFadeEnabled = false;
    double upperFadeOctaves = 1.0 / 3.0;
};

struct KFilterCorrectionImportResult
{
    KFilterMeasurementCurve correctionCurve;
    QVector<KFilterMeasurementPoint> calibratedMeasurement;
    double referenceMedianDb = 0.0;
    double automaticOffsetDb = 0.0;
    double effectiveOffsetDb = 0.0;
    QStringList warnings;
    QString errorMessage;

    bool isValid() const;
};

KFilterCorrectionImportResult createKFilterCorrectionCurve(
    const QVector<KFilterImportedMeasurementPoint>& measurements,
    const KFilterCorrectionImportSettings& settings);

#endif // CORRECTIONCURVEIMPORT_H
