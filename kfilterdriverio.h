/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERDRIVERIO_H
#define KFILTERDRIVERIO_H

#include "driver.h"
#include "kfilterprojectio.h"

#include <QString>

class KFilterDoc;

class KFilterDriverIo
{
public:
    struct DriverSlot
    {
        driver driverData;
        KFilterMeasurementCurve measurementCurve;
        bool measurementHidden = false;
        bool mergeMeasurementsEnabled = false;
        ActiveFilterChain activeFilterChain;
        BaffleSettings baffleSettings;
        FloorReflectionSettings floorReflectionSettings;
        bool hasTubeDiameterCm = false;
        double tubeDiameterCm = 0.0;
    };

    static constexpr int NetworkUnitCount = 48;

    static bool mergeMeasurementsEnabledAfterImport(bool existingProjectState,
                                                    bool otherDriversHaveMeasurements,
                                                    bool importedState);

    static bool applyDriverSlotToDocument(KFilterDoc& document,
                                          int driverIndex,
                                          const DriverSlot& slot);

    static bool loadDriverSlotFromFile(const QString& filePath,
                                       DriverSlot& slot,
                                       QString* errorMessage = nullptr);

    static bool saveDriverSlotToFile(const QString& filePath,
                                     const DriverSlot& slot,
                                     QString* errorMessage = nullptr);
};

#endif // KFILTERDRIVERIO_H
