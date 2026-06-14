/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERDRIVERIO_H
#define KFILTERDRIVERIO_H

#include "driver.h"

#include <QString>

class KFilterDriverIo
{
public:
    struct DriverSlot
    {
        driver driverData;
        bool hasTubeDiameterCm = false;
        double tubeDiameterCm = 0.0;
    };

    static constexpr int NetworkUnitCount = 48;

    static bool loadDriverSlotFromFile(const QString& filePath,
                                       DriverSlot& slot,
                                       QString* errorMessage = nullptr);

    static bool saveDriverSlotToFile(const QString& filePath,
                                     const DriverSlot& slot,
                                     QString* errorMessage = nullptr);
};

#endif // KFILTERDRIVERIO_H
