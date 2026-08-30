/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef DRIVERPARAMETERVALIDATION_H
#define DRIVERPARAMETERVALIDATION_H

#include <QString>

/**
 * Shared plausibility check for the Thiele/Small and enclosure parameters of a
 * single driver.
 *
 * The acoustic core in driver.cpp divides by Rdc, Qts, Qes, Qms and, on the
 * Vented and Bandpass paths, by Vas. None of those divisions is guarded, so a
 * project or driver file containing a zero or negative value silently produces
 * non-finite pressure and impedance samples across the whole frequency grid.
 * All three deserialization paths (.kfp JSON, legacy text .kfp and .kfd) call
 * validateDriverParameters() before applying values to a driver instance, so
 * that such a file is rejected with a readable message instead of loading.
 *
 * The rules deliberately reject only combinations that the acoustic core
 * cannot evaluate. Files that produced finite results before Patch 298 keep
 * loading unchanged.
 */
namespace KFilterDriverValidation
{

struct Parameters
{
    double rdcOhm = 0.0;
    double lspH = 0.0;
    double fsHz = 0.0;
    double qts = 0.0;
    double qes = 0.0;
    double qms = 0.0;
    double vasLitres = 0.0;
    double diameterCm = 0.0;
    double vbLitres = 0.0;
    double fbHz = 0.0;
    double v2Litres = 0.0;
    int enclosureTypeProposal = 0;
};

/**
 * Returns true if the parameter set can be evaluated by the acoustic core.
 *
 * On failure the optional reason receives a complete sentence naming the
 * physical parameter, for example
 * "Qes must be greater than zero when F0 is non-zero.". Callers prefix their
 * own file and driver context; the physical names are identical in all
 * supported formats and match the Driver Parameters dialog.
 */
bool validateDriverParameters(const Parameters& parameters, QString* reason = nullptr);

} // namespace KFilterDriverValidation

#endif // DRIVERPARAMETERVALIDATION_H
