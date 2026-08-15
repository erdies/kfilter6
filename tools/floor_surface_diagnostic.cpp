/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "floorsurfacemodel.h"
#include "kfilterfrequencygrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

namespace
{
constexpr double Pi = 3.141592653589793238462643383279502884;
constexpr double ReferenceThicknessM = 0.010;
constexpr double ReferenceFlowResistivity = 100000.0;

struct Band
{
    double minHz;
    double maxHz;
    const char* label;
};

constexpr std::array<Band, 4> Bands{{
    {20.0, 200.0, "20-200 Hz"},
    {200.0, 500.0, "200-500 Hz"},
    {500.0, 2000.0, "500-2000 Hz"},
    {2000.0, 20000.0, "2000-20000 Hz"}
}};

double dbMagnitude(const std::complex<double>& value)
{
    const double magnitude = std::abs(value);
    if (magnitude <= 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    return 20.0 * std::log10(magnitude);
}

double phaseDegrees(const std::complex<double>& value)
{
    return std::arg(value) * 180.0 / Pi;
}

struct DenseMinimum
{
    double frequencyHz = 0.0;
    double levelDb = std::numeric_limits<double>::infinity();
};

DenseMinimum findDenseMinimum(const FloorReflectionPathGeometry& path,
                              const FloorSurfaceDefinition& surface,
                              double minHz,
                              double maxHz,
                              std::size_t points)
{
    DenseMinimum result;
    if (points < 2) {
        return result;
    }

    for (std::size_t i = 0; i < points; ++i) {
        const double fraction = static_cast<double>(i) / static_cast<double>(points - 1);
        const double frequency = minHz + (maxHz - minHz) * fraction;
        const FloorSurfaceSample material = calculateFloorSurfaceSample(
            surface, frequency, path.incidenceCosine);
        if (material.status != FloorSurfaceSampleStatus::Valid) {
            continue;
        }
        const std::complex<double> response = calculateFloorReflectionSample(
            path, frequency, material.reflectionCoefficient);
        const double level = dbMagnitude(response);
        if (level < result.levelDb) {
            result.levelDb = level;
            result.frequencyHz = frequency;
        }
    }
    return result;
}

bool parseDouble(const char* text, double& value)
{
    char* end = nullptr;
    value = std::strtod(text, &end);
    return end != text && end != nullptr && *end == '\0' && std::isfinite(value);
}

void usage(const char* executable)
{
    std::cerr
        << "Usage:\n"
        << "  " << executable << " --summary h_source h_listener distance [thickness_mm sigma]\n"
        << "  " << executable << " --curve   h_source h_listener distance [thickness_mm sigma]\n"
        << "  " << executable << " --material frequency_hz incidence_deg [thickness_mm sigma]\n\n"
        << "Default diagnostic porous surface:\n"
        << "  Miki rigid-backed layer, thickness=10 mm, sigma=100000 Pa*s/m^2\n"
        << "  (same parameter pair used in Miki 1990 Fig. 3; it is a model\n"
        << "   reference case, not a claim for one universal carpet material).\n";
}

FloorSurfaceDefinition referencePorousSurface()
{
    return mikiPorousRigidBackingDefinition(
        ReferenceThicknessM, ReferenceFlowResistivity);
}

int printMaterial(double frequencyHz,
                  double incidenceDeg,
                  double thicknessM,
                  double sigma)
{
    if (frequencyHz <= 0.0 || incidenceDeg < 0.0 || incidenceDeg > 90.0 ||
        thicknessM <= 0.0 || sigma <= 0.0) {
        return 2;
    }

    const double cosine = std::cos(incidenceDeg * Pi / 180.0);
    const FloorSurfaceSample sample = calculateFloorSurfaceSample(
        mikiPorousRigidBackingDefinition(thicknessM, sigma),
        frequencyHz,
        cosine);
    if (sample.status != FloorSurfaceSampleStatus::Valid) {
        return 3;
    }

    std::cout << std::fixed << std::setprecision(12)
              << "frequency_hz,incidence_deg,thickness_mm,flow_resistivity_pa_s_per_m2,"
                 "ratio_f_over_sigma,legacy_low_ratio_flag,zc_norm_real,zc_norm_imag,"
                 "gamma_per_m_real,gamma_per_m_imag,zs_norm_real,zs_norm_imag,"
                 "reflection_real,reflection_imag,reflection_magnitude,reflection_phase_deg,"
                 "absorption_coefficient,passivity_warning\n"
              << frequencyHz << ','
              << incidenceDeg << ','
              << thicknessM * 1000.0 << ','
              << sigma << ','
              << sample.frequencyToFlowResistivityRatio << ','
              << (sample.belowLegacyValidatedRatio ? 1 : 0) << ','
              << sample.normalizedCharacteristicImpedance.real() << ','
              << sample.normalizedCharacteristicImpedance.imag() << ','
              << sample.propagationConstantPerM.real() << ','
              << sample.propagationConstantPerM.imag() << ','
              << sample.normalizedSurfaceImpedance.real() << ','
              << sample.normalizedSurfaceImpedance.imag() << ','
              << sample.reflectionCoefficient.real() << ','
              << sample.reflectionCoefficient.imag() << ','
              << std::abs(sample.reflectionCoefficient) << ','
              << phaseDegrees(sample.reflectionCoefficient) << ','
              << sample.absorptionCoefficient << ','
              << (sample.passivityWarning ? 1 : 0) << '\n';
    return 0;
}

int printCurve(const FloorReflectionGeometry& geometry,
               const FloorSurfaceDefinition& porous)
{
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid) {
        return 3;
    }

    const FloorReflectionResponse rigid = calculateIdealRigidFloorReflectionResponse(geometry);
    const FloorReflectionResponse damped = calculateFloorReflectionResponseWithSurfaceModel(
        geometry, porous);
    if (rigid.status != FloorReflectionResponseStatus::Valid ||
        damped.status != FloorReflectionResponseStatus::Valid) {
        return 3;
    }

    std::cout << std::fixed << std::setprecision(12)
              << "index,frequency_hz,incidence_deg,gamma_magnitude,gamma_phase_deg,"
                 "absorption_coefficient,legacy_low_ratio_flag,passivity_warning,rigid_floor_db,"
                 "rigid_floor_phase_deg,porous_floor_db,porous_floor_phase_deg,"
                 "porous_minus_rigid_db\n";

    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
        const FloorSurfaceSample material = calculateFloorSurfaceSample(
            porous, frequencies[i], path.incidenceCosine);
        std::cout << i << ','
                  << frequencies[i] << ','
                  << path.incidenceAngleRad * 180.0 / Pi << ','
                  << std::abs(material.reflectionCoefficient) << ','
                  << phaseDegrees(material.reflectionCoefficient) << ','
                  << material.absorptionCoefficient << ','
                  << (material.belowLegacyValidatedRatio ? 1 : 0) << ','
                  << (material.passivityWarning ? 1 : 0) << ','
                  << dbMagnitude(rigid.values[i]) << ','
                  << phaseDegrees(rigid.values[i]) << ','
                  << dbMagnitude(damped.values[i]) << ','
                  << phaseDegrees(damped.values[i]) << ','
                  << dbMagnitude(damped.values[i]) - dbMagnitude(rigid.values[i])
                  << '\n';
    }
    return 0;
}

int printSummary(const FloorReflectionGeometry& geometry,
                 const FloorSurfaceDefinition& porous)
{
    const FloorReflectionPathGeometry path = calculateFloorReflectionPathGeometry(geometry);
    if (!path.valid) {
        return 3;
    }

    const FloorReflectionResponse rigid = calculateIdealRigidFloorReflectionResponse(geometry);
    const FloorReflectionResponse damped = calculateFloorReflectionResponseWithSurfaceModel(
        geometry, porous);
    if (rigid.status != FloorReflectionResponseStatus::Valid ||
        damped.status != FloorReflectionResponseStatus::Valid) {
        return 3;
    }

    std::cout << std::fixed << std::setprecision(9);
    std::cout << "model,miki_porous_rigid_backed\n"
              << "thickness_mm," << porous.thicknessM * 1000.0 << '\n'
              << "flow_resistivity_pa_s_per_m2," << porous.flowResistivityPaSPerM2 << '\n'
              << "source_height_m," << geometry.sourceHeightM << '\n'
              << "listener_height_m," << geometry.listenerHeightM << '\n'
              << "distance_m," << geometry.horizontalDistanceM << '\n'
              << "delta_r_m," << path.pathDifferenceM << '\n'
              << "incidence_deg," << path.incidenceAngleRad * 180.0 / Pi << '\n'
              << "path_ratio," << path.directDistanceM / path.imageDistanceM << "\n\n";

    const DenseMinimum rigidMinimum = findDenseMinimum(
        path, rigidFloorSurfaceDefinition(), 100.0, 600.0, 20001);
    const DenseMinimum porousMinimum = findDenseMinimum(
        path, porous, 100.0, 600.0, 20001);
    std::cout << "dense_search_min_hz,100.000000000\n"
              << "dense_search_max_hz,600.000000000\n"
              << "rigid_first_minimum_hz," << rigidMinimum.frequencyHz << '\n'
              << "rigid_first_minimum_db," << rigidMinimum.levelDb << '\n'
              << "porous_first_minimum_hz," << porousMinimum.frequencyHz << '\n'
              << "porous_first_minimum_db," << porousMinimum.levelDb << "\n\n";

    std::cout << "frequency_hz,gamma_magnitude,gamma_phase_deg,absorption_coefficient,"
                 "legacy_low_ratio_flag,passivity_warning,rigid_floor_db,porous_floor_db,delta_db\n";
    for (double target : {100.0, 316.732705849, 500.0, 1000.0, 2000.0, 5000.0, 10000.0}) {
        const FloorSurfaceSample material = calculateFloorSurfaceSample(
            porous, target, path.incidenceCosine);
        const std::complex<double> rigidSample = calculateFloorReflectionSample(
            path, target, {1.0, 0.0});
        const std::complex<double> porousSample = calculateFloorReflectionSample(
            path, target, material.reflectionCoefficient);
        std::cout << target << ','
                  << std::abs(material.reflectionCoefficient) << ','
                  << phaseDegrees(material.reflectionCoefficient) << ','
                  << material.absorptionCoefficient << ','
                  << (material.belowLegacyValidatedRatio ? 1 : 0) << ','
                  << (material.passivityWarning ? 1 : 0) << ','
                  << dbMagnitude(rigidSample) << ','
                  << dbMagnitude(porousSample) << ','
                  << dbMagnitude(porousSample) - dbMagnitude(rigidSample) << '\n';
    }

    std::cout << "\nband,rigid_min_db,rigid_max_db,rigid_span_db,porous_min_db,porous_max_db,porous_span_db\n";
    const KFilterFrequencyGrid& frequencies = kfilterFrequencyGridHz();
    for (const Band& band : Bands) {
        double rigidMin = std::numeric_limits<double>::infinity();
        double rigidMax = -std::numeric_limits<double>::infinity();
        double porousMin = std::numeric_limits<double>::infinity();
        double porousMax = -std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < KFilterFrequencyCount; ++i) {
            if (frequencies[i] < band.minHz || frequencies[i] > band.maxHz) {
                continue;
            }
            const double rigidDb = dbMagnitude(rigid.values[i]);
            const double porousDb = dbMagnitude(damped.values[i]);
            rigidMin = std::min(rigidMin, rigidDb);
            rigidMax = std::max(rigidMax, rigidDb);
            porousMin = std::min(porousMin, porousDb);
            porousMax = std::max(porousMax, porousDb);
        }
        std::cout << band.label << ','
                  << rigidMin << ',' << rigidMax << ',' << rigidMax - rigidMin << ','
                  << porousMin << ',' << porousMax << ',' << porousMax - porousMin << '\n';
    }

    return 0;
}
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }

    const std::string mode = argv[1];
    if (mode == "--material") {
        if (argc != 4 && argc != 6) {
            usage(argv[0]);
            return 2;
        }
        double frequency = 0.0;
        double incidence = 0.0;
        if (!parseDouble(argv[2], frequency) || !parseDouble(argv[3], incidence)) {
            return 2;
        }
        double thicknessM = ReferenceThicknessM;
        double sigma = ReferenceFlowResistivity;
        if (argc == 6) {
            double thicknessMm = 0.0;
            if (!parseDouble(argv[4], thicknessMm) || !parseDouble(argv[5], sigma)) {
                return 2;
            }
            thicknessM = thicknessMm / 1000.0;
        }
        return printMaterial(frequency, incidence, thicknessM, sigma);
    }

    if ((mode != "--summary" && mode != "--curve") || (argc != 5 && argc != 7)) {
        usage(argv[0]);
        return 2;
    }

    FloorReflectionGeometry geometry;
    if (!parseDouble(argv[2], geometry.sourceHeightM) ||
        !parseDouble(argv[3], geometry.listenerHeightM) ||
        !parseDouble(argv[4], geometry.horizontalDistanceM)) {
        return 2;
    }

    FloorSurfaceDefinition porous = referencePorousSurface();
    if (argc == 7) {
        double thicknessMm = 0.0;
        double sigma = 0.0;
        if (!parseDouble(argv[5], thicknessMm) ||
            !parseDouble(argv[6], sigma) ||
            thicknessMm <= 0.0 || sigma <= 0.0) {
            return 2;
        }
        porous = mikiPorousRigidBackingDefinition(thicknessMm / 1000.0, sigma);
    }

    if (mode == "--summary") {
        return printSummary(geometry, porous);
    }
    return printCurve(geometry, porous);
}
