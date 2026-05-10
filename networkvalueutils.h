/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef NETWORKVALUEUTILS_H
#define NETWORKVALUEUTILS_H

#include <QString>

namespace NetworkValueUtils
{
/**
 * Format a network value for display in editable fields.
 *
 * The returned text intentionally uses the C locale and therefore a decimal
 * point. The parser accepts both decimal point and decimal comma.
 */
QString displayNumber(double value, int significantDigits = 10);

/**
 * Parse a UI-entered numeric value with an explicit range.
 *
 * Empty input is treated as zero. Both decimal point and decimal comma are
 * accepted as decimal separators. Mixed decimal separators are rejected so a
 * string such as "1.234,5" cannot be silently misread.
 */
bool parseDisplayValue(const QString& text, double& value, double minimum, double maximum);

/**
 * Parse a non-negative network value as shown in the UI.
 *
 * Empty input is treated as zero. Both decimal point and decimal comma are
 * accepted as decimal separators. Mixed decimal separators are rejected so a
 * string such as "1.234,5" cannot be silently misread.
 */
bool parseNonNegativeDisplayValue(const QString& text, double& value);

QString validationHint();
QString validationError(const QString& label);
}

#endif // NETWORKVALUEUTILS_H
