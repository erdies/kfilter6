/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef IMPEDANCEREFERENCECURVE_H
#define IMPEDANCEREFERENCECURVE_H

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * One point of a measured driver impedance (.zma).
 *
 * The phase is kept for later use; the plot currently shows |Z| only.
 * phaseDeg is NaN when the source row carried no phase column.
 */
struct KFilterImpedancePoint
{
    double frequencyHz = 0.0;
    double magnitudeOhm = 0.0;
    double phaseDeg = 0.0;
};

/**
 * Measured impedance shown as a reference next to the simulated impedance.
 * It never influences the simulation or the impedance sum.
 */
using KFilterImpedanceReferenceCurve = QVector<KFilterImpedancePoint>;

struct KFilterImpedanceParseResult
{
    KFilterImpedanceReferenceCurve points;
    QStringList warnings;
    QString errorMessage;
    int ignoredLineCount = 0;

    bool isValid() const;
};

/**
 * Parse a .zma impedance file: "frequency |Z| [phase]" per row.
 *
 * Empty lines and lines starting with '*', '#', ';' or "//" are comments.
 * Columns may be separated by whitespace or semicolons; decimal commas are
 * accepted. Rows with a non-positive frequency, a negative or non-finite |Z|
 * or a non-numeric phase are ignored and counted. Frequencies must strictly
 * increase; at least two valid rows are required.
 */
KFilterImpedanceParseResult parseKFilterZmaFile(const QString& fileName);

#endif // IMPEDANCEREFERENCECURVE_H
