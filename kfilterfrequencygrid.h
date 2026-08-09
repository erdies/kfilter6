/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERFREQUENCYGRID_H
#define KFILTERFREQUENCYGRID_H

#include <array>
#include <cstddef>

constexpr std::size_t KFilterFrequencyCount = 150;
constexpr double KFilterMinimumFrequencyHz = 20.0;
constexpr double KFilterFrequencyStep = 1.047128548;

using KFilterFrequencyGrid = std::array<double, KFilterFrequencyCount>;

const KFilterFrequencyGrid& kfilterFrequencyGridHz();

#endif // KFILTERFREQUENCYGRID_H
