/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERPROJECTIO_H
#define KFILTERPROJECTIO_H

#include "driver.h"

#include <QString>

class KFilterProjectIo
{
public:
    static constexpr int DriverCount = 4;
    static constexpr int NetworkUnitCount = 48;

    static bool loadFromFile(const QString& filePath,
                             driver (&drivers)[DriverCount],
                             QString* errorMessage = nullptr);

    static bool saveToFile(const QString& filePath,
                           driver (&drivers)[DriverCount],
                           QString* errorMessage = nullptr);
};

#endif // KFILTERPROJECTIO_H
