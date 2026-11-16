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

/**
 * Driver parameters of a model fit carried in the .zma header.
 *
 * tsp_impedance_measure.py writes them as comment lines for driver
 * measurements only, e.g. "* Fit Re: 5.5079 ohm", "* Fit Fs: 46.246 Hz",
 * "* Fit Qms: 2.6933", "* Fit Le: 0.1758 mH". They are offered to the user on
 * import and are never stored with the curve.
 *
 * valid is true only if Re, Fs, Qms, Qes and Qts are all present, finite and
 * positive. hasVoiceCoil is true if Le, R2 and L2 are all present, finite and
 * non-negative.
 */
struct KFilterZmaFitParameters
{
    bool valid = false;
    double rdcOhm = 0.0;
    double fsHz = 0.0;
    double qms = 0.0;
    double qes = 0.0;
    double qts = 0.0;

    bool hasVoiceCoil = false;
    double lspH = 0.0;
    double r2Ohm = 0.0;
    double l2H = 0.0;
};

struct KFilterImpedanceParseResult
{
    KFilterImpedanceReferenceCurve points;
    KFilterZmaFitParameters fitParameters;
    QStringList warnings;
    QString errorMessage;
    int ignoredLineCount = 0;

    bool isValid() const;
};

/**
 * Parse a .zma impedance file: "frequency |Z| [phase]" per row.
 *
 * Empty lines and lines starting with '*', '#', ';' or "//" are comments.
 * Comment lines of the form "Fit <name>: <value> [unit]" fill fitParameters.
 * Columns may be separated by whitespace or semicolons; decimal commas are
 * accepted. Rows with a non-positive frequency, a negative or non-finite |Z|
 * or a non-numeric phase are ignored and counted. Frequencies must strictly
 * increase; at least two valid rows are required.
 */
KFilterImpedanceParseResult parseKFilterZmaFile(const QString& fileName);

#endif // IMPEDANCEREFERENCECURVE_H
