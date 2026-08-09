/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterfrequencygrid.h"

const KFilterFrequencyGrid& kfilterFrequencyGridHz()
{
    static const KFilterFrequencyGrid frequencies = []() {
        KFilterFrequencyGrid values{};
        values[0] = KFilterMinimumFrequencyHz;
        for (std::size_t sampleIndex = 1; sampleIndex < KFilterFrequencyCount; ++sampleIndex) {
            values[sampleIndex] = values[sampleIndex - 1] * KFilterFrequencyStep;
        }
        return values;
    }();

    return frequencies;
}
