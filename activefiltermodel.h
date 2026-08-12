/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef ACTIVEFILTERMODEL_H
#define ACTIVEFILTERMODEL_H

#include <cstddef>
#include <variant>
#include <vector>

enum class ActiveFilterType
{
    LowPass = 0,
    HighPass,
    BandPass,
    Notch,
    AllPass,
    Gain,
    Delay,
    Polarity,
    PeakingEq,
    LowShelf,
    HighShelf
};

enum class ActiveFilterCharacteristic
{
    Butterworth = 0,
    Bessel,
    LinkwitzRiley,
    GenericQ
};

struct ActiveFilterLowPassParameters
{
    ActiveFilterCharacteristic characteristic = ActiveFilterCharacteristic::Butterworth;
    int order = 2;
    double frequencyHz = 2000.0;
    double q = 0.707;
};

struct ActiveFilterHighPassParameters
{
    ActiveFilterCharacteristic characteristic = ActiveFilterCharacteristic::Butterworth;
    int order = 2;
    double frequencyHz = 2000.0;
    double q = 0.707;
};

struct ActiveFilterBandPassParameters
{
    ActiveFilterCharacteristic characteristic = ActiveFilterCharacteristic::Butterworth;
    int order = 2;
    double lowerFrequencyHz = 2000.0;
    double upperFrequencyHz = 4000.0;
    double q = 0.707;
};

struct ActiveFilterNotchParameters
{
    double centerFrequencyHz = 2000.0;
    double q = 0.707;
    double gainDb = 0.0;
};

struct ActiveFilterPeakingEqParameters
{
    double centerFrequencyHz = 2000.0;
    double q = 0.707;
    double gainDb = 0.0;
};

struct ActiveFilterLowShelfParameters
{
    double transitionFrequencyHz = 2000.0;
    double q = 0.707;
    double gainDb = 0.0;
};

struct ActiveFilterHighShelfParameters
{
    double transitionFrequencyHz = 2000.0;
    double q = 0.707;
    double gainDb = 0.0;
};

struct ActiveFilterAllPassParameters
{
    int order = 2;
    double frequencyHz = 2000.0;
    double q = 0.707;
};

struct ActiveFilterGainParameters
{
    double gainDb = 0.0;
};

struct ActiveFilterDelayParameters
{
    double delayMs = 0.0;
};

struct ActiveFilterPolarityParameters
{
    bool inverted = false;
};

using ActiveFilterParameters = std::variant<ActiveFilterLowPassParameters,
                                            ActiveFilterHighPassParameters,
                                            ActiveFilterBandPassParameters,
                                            ActiveFilterNotchParameters,
                                            ActiveFilterPeakingEqParameters,
                                            ActiveFilterLowShelfParameters,
                                            ActiveFilterHighShelfParameters,
                                            ActiveFilterAllPassParameters,
                                            ActiveFilterGainParameters,
                                            ActiveFilterDelayParameters,
                                            ActiveFilterPolarityParameters>;

class ActiveFilterSection
{
public:
    explicit ActiveFilterSection(ActiveFilterType type = ActiveFilterType::LowPass);

    bool enabled() const;
    void setEnabled(bool enabled);

    ActiveFilterType type() const;
    void setType(ActiveFilterType type);

    ActiveFilterParameters& parameters();
    const ActiveFilterParameters& parameters() const;
    bool transferEquivalent(const ActiveFilterSection& other) const;

private:
    bool m_enabled = true;
    ActiveFilterParameters m_parameters;
};

class ActiveFilterChain
{
public:
    bool enabled() const;
    void setEnabled(bool enabled);

    bool showResponseInPlot() const;
    void setShowResponseInPlot(bool show);

    std::size_t sectionCount() const;
    bool empty() const;

    ActiveFilterSection& section(std::size_t index);
    const ActiveFilterSection& section(std::size_t index) const;

    std::size_t addSection(ActiveFilterType type = ActiveFilterType::LowPass);
    bool removeSection(std::size_t index);
    bool moveSection(std::size_t fromIndex, std::size_t toIndex);
    void clearSections();
    bool transferEquivalent(const ActiveFilterChain& other) const;

private:
    bool m_enabled = false;
    bool m_showResponseInPlot = false;
    std::vector<ActiveFilterSection> m_sections;
};

#endif // ACTIVEFILTERMODEL_H
