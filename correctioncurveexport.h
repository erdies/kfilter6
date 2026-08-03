/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef CORRECTIONCURVEEXPORT_H
#define CORRECTIONCURVEEXPORT_H

#include "kfiltermeasurementcurve.h"

#include <QString>

struct KFilterCorrectionCurveExportResult
{
    qsizetype exportedPointCount = 0;
    QString errorMessage;

    bool isValid() const
    {
        return exportedPointCount > 0 && errorMessage.isEmpty();
    }
};

KFilterCorrectionCurveExportResult exportKFilterCorrectionCurveAsFrd(
    const QString& filePath,
    const KFilterMeasurementCurve& curve,
    const QString& driverDescription,
    const QString& patchLevel);

#endif // CORRECTIONCURVEEXPORT_H
