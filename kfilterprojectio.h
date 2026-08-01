/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERPROJECTIO_H
#define KFILTERPROJECTIO_H

#include "driver.h"
#include "kfiltermeasurementcurve.h"

#include <QString>

#include <array>

class KFilterProjectIo
{
public:
    static constexpr int DriverCount = 4;
    static constexpr int NetworkUnitCount = 48;
    static constexpr int LegacyJsonFormatVersion = 1;
    static constexpr int JsonFormatVersion = 2;

    using MeasurementCurves = std::array<KFilterMeasurementCurve, DriverCount>;

    static bool loadFromFile(const QString& filePath,
                             driver (&drivers)[DriverCount],
                             MeasurementCurves& splCorrectionCurves,
                             bool& mergeMeasurementsEnabled,
                             QString* errorMessage = nullptr);

    static bool saveToFile(const QString& filePath,
                           driver (&drivers)[DriverCount],
                           const MeasurementCurves& splCorrectionCurves,
                           bool mergeMeasurementsEnabled,
                           QString* errorMessage = nullptr);
};

#endif // KFILTERPROJECTIO_H
