/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "baffleresponse.h"
#include "floorreflectionresponse.h"

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

namespace
{
constexpr double ReferenceWidthMm = 231.0;
constexpr double ReferenceHeightMm = 965.0;
constexpr double ReferenceDriverXMm = ReferenceWidthMm / 2.0;
constexpr std::size_t ReferenceEdgeSourceCount = 200;
constexpr double ReferenceEffectiveDiameterCm = 13.81976597885342;
constexpr double RadToDeg = 180.0 / 3.141592653589793238462643383279502884;
constexpr double SixDb = 20.0 * std::log10(2.0);

struct CombinedDiagnostic
{
    BaffleSettings freeSettings;
    BaffleSettings rigidSettings;
    BaffleResponse freeBaffle;
    BaffleResponse rigidDiffraction;
    BaffleRectangularRigidFloorDiagnostic rigidDiagnostic;
    FloorReflectionResponse floorReflection;
    std::array<std::complex<double>, KFilterFrequencyCount> combined{};
    std::array<std::complex<double>, KFilterFrequencyCount> doubleCounted{};
    double sourceHeightM = 0.0;
    bool valid = false;
};

bool parseDouble(const char* text, double& value)
{
    if (text == nullptr || *text == '\0') {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    value = std::strtod(text, &end);
    return errno == 0 && end != text && *end == '\0' && std::isfinite(value);
}

double magnitudeDb(const std::complex<double>& value)
{
    const double magnitude = std::abs(value);
    if (!(magnitude > 0.0) || !std::isfinite(magnitude)) {
        return -std::numeric_limits<double>::infinity();
    }
    return 20.0 * std::log10(magnitude);
}

double phaseDeg(const std::complex<double>& value)
{
    return std::arg(value) * RadToDeg;
}

double maxComplexDifference(const BaffleResponse& a, const BaffleResponse& b)
{
    double maximum = 0.0;
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        maximum = std::max(maximum, std::abs(a.values[i] - b.values[i]));
    }
    return maximum;
}

BaffleSettings makeReferenceBaffle(double sourceHeightM,
                                   BaffleBoundaryCondition boundaryCondition)
{
    BaffleSettings settings;
    settings.enabled = true;
    settings.model = BaffleModel::RectangularEdgeDiffraction;
    settings.widthMm = ReferenceWidthMm;
    settings.heightMm = ReferenceHeightMm;
    settings.driverXmm = ReferenceDriverXMm;
    settings.driverYmm = ReferenceHeightMm - sourceHeightM * 1000.0;
    settings.boundaryCondition = boundaryCondition;
    settings.edgeSourceCount = ReferenceEdgeSourceCount;
    settings.leftEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
    settings.rightEdgeTreatment = BaffleSideEdgeTreatment::Sharp;
    return settings;
}

CombinedDiagnostic calculateCombined(double sourceHeightM,
                                     double listenerHeightM,
                                     double distanceM,
                                     double effectiveDiameterCm)
{
    CombinedDiagnostic result;
    result.sourceHeightM = sourceHeightM;

    if (!std::isfinite(sourceHeightM) || sourceHeightM <= 0.0 ||
        sourceHeightM >= ReferenceHeightMm / 1000.0 ||
        !std::isfinite(listenerHeightM) || listenerHeightM < 0.0 ||
        !std::isfinite(distanceM) || distanceM < 0.0 ||
        !std::isfinite(effectiveDiameterCm) || effectiveDiameterCm < 0.0) {
        return result;
    }

    result.freeSettings = makeReferenceBaffle(sourceHeightM,
                                              BaffleBoundaryCondition::FreeField);
    result.rigidSettings = makeReferenceBaffle(
        sourceHeightM, BaffleBoundaryCondition::RigidFloorContactDiffractionOnly);

    result.freeBaffle = calculateBaffleResponse(result.freeSettings, effectiveDiameterCm);
    result.rigidDiffraction = calculateBaffleResponse(result.rigidSettings, effectiveDiameterCm);
    result.rigidDiagnostic = calculateBaffleRectangularRigidFloorDiagnostic(
        result.freeSettings, effectiveDiameterCm);
    result.floorReflection = calculateIdealRigidFloorReflectionResponse(
        {sourceHeightM, listenerHeightM, distanceM});

    if (result.freeBaffle.status != BaffleResponseStatus::Valid ||
        result.rigidDiffraction.status != BaffleResponseStatus::Valid ||
        result.rigidDiagnostic.imageGeometryNormalized.status != BaffleResponseStatus::Valid ||
        result.rigidDiagnostic.imageGeometryRaw.status != BaffleResponseStatus::Valid ||
        result.floorReflection.status != FloorReflectionResponseStatus::Valid) {
        return result;
    }

    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        result.combined[i] = result.rigidDiffraction.values[i] *
                             result.floorReflection.values[i];
        result.doubleCounted[i] = result.rigidDiagnostic.imageGeometryRaw.values[i] *
                                  result.floorReflection.values[i];
    }

    result.valid = true;
    return result;
}

void printSummaryHeader()
{
    std::cout
        << "source_height_m,listener_height_m,distance_m,driver_y_from_top_mm,"
        << "delta_r_m,incidence_deg,path_ratio,first_notch_hz,"
        << "free_baffle_20_db,rigid_diffraction_20_db,rigid_minus_free_20_db,"
        << "floor_20_db,combined_20_db,combined_minus_rigid_20_db,"
        << "raw_double_counted_20_db,raw_extra_gain_20_db,"
        << "max_normalized_vs_production_complex_error,max_raw_extra_gain_error_db\n";
}

void printSummary(const CombinedDiagnostic& result,
                  double listenerHeightM,
                  double distanceM)
{
    if (!result.valid) {
        std::cerr << "invalid Stage-F2 reference geometry\n";
        return;
    }

    const FloorReflectionPathGeometry& path = result.floorReflection.geometry;
    const double firstNotchHz = path.pathDifferenceM > 0.0
        ? KFilterFloorReflectionSpeedOfSoundMPerS / (2.0 * path.pathDifferenceM)
        : std::numeric_limits<double>::infinity();

    double maximumRawGainErrorDb = 0.0;
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        const double normalizedMagnitude = std::abs(result.combined[i]);
        const double rawMagnitude = std::abs(result.doubleCounted[i]);
        if (normalizedMagnitude > 0.0 && rawMagnitude > 0.0) {
            const double rawExtraDb = 20.0 * std::log10(rawMagnitude / normalizedMagnitude);
            maximumRawGainErrorDb = std::max(maximumRawGainErrorDb,
                                             std::abs(rawExtraDb - SixDb));
        }
    }

    const std::size_t i = 0;
    const double free20 = magnitudeDb(result.freeBaffle.values[i]);
    const double rigid20 = magnitudeDb(result.rigidDiffraction.values[i]);
    const double floor20 = magnitudeDb(result.floorReflection.values[i]);
    const double combined20 = magnitudeDb(result.combined[i]);
    const double raw20 = magnitudeDb(result.doubleCounted[i]);

    printSummaryHeader();
    std::cout << std::fixed << std::setprecision(12)
              << result.sourceHeightM << ','
              << listenerHeightM << ','
              << distanceM << ','
              << result.rigidSettings.driverYmm << ','
              << path.pathDifferenceM << ','
              << path.incidenceAngleRad * RadToDeg << ','
              << path.directDistanceM / path.imageDistanceM << ','
              << firstNotchHz << ','
              << free20 << ','
              << rigid20 << ','
              << rigid20 - free20 << ','
              << floor20 << ','
              << combined20 << ','
              << combined20 - rigid20 << ','
              << raw20 << ','
              << raw20 - combined20 << ','
              << maxComplexDifference(result.rigidDiffraction,
                                      result.rigidDiagnostic.imageGeometryNormalized) << ','
              << maximumRawGainErrorDb << '\n';
}

void printCurve(const CombinedDiagnostic& result)
{
    if (!result.valid) {
        std::cerr << "invalid Stage-F2 reference geometry\n";
        return;
    }

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    std::cout
        << "index,frequency_hz,free_baffle_db,free_baffle_phase_deg,"
        << "rigid_diffraction_db,rigid_diffraction_phase_deg,"
        << "floor_reflection_db,floor_reflection_phase_deg,"
        << "combined_db,combined_phase_deg,combined_minus_free_db,"
        << "double_counted_db,double_counted_minus_combined_db\n";

    std::cout << std::fixed << std::setprecision(12);
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        const double freeDb = magnitudeDb(result.freeBaffle.values[i]);
        const double combinedDb = magnitudeDb(result.combined[i]);
        const double doubleDb = magnitudeDb(result.doubleCounted[i]);
        std::cout << i << ','
                  << frequencies[i] << ','
                  << freeDb << ','
                  << phaseDeg(result.freeBaffle.values[i]) << ','
                  << magnitudeDb(result.rigidDiffraction.values[i]) << ','
                  << phaseDeg(result.rigidDiffraction.values[i]) << ','
                  << magnitudeDb(result.floorReflection.values[i]) << ','
                  << phaseDeg(result.floorReflection.values[i]) << ','
                  << combinedDb << ','
                  << phaseDeg(result.combined[i]) << ','
                  << combinedDb - freeDb << ','
                  << doubleDb << ','
                  << doubleDb - combinedDb << '\n';
    }
}

void printBuiltInSweep()
{
    constexpr std::array<double, 4> SourceHeightsM{0.10, 0.30, 0.50, 0.72};
    constexpr std::array<double, 2> ListenerHeightsM{1.00, 1.20};
    constexpr std::array<double, 3> DistancesM{1.50, 2.50, 4.00};

    printSummaryHeader();
    for (double sourceHeightM : SourceHeightsM) {
        for (double listenerHeightM : ListenerHeightsM) {
            for (double distanceM : DistancesM) {
                const CombinedDiagnostic result = calculateCombined(
                    sourceHeightM, listenerHeightM, distanceM,
                    ReferenceEffectiveDiameterCm);
                if (!result.valid) {
                    std::cerr << "built-in Stage-F2 sweep failed\n";
                    return;
                }

                const FloorReflectionPathGeometry& path = result.floorReflection.geometry;
                const double firstNotchHz = path.pathDifferenceM > 0.0
                    ? KFilterFloorReflectionSpeedOfSoundMPerS /
                          (2.0 * path.pathDifferenceM)
                    : std::numeric_limits<double>::infinity();

                double maximumRawGainErrorDb = 0.0;
                for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
                    const double normal = std::abs(result.combined[i]);
                    const double raw = std::abs(result.doubleCounted[i]);
                    if (normal > 0.0 && raw > 0.0) {
                        maximumRawGainErrorDb = std::max(
                            maximumRawGainErrorDb,
                            std::abs(20.0 * std::log10(raw / normal) - SixDb));
                    }
                }

                const double free20 = magnitudeDb(result.freeBaffle.values.front());
                const double rigid20 = magnitudeDb(result.rigidDiffraction.values.front());
                const double floor20 = magnitudeDb(result.floorReflection.values.front());
                const double combined20 = magnitudeDb(result.combined.front());
                const double raw20 = magnitudeDb(result.doubleCounted.front());

                std::cout << std::fixed << std::setprecision(12)
                          << sourceHeightM << ','
                          << listenerHeightM << ','
                          << distanceM << ','
                          << result.rigidSettings.driverYmm << ','
                          << path.pathDifferenceM << ','
                          << path.incidenceAngleRad * RadToDeg << ','
                          << path.directDistanceM / path.imageDistanceM << ','
                          << firstNotchHz << ','
                          << free20 << ','
                          << rigid20 << ','
                          << rigid20 - free20 << ','
                          << floor20 << ','
                          << combined20 << ','
                          << combined20 - rigid20 << ','
                          << raw20 << ','
                          << raw20 - combined20 << ','
                          << maxComplexDifference(result.rigidDiffraction,
                                                  result.rigidDiagnostic.imageGeometryNormalized)
                          << ',' << maximumRawGainErrorDb << '\n';
            }
        }
    }
}

void printUsage(const char* executable)
{
    std::cerr
        << "KFilter Patch 224 Floor Reflection Stage-F2 combination diagnostic\n\n"
        << "Reference baffle: 231 x 965 mm, X=115.5 mm, Sharp edges, N=200.\n"
        << "source_height_m is converted to Driver Y from top by:\n"
        << "  driver_y_mm = 965 - source_height_m * 1000\n\n"
        << "Usage:\n"
        << "  " << executable << "\n"
        << "      Built-in 24-geometry summary sweep.\n\n"
        << "  " << executable
        << " --summary <source_height_m> <listener_height_m> <distance_m> [Dm_cm]\n"
        << "      One Stage-F2 summary row. Default Dm="
        << ReferenceEffectiveDiameterCm << " cm.\n\n"
        << "  " << executable
        << " --curve <source_height_m> <listener_height_m> <distance_m> [Dm_cm]\n"
        << "      150-point CSV for free baffle, rigid diffraction-only, floor reflection,\n"
        << "      normalized combination and deliberately double-counted reference.\n";
}

bool parseGeometryArguments(int argc,
                            char** argv,
                            double& sourceHeightM,
                            double& listenerHeightM,
                            double& distanceM,
                            double& effectiveDiameterCm)
{
    if (argc != 5 && argc != 6) {
        return false;
    }
    if (!parseDouble(argv[2], sourceHeightM) ||
        !parseDouble(argv[3], listenerHeightM) ||
        !parseDouble(argv[4], distanceM)) {
        return false;
    }
    effectiveDiameterCm = ReferenceEffectiveDiameterCm;
    if (argc == 6 && !parseDouble(argv[5], effectiveDiameterCm)) {
        return false;
    }
    return true;
}
}

int main(int argc, char** argv)
{
    if (argc == 1) {
        printBuiltInSweep();
        return 0;
    }

    const std::string mode = argv[1];
    if (mode != "--summary" && mode != "--curve") {
        printUsage(argv[0]);
        return 2;
    }

    double sourceHeightM = 0.0;
    double listenerHeightM = 0.0;
    double distanceM = 0.0;
    double effectiveDiameterCm = 0.0;
    if (!parseGeometryArguments(argc, argv, sourceHeightM, listenerHeightM,
                                distanceM, effectiveDiameterCm)) {
        printUsage(argv[0]);
        return 2;
    }

    const CombinedDiagnostic result = calculateCombined(
        sourceHeightM, listenerHeightM, distanceM, effectiveDiameterCm);
    if (!result.valid) {
        std::cerr << "invalid Stage-F2 reference geometry or response\n";
        return 1;
    }

    if (mode == "--summary") {
        printSummary(result, listenerHeightM, distanceM);
    } else {
        printCurve(result);
    }
    return 0;
}
