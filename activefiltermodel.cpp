/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefiltermodel.h"

#include <type_traits>

namespace
{
ActiveFilterParameters defaultParameters(ActiveFilterType type)
{
    switch (type) {
    case ActiveFilterType::LowPass:
        return ActiveFilterLowPassParameters{};
    case ActiveFilterType::HighPass:
        return ActiveFilterHighPassParameters{};
    case ActiveFilterType::BandPass:
        return ActiveFilterBandPassParameters{};
    case ActiveFilterType::Notch:
        return ActiveFilterNotchParameters{};
    case ActiveFilterType::PeakingEq:
        return ActiveFilterPeakingEqParameters{};
    case ActiveFilterType::LowShelf:
        return ActiveFilterLowShelfParameters{};
    case ActiveFilterType::HighShelf:
        return ActiveFilterHighShelfParameters{};
    case ActiveFilterType::AllPass:
        return ActiveFilterAllPassParameters{};
    case ActiveFilterType::Gain:
        return ActiveFilterGainParameters{};
    case ActiveFilterType::Delay:
        return ActiveFilterDelayParameters{};
    case ActiveFilterType::Polarity:
        return ActiveFilterPolarityParameters{};
    }

    return ActiveFilterLowPassParameters{};
}


bool parametersEquivalent(const ActiveFilterParameters& first,
                          const ActiveFilterParameters& second)
{
    if (first.index() != second.index()) {
        return false;
    }

    return std::visit([](const auto& firstParameters, const auto& secondParameters) {
        using First = std::decay_t<decltype(firstParameters)>;
        using Second = std::decay_t<decltype(secondParameters)>;
        if constexpr (!std::is_same_v<First, Second>) {
            return false;
        } else if constexpr (std::is_same_v<First, ActiveFilterLowPassParameters> ||
                             std::is_same_v<First, ActiveFilterHighPassParameters>) {
            return firstParameters.characteristic == secondParameters.characteristic &&
                   firstParameters.order == secondParameters.order &&
                   firstParameters.frequencyHz == secondParameters.frequencyHz &&
                   (firstParameters.characteristic != ActiveFilterCharacteristic::GenericQ ||
                    firstParameters.q == secondParameters.q);
        } else if constexpr (std::is_same_v<First, ActiveFilterBandPassParameters>) {
            return firstParameters.characteristic == secondParameters.characteristic &&
                   firstParameters.order == secondParameters.order &&
                   firstParameters.lowerFrequencyHz == secondParameters.lowerFrequencyHz &&
                   firstParameters.upperFrequencyHz == secondParameters.upperFrequencyHz &&
                   (firstParameters.characteristic != ActiveFilterCharacteristic::GenericQ ||
                    firstParameters.q == secondParameters.q);
        } else if constexpr (std::is_same_v<First, ActiveFilterNotchParameters>) {
            // Patch 182 implements the canonical full-depth second-order notch.
            // gainDb remains persisted model metadata for possible future finite-depth
            // variants, but it does not affect the current transfer function/cache.
            return firstParameters.centerFrequencyHz == secondParameters.centerFrequencyHz &&
                   firstParameters.q == secondParameters.q;
        } else if constexpr (std::is_same_v<First, ActiveFilterPeakingEqParameters>) {
            return firstParameters.centerFrequencyHz == secondParameters.centerFrequencyHz &&
                   firstParameters.q == secondParameters.q &&
                   firstParameters.gainDb == secondParameters.gainDb;
        } else if constexpr (std::is_same_v<First, ActiveFilterLowShelfParameters> ||
                             std::is_same_v<First, ActiveFilterHighShelfParameters>) {
            return firstParameters.transitionFrequencyHz == secondParameters.transitionFrequencyHz &&
                   firstParameters.q == secondParameters.q &&
                   firstParameters.gainDb == secondParameters.gainDb;
        } else if constexpr (std::is_same_v<First, ActiveFilterAllPassParameters>) {
            return firstParameters.order == secondParameters.order &&
                   firstParameters.frequencyHz == secondParameters.frequencyHz &&
                   (firstParameters.order != 2 || firstParameters.q == secondParameters.q);
        } else if constexpr (std::is_same_v<First, ActiveFilterGainParameters>) {
            return firstParameters.gainDb == secondParameters.gainDb;
        } else if constexpr (std::is_same_v<First, ActiveFilterDelayParameters>) {
            return firstParameters.delayMs == secondParameters.delayMs;
        } else {
            return firstParameters.inverted == secondParameters.inverted;
        }
    }, first, second);
}

}

ActiveFilterSection::ActiveFilterSection(ActiveFilterType type)
    : m_parameters(defaultParameters(type))
{
}

bool ActiveFilterSection::enabled() const
{
    return m_enabled;
}

void ActiveFilterSection::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

ActiveFilterType ActiveFilterSection::type() const
{
    return std::visit([](const auto& parameters) -> ActiveFilterType {
        using Parameters = std::decay_t<decltype(parameters)>;
        if constexpr (std::is_same_v<Parameters, ActiveFilterLowPassParameters>) {
            return ActiveFilterType::LowPass;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterHighPassParameters>) {
            return ActiveFilterType::HighPass;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterBandPassParameters>) {
            return ActiveFilterType::BandPass;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterNotchParameters>) {
            return ActiveFilterType::Notch;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterPeakingEqParameters>) {
            return ActiveFilterType::PeakingEq;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterLowShelfParameters>) {
            return ActiveFilterType::LowShelf;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterHighShelfParameters>) {
            return ActiveFilterType::HighShelf;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterAllPassParameters>) {
            return ActiveFilterType::AllPass;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterGainParameters>) {
            return ActiveFilterType::Gain;
        } else if constexpr (std::is_same_v<Parameters, ActiveFilterDelayParameters>) {
            return ActiveFilterType::Delay;
        } else {
            return ActiveFilterType::Polarity;
        }
    }, m_parameters);
}

void ActiveFilterSection::setType(ActiveFilterType newType)
{
    if (type() == newType) {
        return;
    }
    m_parameters = defaultParameters(newType);
}

ActiveFilterParameters& ActiveFilterSection::parameters()
{
    return m_parameters;
}

const ActiveFilterParameters& ActiveFilterSection::parameters() const
{
    return m_parameters;
}

bool ActiveFilterSection::transferEquivalent(const ActiveFilterSection& other) const
{
    return m_enabled == other.m_enabled && parametersEquivalent(m_parameters, other.m_parameters);
}

bool ActiveFilterChain::enabled() const
{
    return m_enabled;
}

void ActiveFilterChain::setEnabled(bool enabled)
{
    m_enabled = enabled;
}

bool ActiveFilterChain::showResponseInPlot() const
{
    return m_showResponseInPlot;
}

void ActiveFilterChain::setShowResponseInPlot(bool show)
{
    m_showResponseInPlot = show;
}

std::size_t ActiveFilterChain::sectionCount() const
{
    return m_sections.size();
}

bool ActiveFilterChain::empty() const
{
    return m_sections.empty();
}

ActiveFilterSection& ActiveFilterChain::section(std::size_t index)
{
    return m_sections.at(index);
}

const ActiveFilterSection& ActiveFilterChain::section(std::size_t index) const
{
    return m_sections.at(index);
}

std::size_t ActiveFilterChain::addSection(ActiveFilterType type)
{
    m_sections.emplace_back(type);
    return m_sections.size() - 1;
}

bool ActiveFilterChain::removeSection(std::size_t index)
{
    if (index >= m_sections.size()) {
        return false;
    }

    m_sections.erase(m_sections.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool ActiveFilterChain::moveSection(std::size_t fromIndex, std::size_t toIndex)
{
    if (fromIndex >= m_sections.size() || toIndex >= m_sections.size()) {
        return false;
    }
    if (fromIndex == toIndex) {
        return true;
    }

    ActiveFilterSection moved = m_sections.at(fromIndex);
    m_sections.erase(m_sections.begin() + static_cast<std::ptrdiff_t>(fromIndex));
    m_sections.insert(m_sections.begin() + static_cast<std::ptrdiff_t>(toIndex), moved);
    return true;
}

void ActiveFilterChain::clearSections()
{
    m_sections.clear();
}


bool ActiveFilterChain::transferEquivalent(const ActiveFilterChain& other) const
{
    if (m_enabled != other.m_enabled || m_sections.size() != other.m_sections.size()) {
        return false;
    }

    for (std::size_t index = 0; index < m_sections.size(); ++index) {
        if (!m_sections[index].transferEquivalent(other.m_sections[index])) {
            return false;
        }
    }
    return true;
}
