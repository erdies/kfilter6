/***************************************************************************
                          kfilterprojectio.h  -  Qt6 project file I/O
                             -------------------
    copyright            : (C) 2002 by Martin Erdtmann
                           Qt6 porting helper added 2026
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

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
