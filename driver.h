/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef DRIVER_H
#define DRIVER_H

/**
*the base class which provides mathematical routines for the KFilter App.
*@author Martin Erdtmann
*/

#include <QString>
#include "kfilterfrequencygrid.h"
#include <array>
#include <complex>

enum class EnclosureType : int {
	OpenBaffle = 0,
	Sealed = 1,
	Vented = 2,
	Bandpass = 3
};

enum class NetworkBranchType : int {
	Series = 0,
	Shunt = 1
};

enum class NetworkComponent : int {
	Resistance = 0,
	Capacitance = 1,
	Inductance = 2
};

struct DriverPlotState {
	bool pressure = false;
	bool impedance = false;
	bool vectorSummary = false;
	bool scalarSummary = false;
	bool impedanceSummary = false;

	bool anyEnabled() const
	{
		return pressure || impedance || vectorSummary || scalarSummary || impedanceSummary;
	}
};

static_assert(static_cast<int>(EnclosureType::OpenBaffle) == 0);
static_assert(static_cast<int>(EnclosureType::Sealed) == 1);
static_assert(static_cast<int>(EnclosureType::Vented) == 2);
static_assert(static_cast<int>(EnclosureType::Bandpass) == 3);

class driver {
public:
	using ResponseArray = std::array<std::complex<double>, KFilterFrequencyCount>;

	driver();
	~driver();


	/** Restores the complete historical default driver state. */
	void resetToDefaults(void);

	void calculatePressureResponse(void);
	void calculateImpedanceResponse (void);

	/** Returns the calculated complex pressure response as read-only data. */
	const ResponseArray& pressureResponse() const;
	/** Returns the calculated complex impedance response as read-only data. */
	const ResponseArray& impedanceResponse() const;
	/** Returns per-driver plot/summary visibility state as read-only data. */
	const DriverPlotState& plotState() const;
	/** Replaces per-driver plot/summary visibility state without invalidating response caches. */
	void setPlotState(const DriverPlotState& state);

	QString	getTitle() const;
	void	setTitle( const QString& a_qstringTitle );
  /** Sets Rdc value */
  void setRdc(double rdc);
  /** Sets the total driver Q (Qts). */
  void setQts(double qts);
  /** No descriptions */
  void setF0(double f0);
  /** Sets Lsp value */
  void setLsp(double lsp);
  /** No descriptions */
  void setDm(double dm);
  /** Sets the primary enclosure volume in litres. */
  void setVb(double vb);
  /** Sets the enclosure tuning frequency in hertz. */
  void setFb(double fb);
  /** No descriptions */
  void setQl(double ql);
  /** Sets the secondary enclosure volume in litres. */
  void setV2(double v2);
  /** Sets the linear pressure-response gain. */
  void setGainLinear(double gain);
  /** Sets whether the driver pressure response is phase-inverted (polarity reversed). */
  void setPhaseInverted(bool inverted);
  /** Sets the requested enclosure alignment used by calculateParameters(). */
  void setEnclosureTypeProposal(EnclosureType type);
  /** No descriptions */
  void setVas(double vas);
  /** No descriptions */
  void setQms(double qms);
  /** Sets the electrical driver Q (Qes). */
  void setQes(double qes);
  /** Sets the full circuit flag */
  void setFullCircuit(bool toggle);
  /** Gets the full circuit flag */
  bool getFullCircuit(void) const;
  /** No descriptions */
  double getDm() const;
  /** Gets the primary enclosure volume in litres. */
  double getVb() const;
  /** Gets the enclosure tuning frequency in hertz. */
  double getFb() const;
  /** No descriptions */
  double getQl() const;
  /** Gets the secondary enclosure volume in litres. */
  double getV2() const;
  /** Gets the linear pressure-response gain. */
  double getGainLinear() const;
  /** Returns whether the driver pressure response is phase-inverted. */
  bool isPhaseInverted() const;
  /** Returns the requested enclosure alignment. */
  EnclosureType getEnclosureTypeProposal() const;
  /** No descriptions */
  double getVas() const;
  /** No descriptions */
  double getQms() const;
  /** Returns the electrical driver Q (Qes). */
  double getQes() const;
  /** Returns the total driver Q (Qts). */
  double getQts() const;
  /** No descriptions */
  double getF0() const;
  /** No descriptions */
  double getLsp() const;
  /** No descriptions */
  double getRdc() const;
  /** Sets one component of a passive-network branch. Section indices are 0-based. */
  void setNetworkValue(int sectionIndex, NetworkBranchType branch, NetworkComponent component, double value);
  /** Reads one component of a passive-network branch. Section indices are 0-based. */
  double getNetworkValue(int sectionIndex, NetworkBranchType branch, NetworkComponent component) const;
private:
	void setModified(void);
	void calculateParameters(void);

	struct NetworkBranch {
		double resistance = 0.0;
		double capacitance = 0.0;
		double inductance = 0.0;
	};

	struct NetworkSection {
		NetworkBranch series;
		NetworkBranch parallel;
	};

	static constexpr std::size_t NetworkSectionCount = 8;
	// Historical full-circuit calibration gain. Numerically this is approximately
	// +22 dB, but the original absolute-reference provenance is no longer known.
	static constexpr double LegacyFullCircuitCalibrationGain = 12.58925412;
	std::array<NetworkSection, NetworkSectionCount> network;
	ResponseArray resultPressure;
	ResponseArray resultImpedance;
	DriverPlotState plotState_;

	NetworkBranch* networkBranch(int sectionIndex, NetworkBranchType branch);
	const NetworkBranch* networkBranch(int sectionIndex, NetworkBranchType branch) const;
	std::complex<double> calculateEquivalentCircuit(double omega);
	void calculateAcousticResponse(std::complex<double>& response, double omega);
	int  findLastNetworkSectionIndex(void) const;
	void calculateParallelBranch(std::complex<double>& networkImpedance, double omega, const NetworkBranch& branch);
	void calculateSeriesBranch(std::complex<double>& networkImpedance, double omega, const NetworkBranch& branch);

	EnclosureType enclosureType, enclosureTypeProposal;
	bool dirty_pressure,dirty_impedance;
	bool pistonLowPassActive,fullCircuitFlag;
	bool phaseInverted;
	// Historical diagnostic switch retained intentionally: legacy KFilter could
	// display only the bass-reflex-port contribution for a vented enclosure.
	// There is currently no UI/API toggle for this mode, but the calculation
	// path remains available for possible future diagnostic use.
	bool show_reflex_only;

	double
		Rdc,Lsp,F0,Qts,Qms,Qes,Dm,Vas,Vb,Fb,Ql,V2,gain,
		//    Anpassung,Ausgleichsfaktor,Versatz,
		ventedDenominatorA0,ventedDenominatorA1,ventedDenominatorA2,ventedDenominatorA3,
		// Historical physical-model approximation of the driver's natural upper
		// roll-off as an ideal piston radiator. This is a 0 dB-normalized model
		// component, not a user Active Filter. The present Qt6 code no longer has
		// a productive write path for pistonLowPassQ/pistonLowPassFrequency, so the path is dormant
		// but retained until its original parameter derivation is reconstructed.
		pistonLowPassInductance,pistonLowPassCapacitance,pistonLowPassQ,pistonLowPassFrequency,
		// Full-circuit ideal-piston radiation-resistance approximation. Keep
		// this model alongside the simplified normalized LowPass* path; the
		// two alternatives are useful for different Driver-analysis tasks.
		radiationCapacitance,fullCircuitNormalizationFactor,
		// Combined motional/enclosure inductive term used only by the sealed
		// full/equivalent-circuit path. Open Baffle uses motionalInductance directly.
		sealedEffectiveMotionalInductance,
		// Sealed-enclosure loss terms derived from Ql. The first is the
		// dimensionless conductance of the normalized simplified high-pass;
		// the second is the reflected full-circuit conductance in siemens.
		sealedHighPassLossConductance,sealedLeakageConductance,
		// Tuned enclosure series-RLC branch used by Vented and Bandpass
		// equivalent-circuit paths.
		enclosureBranchCapacitance,enclosureBranchInductance,enclosureBranchResistance,
		motionalCapacitance,motionalInductance,motionalResistance,
		acousticHighPassCapacitance,acousticHighPassInductance;

	QString	m_qstringTitle;
};

#endif
