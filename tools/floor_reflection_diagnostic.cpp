/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorreflectionresponse.h"
#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double RadToDeg = 180.0 / Pi;
constexpr std::size_t DefaultDensePointCount = 8192;
constexpr std::size_t MinimumDensePointCount = 100;
constexpr std::size_t MaximumDensePointCount = 1000000;

struct NotchGridComparison
{
    std::size_t ordinal = 0;
    double exactFrequencyHz = 0.0;
    double exactMagnitudeDb = 0.0;
    std::size_t nearestGridIndex = 0;
    double nearestGridFrequencyHz = 0.0;
    double nearestGridMagnitudeDb = 0.0;
    double depthMissDb = 0.0;
    double frequencyErrorPercent = 0.0;
};

struct GeometrySummary
{
    FloorReflectionGeometry input;
    FloorReflectionPathGeometry path;
    double pathRatio = 0.0;
    double exactNotchMagnitudeDb = 0.0;
    std::vector<NotchGridComparison> notches;
};

bool parseDouble(const char* text, double& value)
{
    if (!text || *text == '\0') {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

bool parseSize(const char* text, std::size_t& value)
{
    if (!text || *text == '\0' || *text == '-') {
        return false;
    }

    errno = 0;
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' ||
        parsed < MinimumDensePointCount || parsed > MaximumDensePointCount) {
        return false;
    }

    value = static_cast<std::size_t>(parsed);
    return true;
}

bool parseGeometry(int argc, char** argv, int firstIndex, FloorReflectionGeometry& geometry)
{
    if (argc <= firstIndex + 2) {
        return false;
    }

    return parseDouble(argv[firstIndex], geometry.sourceHeightM) &&
           parseDouble(argv[firstIndex + 1], geometry.listenerHeightM) &&
           parseDouble(argv[firstIndex + 2], geometry.horizontalDistanceM);
}

double magnitudeDb(const std::complex<double>& value)
{
    const double magnitude = std::abs(value);
    if (magnitude <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    return 20.0 * std::log10(magnitude);
}

double phaseDeg(const std::complex<double>& value)
{
    return std::arg(value) * RadToDeg;
}

std::size_t nearestGridIndex(double frequencyHz)
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    const auto iterator = std::lower_bound(frequencies.begin(), frequencies.end(), frequencyHz);

    if (iterator == frequencies.begin()) {
        return 0;
    }
    if (iterator == frequencies.end()) {
        return frequencies.size() - 1;
    }

    const std::size_t upperIndex = static_cast<std::size_t>(iterator - frequencies.begin());
    const std::size_t lowerIndex = upperIndex - 1;
    const double lowerError = std::abs(frequencyHz - frequencies[lowerIndex]);
    const double upperError = std::abs(frequencies[upperIndex] - frequencyHz);
    return lowerError <= upperError ? lowerIndex : upperIndex;
}

GeometrySummary summarizeGeometry(const FloorReflectionGeometry& geometry)
{
    GeometrySummary summary;
    summary.input = geometry;
    summary.path = calculateFloorReflectionPathGeometry(geometry);
    if (!summary.path.valid) {
        return summary;
    }

    summary.pathRatio = summary.path.directDistanceM / summary.path.imageDistanceM;
    const double residual = std::max(0.0, 1.0 - summary.pathRatio);
    summary.exactNotchMagnitudeDb = residual > 0.0
        ? 20.0 * std::log10(residual)
        : -std::numeric_limits<double>::infinity();

    if (!(summary.path.pathDifferenceM > 0.0)) {
        return summary;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    const double minimumHz = frequencies.front();
    const double maximumHz = frequencies.back();
    const double fundamentalNotchHz = KFilterFloorReflectionSpeedOfSoundMPerS /
                                      (2.0 * summary.path.pathDifferenceM);

    for (std::size_t ordinal = 0;; ++ordinal) {
        const double multiplier = static_cast<double>(2 * ordinal + 1);
        const double notchHz = multiplier * fundamentalNotchHz;
        if (!std::isfinite(notchHz) || notchHz > maximumHz) {
            break;
        }
        if (notchHz < minimumHz) {
            continue;
        }

        const std::size_t gridIndex = nearestGridIndex(notchHz);
        const double gridHz = frequencies[gridIndex];
        const std::complex<double> gridSample = calculateFloorReflectionSample(
            summary.path, gridHz, {1.0, 0.0});
        const double gridMagnitudeDb = magnitudeDb(gridSample);

        NotchGridComparison comparison;
        comparison.ordinal = ordinal + 1;
        comparison.exactFrequencyHz = notchHz;
        comparison.exactMagnitudeDb = summary.exactNotchMagnitudeDb;
        comparison.nearestGridIndex = gridIndex;
        comparison.nearestGridFrequencyHz = gridHz;
        comparison.nearestGridMagnitudeDb = gridMagnitudeDb;
        comparison.depthMissDb = gridMagnitudeDb - summary.exactNotchMagnitudeDb;
        comparison.frequencyErrorPercent = 100.0 * (gridHz - notchHz) / notchHz;
        summary.notches.push_back(comparison);
    }

    return summary;
}

const NotchGridComparison* worstNotch(const GeometrySummary& summary)
{
    if (summary.notches.empty()) {
        return nullptr;
    }

    return &*std::max_element(summary.notches.begin(), summary.notches.end(),
        [](const NotchGridComparison& left, const NotchGridComparison& right) {
            return left.depthMissDb < right.depthMissDb;
        });
}

void printSummaryHeader()
{
    std::cout
        << "source_height_m,listener_height_m,distance_m,"
        << "r_direct_m,r_image_m,delta_r_m,incidence_deg,path_ratio,"
        << "notch_count,first_notch_hz,exact_notch_db,"
        << "first_grid_hz,first_grid_db,first_depth_miss_db,first_freq_error_pct,"
        << "worst_notch_ordinal,worst_notch_hz,worst_grid_hz,worst_grid_db,"
        << "worst_depth_miss_db,worst_freq_error_pct\n";
}

void printSummaryRow(const GeometrySummary& summary)
{
    std::cout << std::fixed << std::setprecision(9)
              << summary.input.sourceHeightM << ','
              << summary.input.listenerHeightM << ','
              << summary.input.horizontalDistanceM << ',';

    if (!summary.path.valid) {
        std::cout << "INVALID\n";
        return;
    }

    std::cout << summary.path.directDistanceM << ','
              << summary.path.imageDistanceM << ','
              << summary.path.pathDifferenceM << ','
              << summary.path.incidenceAngleRad * RadToDeg << ','
              << summary.pathRatio << ','
              << summary.notches.size() << ',';

    if (summary.notches.empty()) {
        std::cout << "NA," << summary.exactNotchMagnitudeDb
                  << ",NA,NA,NA,NA,NA,NA,NA,NA,NA,NA\n";
        return;
    }

    const NotchGridComparison& first = summary.notches.front();
    const NotchGridComparison* worst = worstNotch(summary);
    std::cout << first.exactFrequencyHz << ','
              << first.exactMagnitudeDb << ','
              << first.nearestGridFrequencyHz << ','
              << first.nearestGridMagnitudeDb << ','
              << first.depthMissDb << ','
              << first.frequencyErrorPercent << ','
              << worst->ordinal << ','
              << worst->exactFrequencyHz << ','
              << worst->nearestGridFrequencyHz << ','
              << worst->nearestGridMagnitudeDb << ','
              << worst->depthMissDb << ','
              << worst->frequencyErrorPercent << '\n';
}

void printNotchTable(const GeometrySummary& summary)
{
    if (!summary.path.valid) {
        std::cerr << "invalid floor-reflection geometry\n";
        return;
    }

    std::cout << "notch_ordinal,exact_frequency_hz,exact_magnitude_db,"
                 "nearest_grid_index,nearest_grid_frequency_hz,nearest_grid_magnitude_db,"
                 "depth_miss_db,frequency_error_pct\n";
    std::cout << std::fixed << std::setprecision(9);
    for (const NotchGridComparison& notch : summary.notches) {
        std::cout << notch.ordinal << ','
                  << notch.exactFrequencyHz << ','
                  << notch.exactMagnitudeDb << ','
                  << notch.nearestGridIndex << ','
                  << notch.nearestGridFrequencyHz << ','
                  << notch.nearestGridMagnitudeDb << ','
                  << notch.depthMissDb << ','
                  << notch.frequencyErrorPercent << '\n';
    }
}

void printGridCurve(const FloorReflectionGeometry& geometry)
{
    const FloorReflectionResponse response = calculateIdealRigidFloorReflectionResponse(geometry);
    if (response.status != FloorReflectionResponseStatus::Valid) {
        std::cerr << "invalid floor-reflection geometry\n";
        return;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    std::cout << "index,frequency_hz,magnitude_db,phase_deg,real,imag\n";
    std::cout << std::fixed << std::setprecision(12);
    for (std::size_t index = 0; index < KFilterFrequencyCount; ++index) {
        const std::complex<double>& value = response.values[index];
        std::cout << index << ','
                  << frequencies[index] << ','
                  << magnitudeDb(value) << ','
                  << phaseDeg(value) << ','
                  << value.real() << ','
                  << value.imag() << '\n';
    }
}

void printDenseCurve(const FloorReflectionGeometry& geometry, std::size_t pointCount)
{
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid) {
        std::cerr << "invalid floor-reflection geometry\n";
        return;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    const double minimumHz = frequencies.front();
    const double maximumHz = frequencies.back();
    const double ratio = std::pow(maximumHz / minimumHz,
                                  1.0 / static_cast<double>(pointCount - 1));

    std::cout << "index,frequency_hz,magnitude_db,phase_deg,real,imag\n";
    std::cout << std::fixed << std::setprecision(12);
    double frequencyHz = minimumHz;
    for (std::size_t index = 0; index < pointCount; ++index) {
        if (index + 1 == pointCount) {
            frequencyHz = maximumHz;
        }
        const std::complex<double> value = calculateFloorReflectionSample(
            path, frequencyHz, {1.0, 0.0});
        std::cout << index << ','
                  << frequencyHz << ','
                  << magnitudeDb(value) << ','
                  << phaseDeg(value) << ','
                  << value.real() << ','
                  << value.imag() << '\n';
        frequencyHz *= ratio;
    }
}

void printBuiltInSweep()
{
    constexpr std::array<double, 5> SourceHeightsM{0.20, 0.40, 0.72, 1.00, 1.20};
    constexpr std::array<double, 3> ListenerHeightsM{0.90, 1.05, 1.20};
    constexpr std::array<double, 4> DistancesM{1.00, 2.50, 4.00, 6.00};

    printSummaryHeader();
    for (double sourceHeightM : SourceHeightsM) {
        for (double listenerHeightM : ListenerHeightsM) {
            for (double distanceM : DistancesM) {
                printSummaryRow(summarizeGeometry(
                    {sourceHeightM, listenerHeightM, distanceM}));
            }
        }
    }
}

void printUsage(const char* executable)
{
    std::cerr
        << "KFilter Patch 223 floor-reflection Stage-F1 diagnostic\n\n"
        << "Usage:\n"
        << "  " << executable << "\n"
        << "      Built-in geometry sweep; summary CSV to stdout.\n\n"
        << "  " << executable << " --summary <source_m> <listener_m> <distance_m>\n"
        << "      One geometry summary CSV row.\n\n"
        << "  " << executable << " --notches <source_m> <listener_m> <distance_m>\n"
        << "      Exact rigid-floor notch positions versus nearest 150-point grid samples.\n\n"
        << "  " << executable << " --curve <source_m> <listener_m> <distance_m>\n"
        << "      Full 150-point KFilter response CSV: magnitude, phase and complex value.\n\n"
        << "  " << executable << " --dense <source_m> <listener_m> <distance_m> [points]\n"
        << "      Dense logarithmic reference CSV over the KFilter frequency range.\n"
        << "      Default points: " << DefaultDensePointCount << ".\n\n"
        << "All output is plain CSV and can be redirected to a file.\n";
}
}

int main(int argc, char** argv)
{
    if (argc == 1) {
        printBuiltInSweep();
        return 0;
    }

    const std::string mode = argv[1];
    if (mode == "--help" || mode == "-h") {
        printUsage(argv[0]);
        return 0;
    }

    FloorReflectionGeometry geometry;
    if (!parseGeometry(argc, argv, 2, geometry)) {
        printUsage(argv[0]);
        return 2;
    }

    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid) {
        std::cerr << "invalid floor-reflection geometry\n";
        return 3;
    }

    if (mode == "--summary") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 2;
        }
        printSummaryHeader();
        printSummaryRow(summarizeGeometry(geometry));
        return 0;
    }

    if (mode == "--notches") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 2;
        }
        printNotchTable(summarizeGeometry(geometry));
        return 0;
    }

    if (mode == "--curve") {
        if (argc != 5) {
            printUsage(argv[0]);
            return 2;
        }
        printGridCurve(geometry);
        return 0;
    }

    if (mode == "--dense") {
        if (argc != 5 && argc != 6) {
            printUsage(argv[0]);
            return 2;
        }
        std::size_t pointCount = DefaultDensePointCount;
        if (argc == 6 && !parseSize(argv[5], pointCount)) {
            std::cerr << "dense point count must be between "
                      << MinimumDensePointCount << " and " << MaximumDensePointCount << '\n';
            return 2;
        }
        printDenseCurve(geometry, pointCount);
        return 0;
    }

    printUsage(argv[0]);
    return 2;
}
