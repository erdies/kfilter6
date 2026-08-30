/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driverparametervalidation.h"

#include "driver.h"

#include <array>
#include <cmath>

namespace
{

void setReason(QString* reason, const QString& message)
{
    if (reason != nullptr) {
        *reason = message;
    }
}

struct NamedValue
{
    const char* name;
    double value;
};

} // namespace

namespace KFilterDriverValidation
{

bool validateDriverParameters(const Parameters& parameters, QString* reason)
{
    const std::array<NamedValue, 11> nonNegative{{
        {"Rdc", parameters.rdcOhm},
        {"Lsp", parameters.lspH},
        {"F0", parameters.fsHz},
        {"Qts", parameters.qts},
        {"Qes", parameters.qes},
        {"Qms", parameters.qms},
        {"Vas", parameters.vasLitres},
        {"Dm", parameters.diameterCm},
        {"Vb", parameters.vbLitres},
        {"Fb", parameters.fbHz},
        {"V2", parameters.v2Litres},
    }};

    for (const NamedValue& entry : nonNegative) {
        if (!std::isfinite(entry.value)) {
            setReason(reason,
                      QStringLiteral("%1 must be a finite number.")
                          .arg(QLatin1String(entry.name)));
            return false;
        }
        if (entry.value < 0.0) {
            setReason(reason,
                      QStringLiteral("%1 must not be negative.")
                          .arg(QLatin1String(entry.name)));
            return false;
        }
    }

    if (parameters.enclosureTypeProposal < static_cast<int>(EnclosureType::OpenBaffle) ||
        parameters.enclosureTypeProposal > static_cast<int>(EnclosureType::Bandpass)) {
        setReason(reason, QStringLiteral("Enclosure type proposal must be between 0 and 3."));
        return false;
    }

    // The full-circuit normalization uses sqrt(8/Rdc) unconditionally, and the
    // impedance model degenerates to a pure inductance without a voice-coil
    // resistance.
    if (parameters.rdcOhm <= 0.0) {
        setReason(reason, QStringLiteral("Rdc must be greater than zero."));
        return false;
    }

    // F0 == 0 deliberately bypasses the acoustic T/S path; the impedance then
    // reduces to Rdc + j*omega*Lsp and none of the quantities below is used.
    if (parameters.fsHz == 0.0) {
        return true;
    }

    const std::array<NamedValue, 3> positiveQualityFactors{{
        {"Qts", parameters.qts},
        {"Qes", parameters.qes},
        {"Qms", parameters.qms},
    }};

    for (const NamedValue& entry : positiveQualityFactors) {
        if (entry.value <= 0.0) {
            setReason(reason,
                      QStringLiteral("%1 must be greater than zero when F0 is non-zero.")
                          .arg(QLatin1String(entry.name)));
            return false;
        }
    }

    // Exactly the condition under which calculateParameters() evaluates the
    // tuned enclosure branch and divides by Vas.
    const bool tunedEnclosureActive =
        parameters.vbLitres > 0.0 && parameters.fbHz > 0.0 &&
        parameters.enclosureTypeProposal >= static_cast<int>(EnclosureType::Vented);

    if (tunedEnclosureActive && parameters.vasLitres <= 0.0) {
        setReason(reason,
                  QStringLiteral("Vas must be greater than zero for a Vented or Bandpass "
                                 "enclosure with a tuning frequency."));
        return false;
    }

    return true;
}

} // namespace KFilterDriverValidation
