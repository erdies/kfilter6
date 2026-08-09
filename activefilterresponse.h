/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef ACTIVEFILTERRESPONSE_H
#define ACTIVEFILTERRESPONSE_H

#include "activefiltermodel.h"
#include "kfilterfrequencygrid.h"

#include <array>
#include <complex>
#include <cstddef>
#include <cstdint>

// The current transfer engine deliberately supports only analog Butterworth low-pass/high-pass
// sections. Other section types remain representable by the data model but are
// reported explicitly instead of being silently treated as neutral filters.
enum class ActiveFilterResponseStatus
{
    Neutral = 0,
    Valid,
    Unsupported,
    InvalidParameters
};

struct ActiveFilterResponse
{
    std::array<std::complex<double>, KFilterFrequencyCount> values{};
    ActiveFilterResponseStatus status = ActiveFilterResponseStatus::Neutral;
    bool hasActiveSections = false;
    std::size_t problemSectionIndex = 0;

    bool plottable() const
    {
        return status == ActiveFilterResponseStatus::Valid && hasActiveSections;
    }
};

ActiveFilterResponse calculateActiveFilterResponse(const ActiveFilterChain& chain);

// Apply one cached active-filter sample to a complex driver sample. Neutral,
// unsupported and invalid responses deliberately bypass the active-filter
// stage; callers can inspect response.status to surface the reason.
std::complex<double> applyActiveFilterResponseSample(
    const ActiveFilterResponse& response,
    std::size_t sampleIndex,
    const std::complex<double>& signal);

class ActiveFilterResponseCache
{
public:
    const ActiveFilterResponse& responseFor(const ActiveFilterChain& chain);
    std::uint64_t generation() const;

private:
    bool m_valid = false;
    ActiveFilterChain m_cachedChain;
    ActiveFilterResponse m_response;
    std::uint64_t m_generation = 0;
};

#endif // ACTIVEFILTERRESPONSE_H
