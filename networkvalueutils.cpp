/***************************************************************************
                          networkvalueutils.cpp  -  shared network value helpers
                             -------------------
    begin                : May 2026
    copyright            : (C) 2002-2026 by Martin Erdtmann
 ***************************************************************************/

#include "networkvalueutils.h"

#include <QLocale>
#include <QObject>

#include <cmath>
#include <limits>

namespace NetworkValueUtils
{
QString displayNumber(double value, int significantDigits)
{
    if (!std::isfinite(value)) {
        value = 0.0;
    }
    return QLocale::c().toString(value, 'g', significantDigits);
}

bool parseDisplayValue(const QString& text, double& value, double minimum, double maximum)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        value = 0.0;
        return minimum <= 0.0 && maximum >= 0.0;
    }

    const bool hasComma = trimmed.contains(QLatin1Char(','));
    const bool hasDot = trimmed.contains(QLatin1Char('.'));
    if (hasComma && hasDot) {
        return false;
    }

    QString normalized = trimmed;
    if (hasComma) {
        normalized.replace(QLatin1Char(','), QLatin1Char('.'));
    }

    bool ok = false;
    const double parsed = QLocale::c().toDouble(normalized, &ok);
    if (!ok || !std::isfinite(parsed) || parsed < minimum || parsed > maximum) {
        return false;
    }

    value = parsed;
    return true;
}

bool parseNonNegativeDisplayValue(const QString& text, double& value)
{
    return parseDisplayValue(text, value, 0.0, std::numeric_limits<double>::max());
}

QString validationHint()
{
    return QObject::tr("Empty fields are treated as 0. Values must be finite and non-negative. Decimal point and decimal comma are accepted, but do not mix them.");
}

QString validationError(const QString& label)
{
    return QObject::tr("Invalid value for %1. Use a finite non-negative number; empty fields are treated as 0. Decimal point and decimal comma are accepted, but do not mix them.")
        .arg(label);
}
}
