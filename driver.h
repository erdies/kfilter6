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
#include <array>
#include <complex>

enum class EnclosureType : int {
	OpenBaffle = 0,
	Sealed = 1,
	Vented = 2,
	Bandpass = 3
};

static_assert(static_cast<int>(EnclosureType::OpenBaffle) == 0);
static_assert(static_cast<int>(EnclosureType::Sealed) == 1);
static_assert(static_cast<int>(EnclosureType::Vented) == 2);
static_assert(static_cast<int>(EnclosureType::Bandpass) == 3);

class driver {
public:
	driver();
	~driver();

	double Vb,Fb,Ql,V2,gain;

	std::array<std::complex<double>, 150> ResultPressure, ResultImpedance;
	double	Unit[49];
	EnclosureType enclosureType, enclosureTypeProposal;
	bool	parameterFlag,pistonLowPassActive,fullCircuitFlag;
	bool 	pressureIsActive,impedanceIsActive,summaryIsActive,ScalarSummaryisActive,
		ImpedanceSummaryisActive,InvertPhase;

	void setModified(void);
	void initContents(void);

	void calculatePressureResponse(void);
	void calculateImpedanceResponse (void);
	void calculateParameters(void);
	void invertImpedance(void);
	void cleanupNetwork(void);

	QString	getTitle() const;
	void	setTitle( const QString& a_qstringTitle );
  /** Sets Rdc value */
  void setRdc(double rdc);
  /** No descriptions */
  void setQtc(double qtc);
  /** No descriptions */
  void setF0(double f0);
  /** Sets Lsp value */
  void setLsp(double lsp);
  /** No descriptions */
  void setDm(double dm);
  /** No descriptions */
  void setQl(double ql);
  /** No descriptions */
  void setVas(double vas);
  /** No descriptions */
  void setQms(double qms);
  /** No descriptions */
  void setQes(double qes);
  /** Sets the full circuit flag */
  void setFullCircuit(bool toggle);
  /** Gets the full circuit flag */
  bool getFullCircuit(void) const;
  /** No descriptions */
  double getDm() const;
  /** No descriptions */
  double getQl() const;
  /** No descriptions */
  double getVas() const;
  /** No descriptions */
  double getQms() const;
  /** No descriptions */
  double getQes() const;
  /** No descriptions */
  double getQtc() const;
  /** No descriptions */
  double getF0() const;
  /** No descriptions */
  double getLsp() const;
  /** No descriptions */
  double getRdc() const;
  /** Sets network unit values */
  void setUnit(int unit, double val);
  /** No descriptions */
  double getUnit(int unit) const;
private:

	std::complex<double> calculateEquivalentCircuit(double omega);
	void calculateAcousticResponse(std::complex<double>& response, double omega);
	int  findLastNetworkSectionOffset(void);
	void calculateParallelBranch(std::complex<double>& networkImpedance, double omega, int sectionOffset);
	void calculateSeriesBranch(std::complex<double>& networkImpedance, double omega, int sectionOffset);

	bool dirty_pressure,dirty_impedance;
	// Historical diagnostic switch retained intentionally: legacy KFilter could
	// display only the bass-reflex-port contribution for a vented enclosure.
	// There is currently no UI/API toggle for this mode, but the calculation
	// path remains available for possible future diagnostic use.
	bool show_reflex_only;

	double
		Rdc,Lsp,F0,Qtc,Qms,Qe,Dm,Vas,
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
		radiationCapacitance,Norm,calibrate,
		C2,L2,R2,
		motionalCapacitance,motionalInductance,motionalResistance,
		acousticHighPassCapacitance,acousticHighPassInductance;

	QString	m_qstringTitle;
};

#endif
