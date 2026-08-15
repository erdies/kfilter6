/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERPROJECTIO_H
#define KFILTERPROJECTIO_H

#include "activefiltermodel.h"
#include "bafflemodel.h"
#include "driver.h"
#include "floorreflectionmodel.h"
#include "kfiltermeasurementcurve.h"

#include <QString>

#include <array>

class KFilterProjectIo
{
public:
    static constexpr int DriverCount = 4;
    static constexpr int NetworkUnitCount = 48;
    static constexpr int LegacyJsonFormatVersion = 1;
    static constexpr int JsonFormatVersion = 10;

    using ActiveFilterChains = std::array<ActiveFilterChain, DriverCount>;
    using BaffleSettingsPerDriver = std::array<BaffleSettings, DriverCount>;
    using FloorReflectionSettingsPerDriver = std::array<FloorReflectionSettings, DriverCount>;
    using MeasurementCurves = std::array<KFilterMeasurementCurve, DriverCount>;
    using MeasurementHiddenStates = std::array<bool, DriverCount>;

    static bool loadFromFile(const QString& filePath,
                             driver (&drivers)[DriverCount],
                             MeasurementCurves& splCorrectionCurves,
                             bool& mergeMeasurementsEnabled,
                             MeasurementHiddenStates& measurementHiddenForDrivers,
                             ActiveFilterChains& activeFilterChains,
                             BaffleSettingsPerDriver& baffleSettings,
                             FloorReflectionSettingsPerDriver& floorReflectionSettings,
                             QString* errorMessage = nullptr);

    static bool saveToFile(const QString& filePath,
                           driver (&drivers)[DriverCount],
                           const MeasurementCurves& splCorrectionCurves,
                           bool mergeMeasurementsEnabled,
                           const MeasurementHiddenStates& measurementHiddenForDrivers,
                           const ActiveFilterChains& activeFilterChains,
                           const BaffleSettingsPerDriver& baffleSettings,
                           const FloorReflectionSettingsPerDriver& floorReflectionSettings,
                           QString* errorMessage = nullptr);
};

#endif // KFILTERPROJECTIO_H
