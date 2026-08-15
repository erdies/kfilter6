/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffle_lf_hybrid_diagnostic_model.h"
#include "kfilterfrequencygrid.h"

#include <array>
#include <cmath>
#include <complex>
#include <iomanip>
#include <iostream>
#include <limits>

namespace
{
struct GeometryCase
{
    const char *name;
    double widthMm;
    double heightMm;
    double driverXmm;
    double driverYmm;
    double effectiveDriverDiameterCm;
};

constexpr std::array<double, 10> ReportFrequenciesHz{
    50.0, 80.0, 100.0, 150.0, 200.0, 300.0, 500.0, 700.0, 1000.0, 2000.0
};

// Keep all Patch-244 geometries, including the controlled same-width height
// pairs. Patch 245 changes only the width-anchored transition family so the
// geometry set stays fixed for direct comparison.
constexpr std::array<GeometryCase, 10> GeometryCases{{
    {"ZRT", 231.0, 965.0, 115.5, 228.6, 13.81976597885342},
    {"compact_180x280", 180.0, 280.0, 90.0, 120.0, 10.0},
    {"bookshelf_200x350", 200.0, 350.0, 100.0, 145.0, 12.0},
    {"floorstander_220x900", 220.0, 900.0, 110.0, 210.0, 14.0},
    {"square_300x300", 300.0, 300.0, 150.0, 150.0, 15.0},
    {"wide_400x300", 400.0, 300.0, 200.0, 150.0, 15.0},
    {"offset_250x500", 250.0, 500.0, 78.0, 165.0, 12.0},
    {"slender_180x1000", 180.0, 1000.0, 90.0, 220.0, 10.0},
    {"height_pair_180x1000_y120", 180.0, 1000.0, 90.0, 120.0, 10.0},
    {"height_pair_220x300_y210", 220.0, 300.0, 110.0, 210.0, 14.0},
}};

double magnitudeDb(const std::complex<double>& value)
{
    return 20.0 * std::log10(std::abs(value));
}

std::size_t nearestSampleIndex(double targetFrequencyHz)
{
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    std::size_t bestIndex = 0;
    double bestError = std::numeric_limits<double>::infinity();
    for (std::size_t index = 0; index < frequencies.size(); ++index) {
        const double error = std::abs(frequencies[index] - targetFrequencyHz);
        if (error < bestError) {
            bestError = error;
            bestIndex = index;
        }
    }
    return bestIndex;
}

BaffleSettings settingsFor(const GeometryCase& geometry)
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = geometry.widthMm;
    settings.heightMm = geometry.heightMm;
    settings.driverXmm = geometry.driverXmm;
    settings.driverYmm = geometry.driverYmm;
    settings.edgeSourceCount = 200;
    return settings;
}

bool printCase(const GeometryCase& geometry)
{
    const BaffleLfHybridDiagnostic diagnostic =
        calculateBaffleLfHybridDiagnostic(settingsFor(geometry),
                                           geometry.effectiveDriverDiameterCm);
    if (!diagnostic.valid) {
        std::cerr << "diagnostic failed for geometry: " << geometry.name << '\n';
        return false;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    std::cout << "# geometry=" << geometry.name
              << " width_mm=" << geometry.widthMm
              << " height_mm=" << geometry.heightMm
              << " driver_x_mm=" << geometry.driverXmm
              << " driver_y_mm=" << geometry.driverYmm
              << " diameter_cm=" << geometry.effectiveDriverDiameterCm
              << " Le_m=" << diagnostic.effectiveLengthM
              << " simple_midpoint_hz=" << diagnostic.simpleMidpointFrequencyHz << '\n';
    std::cout << "target_hz,grid_hz,sqrt_wh_weight,width_n1_weight,width_n15_weight,"
                 "width_n2_weight,simple_db,raw_rectangular_db,sqrt_wh_hybrid_db,"
                 "width_n1_db,width_n15_db,width_n2_db,raw_rectangular_phase_deg,"
                 "sqrt_wh_phase_deg,width_n1_phase_deg,width_n15_phase_deg,width_n2_phase_deg\n";

    constexpr double RadToDeg = 180.0 / 3.141592653589793238462643383279502884;
    for (double targetHz : ReportFrequenciesHz) {
        const std::size_t index = nearestSampleIndex(targetHz);
        std::cout << targetHz << ','
                  << frequencies[index] << ','
                  << diagnostic.blendWeight[index] << ','
                  << diagnostic.widthAnchoredBlendWeight[index] << ','
                  << diagnostic.widthAnchoredBlendWeightN15[index] << ','
                  << diagnostic.widthAnchoredBlendWeightN2[index] << ','
                  << magnitudeDb(diagnostic.simple.values[index]) << ','
                  << magnitudeDb(diagnostic.rectangular.values[index]) << ','
                  << magnitudeDb(diagnostic.hybrid.values[index]) << ','
                  << magnitudeDb(diagnostic.widthAnchoredHybrid.values[index]) << ','
                  << magnitudeDb(diagnostic.widthAnchoredHybridN15.values[index]) << ','
                  << magnitudeDb(diagnostic.widthAnchoredHybridN2.values[index]) << ','
                  << std::arg(diagnostic.rectangular.values[index]) * RadToDeg << ','
                  << std::arg(diagnostic.hybrid.values[index]) * RadToDeg << ','
                  << std::arg(diagnostic.widthAnchoredHybrid.values[index]) * RadToDeg << ','
                  << std::arg(diagnostic.widthAnchoredHybridN15.values[index]) * RadToDeg << ','
                  << std::arg(diagnostic.widthAnchoredHybridN2.values[index]) * RadToDeg << '\n';
    }
    std::cout << '\n';
    return true;
}
}

int main()
{
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "# KFilter6 Patch 246 LF hybrid production/reference comparison\n"
                 "# Productive Free-field Rectangular Edge Diffraction now uses width n=2.\n"
                 "# raw_rectangular_* remains the unblended Sharp/finite-piston reference.\n"
                 "# Patch-242 reference: w=x/(x+pi), x=2*pi*f*sqrt(W*H)/c.\n"
                 "# Width family: r=f/fBS, wN=r^n/(1+r^n), fBS=115/W[m].\n"
                 "# n=1/n=1.5 remain comparisons; width_n2_db is the promoted product law.\n"
                 "# All hybrids blend dB magnitude only and preserve raw rectangular phase.\n\n";

    for (const GeometryCase& geometry : GeometryCases) {
        if (!printCase(geometry)) {
            return 1;
        }
    }

    return 0;
}
