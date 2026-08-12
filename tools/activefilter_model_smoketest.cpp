/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefiltermodel.h"

#include <array>
#include <cmath>
#include <iostream>
#include <variant>

namespace
{
bool near(double left, double right)
{
    return std::abs(left - right) < 1.0e-9;
}
}

int main()
{
    ActiveFilterChain chain;
    if (chain.enabled() || chain.showResponseInPlot() || !chain.empty() || chain.sectionCount() != 0) {
        std::cerr << "default active-filter chain is not neutral\n";
        return 1;
    }

    chain.setEnabled(true);
    chain.setShowResponseInPlot(true);

    constexpr std::array<ActiveFilterType, 11> types{
        ActiveFilterType::LowPass,
        ActiveFilterType::HighPass,
        ActiveFilterType::BandPass,
        ActiveFilterType::Notch,
        ActiveFilterType::AllPass,
        ActiveFilterType::Gain,
        ActiveFilterType::Delay,
        ActiveFilterType::Polarity,
        ActiveFilterType::PeakingEq,
        ActiveFilterType::LowShelf,
        ActiveFilterType::HighShelf
    };

    for (ActiveFilterType type : types) {
        chain.addSection(type);
    }

    if (!chain.enabled() || !chain.showResponseInPlot() || chain.sectionCount() != types.size()) {
        std::cerr << "active-filter chain state/add operation failed\n";
        return 2;
    }

    for (std::size_t index = 0; index < types.size(); ++index) {
        if (chain.section(index).type() != types[index]) {
            std::cerr << "active-filter section type/variant mapping failed\n";
            return 3;
        }
    }

    auto& lowPass = std::get<ActiveFilterLowPassParameters>(chain.section(0).parameters());
    lowPass.characteristic = ActiveFilterCharacteristic::LinkwitzRiley;
    lowPass.order = 4;
    lowPass.frequencyHz = 2500.0;
    if (lowPass.characteristic != ActiveFilterCharacteristic::LinkwitzRiley ||
        lowPass.order != 4 || !near(lowPass.frequencyHz, 2500.0)) {
        std::cerr << "typed low-pass parameters failed\n";
        return 4;
    }

    auto& notch = std::get<ActiveFilterNotchParameters>(chain.section(3).parameters());
    notch.centerFrequencyHz = 4200.0;
    notch.q = 3.5;
    notch.gainDb = -6.0;
    if (!near(notch.centerFrequencyHz, 4200.0) ||
        !near(notch.q, 3.5) || !near(notch.gainDb, -6.0)) {
        std::cerr << "typed notch parameters failed\n";
        return 5;
    }

    auto& peakingEq = std::get<ActiveFilterPeakingEqParameters>(chain.section(8).parameters());
    peakingEq.centerFrequencyHz = 1800.0;
    peakingEq.q = 2.25;
    peakingEq.gainDb = 5.5;
    if (!near(peakingEq.centerFrequencyHz, 1800.0) ||
        !near(peakingEq.q, 2.25) || !near(peakingEq.gainDb, 5.5)) {
        std::cerr << "typed Peaking-EQ parameters failed\n";
        return 6;
    }

    auto& lowShelf = std::get<ActiveFilterLowShelfParameters>(chain.section(9).parameters());
    lowShelf.transitionFrequencyHz = 320.0;
    lowShelf.q = 0.8;
    lowShelf.gainDb = 4.5;
    if (!near(lowShelf.transitionFrequencyHz, 320.0) ||
        !near(lowShelf.q, 0.8) || !near(lowShelf.gainDb, 4.5)) {
        std::cerr << "typed Low-Shelf parameters failed\n";
        return 6;
    }

    auto& highShelf = std::get<ActiveFilterHighShelfParameters>(chain.section(10).parameters());
    highShelf.transitionFrequencyHz = 6400.0;
    highShelf.q = 0.65;
    highShelf.gainDb = -3.0;
    if (!near(highShelf.transitionFrequencyHz, 6400.0) ||
        !near(highShelf.q, 0.65) || !near(highShelf.gainDb, -3.0)) {
        std::cerr << "typed High-Shelf parameters failed\n";
        return 6;
    }

    chain.section(7).setEnabled(false);
    if (chain.section(7).enabled()) {
        std::cerr << "per-section enable state failed\n";
        return 6;
    }

    chain.section(1).setType(ActiveFilterType::Delay);
    if (chain.section(1).type() != ActiveFilterType::Delay ||
        !std::holds_alternative<ActiveFilterDelayParameters>(chain.section(1).parameters())) {
        std::cerr << "type change did not replace parameters with matching typed defaults\n";
        return 7;
    }

    // Rebuild a compact three-section chain to test ordering semantics.
    ActiveFilterChain ordered;
    ordered.addSection(ActiveFilterType::HighPass);
    ordered.addSection(ActiveFilterType::LowPass);
    ordered.addSection(ActiveFilterType::Notch);

    if (!ordered.moveSection(2, 1) ||
        ordered.section(0).type() != ActiveFilterType::HighPass ||
        ordered.section(1).type() != ActiveFilterType::Notch ||
        ordered.section(2).type() != ActiveFilterType::LowPass) {
        std::cerr << "active-filter move operation failed\n";
        return 8;
    }

    if (!ordered.removeSection(1) || ordered.sectionCount() != 2 ||
        ordered.section(0).type() != ActiveFilterType::HighPass ||
        ordered.section(1).type() != ActiveFilterType::LowPass) {
        std::cerr << "active-filter remove operation failed\n";
        return 9;
    }

    if (ordered.removeSection(99) || ordered.moveSection(0, 99)) {
        std::cerr << "active-filter bounds checks failed\n";
        return 10;
    }

    ordered.clearSections();
    if (!ordered.empty()) {
        std::cerr << "active-filter clear operation failed\n";
        return 11;
    }

    std::cout << "active-filter data model smoke test passed\n";
    return 0;
}
