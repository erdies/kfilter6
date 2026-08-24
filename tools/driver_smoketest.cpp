/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "driver.h"
#include "networkserializationutils.h"

#include <QTextStream>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <type_traits>
#include <utility>

namespace
{
constexpr std::size_t ResultSampleCount = 150;
using ResultArray = std::array<std::complex<double>, ResultSampleCount>;

static_assert(std::is_same_v<decltype(std::declval<driver&>().pressureResponse()),
                             const driver::ResponseArray&>);
static_assert(std::is_same_v<decltype(std::declval<driver&>().impedanceResponse()),
                             const driver::ResponseArray&>);
static_assert(std::is_same_v<decltype(std::declval<const driver&>().plotState()),
                             const DriverPlotState&>);
static_assert(std::is_same_v<driver::ResponseArray, ResultArray>);

ResultArray copyResult(const ResultArray& values)
{
    return values;
}

bool nearlyEqual(const std::complex<double>& left, const std::complex<double>& right)
{
    if (!std::isfinite(left.real()) || !std::isfinite(left.imag()) ||
        !std::isfinite(right.real()) || !std::isfinite(right.imag())) {
        return false;
    }

    const double scale = std::max({1.0, std::abs(left), std::abs(right)});
    return std::abs(left - right) <= 1.0e-10 * scale;
}

bool resultArraysEqual(const ResultArray& left, const ResultArray& right)
{
    for (std::size_t index = 0; index < ResultSampleCount; ++index) {
        if (!nearlyEqual(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool resultArraysDiffer(const ResultArray& left, const ResultArray& right)
{
    return !resultArraysEqual(left, right);
}

void calculateResults(driver& drv)
{
    drv.calculatePressureResponse();
    drv.calculateImpedanceResponse();
}

void configureDefault(driver&)
{
}

void configureClosed(driver& drv)
{
    drv.setVb(20.0);
    drv.setFb(0.0);
    drv.setQl(10.0);
    drv.setV2(0.0);
    drv.setEnclosureTypeProposal(EnclosureType::Sealed);
}

void configureClosedFullCircuit(driver& drv)
{
    configureClosed(drv);
    drv.setFullCircuit(true);
}

void configureVented(driver& drv)
{
    drv.setVb(20.0);
    drv.setFb(40.0);
    drv.setV2(0.0);
    drv.setEnclosureTypeProposal(EnclosureType::Vented);
    drv.setQl(7.0);
}

void configureBandpass(driver& drv)
{
    drv.setVb(20.0);
    drv.setFb(40.0);
    drv.setV2(8.0);
    drv.setEnclosureTypeProposal(EnclosureType::Bandpass);
    drv.setQl(7.0);
}

void configureFullCircuit(driver& drv)
{
    drv.setFullCircuit(true);
}

template<typename Configure, typename Mutate>
bool checkSetterInvalidation(const char* label, Configure configure, Mutate mutate)
{
    driver cachedDriver;
    configure(cachedDriver);
    calculateResults(cachedDriver);
    const ResultArray baselineSound = copyResult(cachedDriver.pressureResponse());
    const ResultArray baselineImpedance = copyResult(cachedDriver.impedanceResponse());

    mutate(cachedDriver);
    calculateResults(cachedDriver);

    driver freshDriver;
    configure(freshDriver);
    mutate(freshDriver);
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.pressureResponse()), cachedDriver.pressureResponse()) ||
        !resultArraysEqual(copyResult(freshDriver.impedanceResponse()), cachedDriver.impedanceResponse())) {
        QTextStream(stderr) << label << " left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(baselineSound, cachedDriver.pressureResponse()) &&
        !resultArraysDiffer(baselineImpedance, cachedDriver.impedanceResponse())) {
        QTextStream(stderr) << label << " did not affect either calculated result; test setup is ineffective\n";
        return false;
    }

    return true;
}

bool checkAllSetterInvalidations()
{
    return
        checkSetterInvalidation("setRdc", configureDefault,
                                [](driver& drv) { drv.setRdc(6.3); }) &&
        checkSetterInvalidation("setLsp", configureDefault,
                                [](driver& drv) { drv.setLsp(0.00031); }) &&
        checkSetterInvalidation("setF0", configureDefault,
                                [](driver& drv) { drv.setF0(180.0); }) &&
        checkSetterInvalidation("setQts", configureDefault,
                                [](driver& drv) { drv.setQts(0.72); }) &&
        checkSetterInvalidation("setQes", configureDefault,
                                [](driver& drv) { drv.setQes(0.63); }) &&
        checkSetterInvalidation("setQms", configureDefault,
                                [](driver& drv) { drv.setQms(4.2); }) &&
        checkSetterInvalidation("setVas", configureClosed,
                                [](driver& drv) { drv.setVas(35.0); }) &&
        checkSetterInvalidation("setDm", configureFullCircuit,
                                [](driver& drv) { drv.setDm(10.0); }) &&
        checkSetterInvalidation("setVb", configureClosed,
                                [](driver& drv) { drv.setVb(35.0); }) &&
        checkSetterInvalidation("setFb", configureVented,
                                [](driver& drv) { drv.setFb(55.0); }) &&
        checkSetterInvalidation("setQl (Sealed simplified)", configureClosed,
                                [](driver& drv) { drv.setQl(4.5); }) &&
        checkSetterInvalidation("setQl (Sealed full circuit)", configureClosedFullCircuit,
                                [](driver& drv) { drv.setQl(4.5); }) &&
        checkSetterInvalidation("setQl (Vented)", configureVented,
                                [](driver& drv) { drv.setQl(4.5); }) &&
        checkSetterInvalidation("setV2", configureBandpass,
                                [](driver& drv) { drv.setV2(12.0); }) &&
        checkSetterInvalidation("setGainLinear", configureDefault,
                                [](driver& drv) { drv.setGainLinear(1.25); }) &&
        checkSetterInvalidation("setPhaseInverted", configureDefault,
                                [](driver& drv) { drv.setPhaseInverted(true); }) &&
        checkSetterInvalidation("setEnclosureTypeProposal", configureVented,
                                [](driver& drv) { drv.setEnclosureTypeProposal(EnclosureType::Sealed); }) &&
        checkSetterInvalidation("setFullCircuit", configureDefault,
                                [](driver& drv) { drv.setFullCircuit(true); });
}

bool checkSealedQlLossModel()
{
    constexpr double pi = 3.141592654;
    constexpr double rdc = 6.0;
    constexpr double lsp = 0.00025;
    constexpr double f0 = 50.0;
    constexpr double qes = 0.5;
    constexpr double qms = 5.0;
    constexpr double qts = qes*qms/(qes+qms);
    constexpr double vas = 40.0;
    constexpr double vb = 20.0;
    constexpr double ql = 4.0;

    driver sealedDriver;
    sealedDriver.setRdc(rdc);
    sealedDriver.setLsp(lsp);
    sealedDriver.setF0(f0);
    sealedDriver.setQes(qes);
    sealedDriver.setQms(qms);
    sealedDriver.setQts(qts);
    sealedDriver.setVas(vas);
    sealedDriver.setVb(vb);
    sealedDriver.setFb(0.0);
    sealedDriver.setV2(0.0);
    sealedDriver.setQl(ql);
    sealedDriver.setEnclosureTypeProposal(EnclosureType::Sealed);
    calculateResults(sealedDriver);

    const double omega0 = 2*pi*f0;
    const double complianceRatio = 1+vas/vb;
    const double acousticCapacitance = qts/omega0;
    const double acousticInductance = 1/(omega0*qts*complianceRatio);
    const double idealSealedQuality = qts*std::sqrt(complianceRatio);
    const double normalizedLossConductance = 1+idealSealedQuality/ql;

    const double motionalResistance = qms*rdc/qes;
    const double motionalCapacitance = qes/(omega0*rdc);
    const double motionalInductance = rdc/(omega0*qes);
    const double sealedEffectiveMotionalInductance = motionalInductance/complianceRatio;
    const double leakageConductance =
        std::sqrt(motionalCapacitance/sealedEffectiveMotionalInductance)/ql;

    double omega = 125.6637061;
    for (std::size_t sampleIndex = 0; sampleIndex < ResultSampleCount; ++sampleIndex) {
        const std::complex<double> parallelHighPassImpedance =
            1.0/std::complex<double>{normalizedLossConductance,
                                     -1/(omega*acousticInductance)};
        const std::complex<double> highPassImpedance =
            parallelHighPassImpedance+
            std::complex<double>{0.0, -1/(omega*acousticCapacitance)};
        const std::complex<double> expectedPressure =
            parallelHighPassImpedance/highPassImpedance;

        const std::complex<double> motionalAdmittance{
            1/motionalResistance+leakageConductance,
            omega*motionalCapacitance-
                1/(omega*sealedEffectiveMotionalInductance)};
        const std::complex<double> expectedImpedance =
            std::complex<double>{rdc, omega*lsp}+1.0/motionalAdmittance;

        if (!nearlyEqual(sealedDriver.pressureResponse()[sampleIndex], expectedPressure)) {
            QTextStream(stderr) << "Sealed Ql simplified SPL mismatch at sample "
                                << static_cast<unsigned long long>(sampleIndex) << '\n';
            return false;
        }
        if (!nearlyEqual(sealedDriver.impedanceResponse()[sampleIndex], expectedImpedance)) {
            QTextStream(stderr) << "Sealed Ql equivalent-circuit mismatch at sample "
                                << static_cast<unsigned long long>(sampleIndex) << '\n';
            return false;
        }
        omega *= KFilterFrequencyStep;
    }

    driver openLowQl;
    openLowQl.setQl(2.0);
    calculateResults(openLowQl);

    driver openHighQl;
    openHighQl.setQl(500.0);
    calculateResults(openHighQl);

    if (!resultArraysEqual(copyResult(openLowQl.pressureResponse()),
                           openHighQl.pressureResponse()) ||
        !resultArraysEqual(copyResult(openLowQl.impedanceResponse()),
                           openHighQl.impedanceResponse())) {
        QTextStream(stderr) << "Ql unexpectedly affects Open Baffle calculations\n";
        return false;
    }

    return true;
}

bool checkNetworkValueInvalidation()
{
    driver drv;
    drv.setNetworkValue(0, NetworkBranchType::Series, NetworkComponent::Resistance, 10.0);
    calculateResults(drv);
    const ResultArray networkSound = copyResult(drv.pressureResponse());
    const ResultArray networkImpedance = copyResult(drv.impedanceResponse());

    drv.setNetworkValue(0, NetworkBranchType::Series, NetworkComponent::Resistance, 0.0);
    calculateResults(drv);

    driver freshDriver;
    calculateResults(freshDriver);

    if (!resultArraysEqual(copyResult(freshDriver.pressureResponse()), drv.pressureResponse()) ||
        !resultArraysEqual(copyResult(freshDriver.impedanceResponse()), drv.impedanceResponse())) {
        QTextStream(stderr) << "setNetworkValue left stale cached results\n";
        return false;
    }

    if (!resultArraysDiffer(networkSound, drv.pressureResponse()) &&
        !resultArraysDiffer(networkImpedance, drv.impedanceResponse())) {
        QTextStream(stderr) << "setNetworkValue invalidation regression setup is ineffective\n";
        return false;
    }

    return true;
}

bool checkEnclosureTransitionConsistency()
{
    driver transitionedDriver;
    configureVented(transitionedDriver);
    transitionedDriver.calculatePressureResponse();
    if (transitionedDriver.getEnclosureTypeProposal() != EnclosureType::Vented) {
        QTextStream(stderr) << "Vented-box proposal setup failed\n";
        return false;
    }

    transitionedDriver.setVb(0.0);
    transitionedDriver.setFb(0.0);
    transitionedDriver.setV2(0.0);
    transitionedDriver.setEnclosureTypeProposal(EnclosureType::OpenBaffle);
    transitionedDriver.calculatePressureResponse();

    if (transitionedDriver.getEnclosureTypeProposal() != EnclosureType::OpenBaffle) {
        QTextStream(stderr) << "Free-air enclosure proposal was not stored\n";
        return false;
    }

    driver directDriver;
    directDriver.setQl(7.0);
    directDriver.calculatePressureResponse();
    if (!resultArraysEqual(copyResult(directDriver.pressureResponse()), transitionedDriver.pressureResponse())) {
        QTextStream(stderr) << "Identical free-air end states produced different complex SPL results\n";
        return false;
    }

    return true;
}


bool checkEquivalentCircuitEnclosureTransitions()
{
    auto checkTransition = [](const char* label,
                              void (*configureInitialState)(driver&),
                              void (*configureFinalState)(driver&)) {
        driver transitionedDriver;
        configureInitialState(transitionedDriver);
        transitionedDriver.setFullCircuit(true);
        calculateResults(transitionedDriver);

        configureFinalState(transitionedDriver);
        calculateResults(transitionedDriver);

        driver directDriver;
        configureFinalState(directDriver);
        directDriver.setFullCircuit(true);
        calculateResults(directDriver);

        if (!resultArraysEqual(copyResult(directDriver.pressureResponse()),
                               transitionedDriver.pressureResponse()) ||
            !resultArraysEqual(copyResult(directDriver.impedanceResponse()),
                               transitionedDriver.impedanceResponse())) {
            QTextStream(stderr) << label
                                << " left enclosure-equivalent-circuit state from the previous alignment\n";
            return false;
        }
        return true;
    };

    auto configureOpen = [](driver& drv) {
        drv.setVb(0.0);
        drv.setFb(0.0);
        drv.setV2(0.0);
        drv.setEnclosureTypeProposal(EnclosureType::OpenBaffle);
    };

    // Function-pointer form keeps the transition helper simple for the existing
    // named configuration functions; Open Baffle is covered separately below.
    if (!checkTransition("Vented -> Sealed", configureVented, configureClosed) ||
        !checkTransition("Sealed -> Vented", configureClosed, configureVented) ||
        !checkTransition("Bandpass -> Vented", configureBandpass, configureVented) ||
        !checkTransition("Vented -> Bandpass", configureVented, configureBandpass)) {
        return false;
    }

    driver transitionedOpen;
    configureVented(transitionedOpen);
    transitionedOpen.setFullCircuit(true);
    calculateResults(transitionedOpen);
    configureOpen(transitionedOpen);
    calculateResults(transitionedOpen);

    driver directOpen;
    configureOpen(directOpen);
    directOpen.setFullCircuit(true);
    calculateResults(directOpen);
    if (!resultArraysEqual(copyResult(directOpen.pressureResponse()), transitionedOpen.pressureResponse()) ||
        !resultArraysEqual(copyResult(directOpen.impedanceResponse()), transitionedOpen.impedanceResponse())) {
        QTextStream(stderr) << "Vented -> Open Baffle left enclosure-equivalent-circuit state\n";
        return false;
    }

    return true;
}

bool checkZeroF0ImpedanceModel()
{
    auto verifyVoiceCoilOnly = [](driver& drv, const char* label) {
        drv.calculateImpedanceResponse();

        double omega = 125.6637061;
        constexpr double frequencyFactor = 1.047128548;
        for (std::size_t index = 0; index < ResultSampleCount; ++index) {
            const std::complex<double> expected{drv.getRdc(), omega * drv.getLsp()};
            if (!nearlyEqual(drv.impedanceResponse()[index], expected)) {
                QTextStream(stderr)
                    << label << " produced a non-voice-coil impedance at sample "
                    << static_cast<unsigned long long>(index) << '\n';
                return false;
            }
            omega *= frequencyFactor;
        }
        return true;
    };

    driver directZeroDriver;
    directZeroDriver.setF0(0.0);
    if (!verifyVoiceCoilOnly(directZeroDriver, "Direct F0 == 0")) {
        return false;
    }

    driver transitionedToZeroDriver;
    transitionedToZeroDriver.setF0(180.0);
    transitionedToZeroDriver.calculateImpedanceResponse();
    transitionedToZeroDriver.setF0(0.0);
    if (!verifyVoiceCoilOnly(transitionedToZeroDriver, "F0 > 0 -> F0 == 0")) {
        return false;
    }

    if (!resultArraysEqual(copyResult(directZeroDriver.impedanceResponse()),
                           transitionedToZeroDriver.impedanceResponse())) {
        QTextStream(stderr) << "Direct and transitioned F0 == 0 impedances differ\n";
        return false;
    }

    transitionedToZeroDriver.setF0(180.0);
    transitionedToZeroDriver.calculateImpedanceResponse();

    driver directValidDriver;
    directValidDriver.setF0(180.0);
    directValidDriver.calculateImpedanceResponse();
    if (!resultArraysEqual(copyResult(directValidDriver.impedanceResponse()),
                           transitionedToZeroDriver.impedanceResponse())) {
        QTextStream(stderr) << "F0 == 0 -> F0 > 0 did not restore the TS impedance model\n";
        return false;
    }

    return true;
}


using LegacyNetwork = std::array<double, 49>;

int legacyFindLastNetworkSectionOffset(const LegacyNetwork& units)
{
    for (int index = 48; index >= 0; --index) {
        if (units[static_cast<std::size_t>(index)] != 0.0) {
            if (index % 6 == 0) {
                return index;
            }
            return (index / 6) * 6 + 6;
        }
    }
    return 0;
}

void legacyCalculateParallelBranch(std::complex<double>& networkImpedance,
                                   double omega,
                                   int sectionOffset,
                                   const LegacyNetwork& units)
{
    std::complex<double> branchImpedance;
    const double resistance = units[static_cast<std::size_t>(sectionOffset + 3)];
    if (units[static_cast<std::size_t>(sectionOffset + 4)] == 0.0) {
        if (units[static_cast<std::size_t>(sectionOffset + 5)] == 0.0) {
            if (units[static_cast<std::size_t>(sectionOffset + 3)] == 0.0) {
                return;
            }
            branchImpedance = {resistance, 0.0};
        } else {
            branchImpedance = {resistance,
                               omega * units[static_cast<std::size_t>(sectionOffset + 5)]};
        }
    } else {
        if (units[static_cast<std::size_t>(sectionOffset + 5)] == 0.0) {
            branchImpedance = {
                resistance,
                -1 / (omega * units[static_cast<std::size_t>(sectionOffset + 4)])};
        } else {
            branchImpedance = {
                resistance,
                omega * units[static_cast<std::size_t>(sectionOffset + 5)] -
                    1 / (omega * units[static_cast<std::size_t>(sectionOffset + 4)])};
        }
    }
    networkImpedance = 1.0 / (1.0 / networkImpedance + 1.0 / branchImpedance);
}

void legacyCalculateSeriesBranch(std::complex<double>& networkImpedance,
                                 double omega,
                                 int sectionOffset,
                                 const LegacyNetwork& units)
{
    if (units[static_cast<std::size_t>(sectionOffset + 1)] == 0.0) {
        if (units[static_cast<std::size_t>(sectionOffset + 2)] == 0.0) {
            networkImpedance += std::complex<double>{
                units[static_cast<std::size_t>(sectionOffset)], 0.0};
        } else {
            networkImpedance += std::complex<double>{
                units[static_cast<std::size_t>(sectionOffset)],
                omega * units[static_cast<std::size_t>(sectionOffset + 2)]};
        }
    } else {
        double susceptance;
        if (units[static_cast<std::size_t>(sectionOffset + 2)] == 0.0) {
            susceptance = omega * units[static_cast<std::size_t>(sectionOffset + 1)];
        } else {
            susceptance = omega * units[static_cast<std::size_t>(sectionOffset + 1)] -
                          1 / (omega * units[static_cast<std::size_t>(sectionOffset + 2)]);
        }
        if (units[static_cast<std::size_t>(sectionOffset)] == 0.0) {
            networkImpedance += std::complex<double>{0.0, -1 / susceptance};
        } else {
            const std::complex<double> branchAdmittance{
                1 / units[static_cast<std::size_t>(sectionOffset)], susceptance};
            networkImpedance += 1.0 / branchAdmittance;
        }
    }
}

struct LegacyNetworkResult
{
    ResultArray pressure{};
    ResultArray impedance{};
};

LegacyNetworkResult calculateLegacyNetworkReference(const LegacyNetwork& units,
                                                     double rdc,
                                                     double lsp)
{
    LegacyNetworkResult result;
    double omega = 125.6637061;
    for (std::size_t sampleIndex = 0; sampleIndex < ResultSampleCount; ++sampleIndex) {
        int sectionOffset = legacyFindLastNetworkSectionOffset(units) + 1;
        std::complex<double> response{1.0, 0.0};
        std::complex<double> networkImpedance{rdc, omega * lsp};
        while (sectionOffset > 1) {
            sectionOffset -= 6;
            legacyCalculateParallelBranch(networkImpedance, omega, sectionOffset, units);
            const std::complex<double> terminationImpedance = networkImpedance;
            legacyCalculateSeriesBranch(networkImpedance, omega, sectionOffset, units);
            response *= terminationImpedance / networkImpedance;
        }
        result.pressure[sampleIndex] = response;
        result.impedance[sampleIndex] = networkImpedance;
        omega *= 1.047128548;
    }
    return result;
}

bool exactComplexEqual(const std::complex<double>& left, const std::complex<double>& right)
{
    return left.real() == right.real() && left.imag() == right.imag();
}

bool checkLegacyNetworkCase(const char* label, const LegacyNetwork& units)
{
    driver drv;
    drv.setF0(0.0); // isolates the passive network from the acoustic T/S model
    for (int unitIndex = 1; unitIndex <= 48; ++unitIndex) {
        const int zeroBasedUnit = unitIndex - 1;
        const int row = zeroBasedUnit % 6;
        const NetworkBranchType branch = row < 3 ? NetworkBranchType::Series : NetworkBranchType::Shunt;
        const NetworkComponent component = row % 3 == 0 ? NetworkComponent::Resistance
                                           : row % 3 == 1 ? NetworkComponent::Capacitance
                                                          : NetworkComponent::Inductance;
        drv.setNetworkValue(zeroBasedUnit / 6,
                            branch,
                            component,
                            units[static_cast<std::size_t>(unitIndex)]);
    }
    calculateResults(drv);

    const LegacyNetworkResult expected =
        calculateLegacyNetworkReference(units, drv.getRdc(), drv.getLsp());

    for (std::size_t sampleIndex = 0; sampleIndex < ResultSampleCount; ++sampleIndex) {
        if (!exactComplexEqual(drv.pressureResponse()[sampleIndex], expected.pressure[sampleIndex])) {
            QTextStream(stderr) << label << " SPL bitwise regression mismatch at sample "
                                << static_cast<unsigned long long>(sampleIndex) << '\n';
            return false;
        }
        if (!exactComplexEqual(drv.impedanceResponse()[sampleIndex], expected.impedance[sampleIndex])) {
            QTextStream(stderr) << label << " impedance bitwise regression mismatch at sample "
                                << static_cast<unsigned long long>(sampleIndex) << '\n';
            return false;
        }
    }
    return true;
}

LegacyNetwork networkWithTriplet(int section,
                                 bool parallel,
                                 double resistance,
                                 double capacitance,
                                 double inductance)
{
    LegacyNetwork units{};
    const int base = section * 6 + (parallel ? 4 : 1);
    units[static_cast<std::size_t>(base)] = resistance;
    units[static_cast<std::size_t>(base + 1)] = capacitance;
    units[static_cast<std::size_t>(base + 2)] = inductance;
    return units;
}

bool checkSemanticNetworkMapping()
{
    driver drv;
    int valueIndex = 1;
    for (int sectionIndex = 0; sectionIndex < 8; ++sectionIndex) {
        for (NetworkBranchType branch : {NetworkBranchType::Series, NetworkBranchType::Shunt}) {
            for (NetworkComponent component : {NetworkComponent::Resistance,
                                               NetworkComponent::Capacitance,
                                               NetworkComponent::Inductance}) {
                const double expected = valueIndex * 1.25;
                drv.setNetworkValue(sectionIndex, branch, component, expected);
                if (drv.getNetworkValue(sectionIndex, branch, component) != expected) {
                    QTextStream(stderr) << "Semantic network mapping mismatch at value "
                                        << valueIndex << '\n';
                    return false;
                }
                ++valueIndex;
            }
        }
    }

    if (drv.getNetworkValue(-1, NetworkBranchType::Series, NetworkComponent::Resistance) != -1.0 ||
        drv.getNetworkValue(8, NetworkBranchType::Series, NetworkComponent::Resistance) != -1.0) {
        QTextStream(stderr) << "Semantic network API accepted an invalid section index\n";
        return false;
    }

    for (int sectionIndex = 0; sectionIndex < 8; ++sectionIndex) {
        for (NetworkBranchType branch : {NetworkBranchType::Series, NetworkBranchType::Shunt}) {
            for (NetworkComponent component : {NetworkComponent::Resistance,
                                               NetworkComponent::Capacitance,
                                               NetworkComponent::Inductance}) {
                drv.setNetworkValue(sectionIndex, branch, component, 0.0);
                if (drv.getNetworkValue(sectionIndex, branch, component) != 0.0) {
                    QTextStream(stderr) << "Semantic network clear via setNetworkValue failed\n";
                    return false;
                }
            }
        }
    }
    return true;
}

bool checkNetworkSerializationMapping()
{
    driver drv;
    for (int serializedIndex = 0; serializedIndex < NetworkSerializationUtils::ValueCount; ++serializedIndex) {
        NetworkSerializationUtils::setValue(drv, serializedIndex, 1000.0 + serializedIndex);
    }

    int serializedIndex = 0;
    for (int sectionIndex = 0; sectionIndex < 8; ++sectionIndex) {
        for (NetworkBranchType branch : {NetworkBranchType::Series, NetworkBranchType::Shunt}) {
            for (NetworkComponent component : {NetworkComponent::Resistance,
                                               NetworkComponent::Capacitance,
                                               NetworkComponent::Inductance}) {
                const double expected = 1000.0 + serializedIndex;
                if (drv.getNetworkValue(sectionIndex, branch, component) != expected) {
                    QTextStream(stderr) << "Serialized network mapping mismatch at index "
                                        << serializedIndex << '\n';
                    return false;
                }
                const double replacement = 2000.0 + serializedIndex;
                drv.setNetworkValue(sectionIndex, branch, component, replacement);
                if (NetworkSerializationUtils::value(drv, serializedIndex) != replacement) {
                    QTextStream(stderr) << "Serialized network reverse mapping mismatch at index "
                                        << serializedIndex << '\n';
                    return false;
                }
                ++serializedIndex;
            }
        }
    }

    return NetworkSerializationUtils::value(drv, -1) == -1.0 &&
           NetworkSerializationUtils::value(drv, NetworkSerializationUtils::ValueCount) == -1.0;
}

bool checkPassiveNetworkBitwiseRegression()
{
    constexpr double r = 4.7;
    constexpr double c = 8.2e-6;
    constexpr double l = 0.00047;

    const std::array<LegacyNetwork, 14> branchCases = {
        networkWithTriplet(0, false, r, 0.0, 0.0),
        networkWithTriplet(0, false, 0.0, c, 0.0),
        networkWithTriplet(0, false, 0.0, 0.0, l),
        networkWithTriplet(0, false, r, 0.0, l),
        networkWithTriplet(0, false, r, c, 0.0),
        networkWithTriplet(0, false, 0.0, c, l),
        networkWithTriplet(0, false, r, c, l),
        networkWithTriplet(0, true, r, 0.0, 0.0),
        networkWithTriplet(0, true, 0.0, c, 0.0),
        networkWithTriplet(0, true, 0.0, 0.0, l),
        networkWithTriplet(0, true, r, 0.0, l),
        networkWithTriplet(0, true, r, c, 0.0),
        networkWithTriplet(0, true, 0.0, c, l),
        networkWithTriplet(0, true, r, c, l)
    };
    const std::array<const char*, 14> branchLabels = {
        "series R", "series C", "series L", "series R+L", "series R+C", "series L+C", "series R+C+L",
        "shunt R", "shunt C", "shunt L", "shunt R+L", "shunt R+C", "shunt L+C", "shunt R+C+L"
    };
    for (std::size_t index = 0; index < branchCases.size(); ++index) {
        if (!checkLegacyNetworkCase(branchLabels[index], branchCases[index])) {
            return false;
        }
    }

    LegacyNetwork multipleSections{};
    multipleSections[1] = 2.2;
    multipleSections[3] = 0.00068;
    multipleSections[10] = 6.8;
    multipleSections[11] = 12.0e-6;
    multipleSections[18] = 0.0015;
    if (!checkLegacyNetworkCase("multiple consecutive sections", multipleSections)) {
        return false;
    }

    LegacyNetwork sectionGap{};
    sectionGap[1] = 3.3;
    sectionGap[20] = 10.0e-6;
    sectionGap[42] = 0.0010;
    if (!checkLegacyNetworkCase("sections with gaps", sectionGap)) {
        return false;
    }

    LegacyNetwork lastSection{};
    lastSection[48] = 0.00082;
    if (!checkLegacyNetworkCase("last valid section", lastSection)) {
        return false;
    }

    LegacyNetwork lowPass{};
    lowPass[3] = 0.0010;
    lowPass[5] = 10.0e-6;
    if (!checkLegacyNetworkCase("typical low-pass", lowPass)) {
        return false;
    }

    LegacyNetwork highPass{};
    highPass[2] = 15.0e-6;
    highPass[6] = 0.00068;
    if (!checkLegacyNetworkCase("typical high-pass", highPass)) {
        return false;
    }

    LegacyNetwork suckCircuit{};
    suckCircuit[1] = 8.2;
    suckCircuit[2] = 6.8e-6;
    suckCircuit[3] = 0.00082;
    if (!checkLegacyNetworkCase("series suck circuit", suckCircuit)) {
        return false;
    }

    LegacyNetwork trapCircuit{};
    trapCircuit[4] = 6.8;
    trapCircuit[5] = 12.0e-6;
    trapCircuit[6] = 0.00056;
    if (!checkLegacyNetworkCase("shunt trap circuit", trapCircuit)) {
        return false;
    }

    return true;
}

bool checkPlotStateEncapsulation()
{
    driver drv;
    if (drv.plotState().anyEnabled()) {
        QTextStream(stderr) << "Default DriverPlotState is not fully disabled\n";
        return false;
    }

    DriverPlotState state;
    state.pressure = true;
    state.scalarSummary = true;
    drv.setPlotState(state);

    const DriverPlotState& actual = drv.plotState();
    if (!actual.pressure || actual.impedance || actual.vectorSummary ||
        !actual.scalarSummary || actual.impedanceSummary || !actual.anyEnabled()) {
        QTextStream(stderr) << "DriverPlotState round-trip mismatch\n";
        return false;
    }

    drv.resetToDefaults();
    if (drv.plotState().anyEnabled()) {
        QTextStream(stderr) << "resetToDefaults did not reset DriverPlotState\n";
        return false;
    }

    return true;
}


bool checkCopyKeepsConsistentCacheState()
{
    driver source;
    configureVented(source);
    source.setGainLinear(1.15);
    source.setNetworkValue(0, NetworkBranchType::Series, NetworkComponent::Inductance, 0.00047);
    calculateResults(source);

    const ResultArray expectedPressure = copyResult(source.pressureResponse());
    const ResultArray expectedImpedance = copyResult(source.impedanceResponse());

    driver copyConstructed(source);
    calculateResults(copyConstructed);
    if (!resultArraysEqual(expectedPressure, copyConstructed.pressureResponse()) ||
        !resultArraysEqual(expectedImpedance, copyConstructed.impedanceResponse())) {
        QTextStream(stderr) << "Copy construction did not preserve a consistent response/cache state\n";
        return false;
    }

    driver assigned;
    assigned = source;
    calculateResults(assigned);
    if (!resultArraysEqual(expectedPressure, assigned.pressureResponse()) ||
        !resultArraysEqual(expectedImpedance, assigned.impedanceResponse())) {
        QTextStream(stderr) << "Copy assignment did not preserve a consistent response/cache state\n";
        return false;
    }

    return true;
}

bool checkCalculationFlagRecovery()
{
    driver recoveredDriver;
    recoveredDriver.setF0(0.0);
    recoveredDriver.calculatePressureResponse();
    for (const std::complex<double>& sample : recoveredDriver.pressureResponse()) {
        if (!nearlyEqual(sample, std::complex<double>{1.0, 0.0})) {
            QTextStream(stderr) << "F0 == 0 did not bypass TS/acoustic parameter processing\n";
            return false;
        }
    }

    recoveredDriver.setF0(180.0);
    recoveredDriver.calculatePressureResponse();

    driver directDriver;
    directDriver.setF0(180.0);
    directDriver.calculatePressureResponse();
    if (!resultArraysEqual(copyResult(directDriver.pressureResponse()), recoveredDriver.pressureResponse())) {
        QTextStream(stderr) << "Recovered and directly valid drivers produced different SPL results\n";
        return false;
    }

    return true;
}
}

int main()
{
    driver d;
    d.setTitle(QStringLiteral("Qt6 driver smoke test"));
    d.setRdc(5.6);
    d.setF0(300.0);
    d.setQl(7.5);
    calculateResults(d);

    if (!checkAllSetterInvalidations() ||
        !checkSealedQlLossModel() ||
        !checkNetworkValueInvalidation() ||
        !checkSemanticNetworkMapping() ||
        !checkNetworkSerializationMapping() ||
        !checkPassiveNetworkBitwiseRegression() ||
        !checkEnclosureTransitionConsistency() ||
        !checkEquivalentCircuitEnclosureTransitions() ||
        !checkZeroF0ImpedanceModel() ||
        !checkPlotStateEncapsulation() ||
        !checkCopyKeepsConsistentCacheState() ||
        !checkCalculationFlagRecovery()) {
        return 1;
    }

    QTextStream out(stdout);
    out << d.getTitle() << '\n';
    out << "Rdc=" << d.getRdc() << '\n';
    out << "Ql=" << d.getQl() << '\n';
    out << "Sound active=" << d.plotState().pressure << '\n';
    out << "Impedance[0]=" << d.impedanceResponse()[0].real()
        << "+j" << d.impedanceResponse()[0].imag() << '\n';
    out << "Driver state regression smoke test passed\n";
    return 0;
}
