/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

/*
 * Historical note:
 *
 * This file contains code derived from the original KFilter calculation core,
 * which started as Turbo Pascal code developed on an 8088-based PC. Some of the
 * structure still reflects performance optimizations that were necessary on
 * that hardware, including many explicit special-case branches.
 *
 * The remaining German comments are also a historical artifact of the original
 * code base. They were written during the early development of KFilter and have
 * intentionally not been translated mechanically, because some of them may
 * capture implementation details or domain-specific reasoning that should be
 * reviewed carefully during refactoring.
 *
 * These optimizations improved runtime performance in the original version, but
 * reduce readability today. The intended long-term direction is to refactor this
 * code into clearer and more maintainable calculation modules while preserving
 * the established numerical behaviour.
 */

#include "driver.h"
#include "kfilterfrequencygrid.h"
#include <cmath>

driver::driver()
{

	resetToDefaults();
}

driver::~driver()
{
}

void driver::resetToDefaults()
{
	Rdc=5.1;
	Lsp=0.00017;
	F0=307;
	Qts=1.14;
	Qms=1.9;
	Qes=2.87;
	Vas=10;
	Dm=7.3;
	gain = 1.0;
	plotState_ = {};
	phaseInverted=false;
	show_reflex_only=false;
	Vb=0;
	V2=0;
	Fb=0;
	Ql=10;
	pistonLowPassInductance=0;
	pistonLowPassCapacitance=0;
	pistonLowPassQ=0;
	pistonLowPassFrequency=0;
	enclosureTypeProposal = EnclosureType::OpenBaffle;
	pistonLowPassActive=false;
	fullCircuitFlag=false;
	network = {};
	m_qstringTitle = "This is a default driver";
	calculateParameters();
	setModified();
}

void driver::setModified(void)
{
	dirty_pressure = true;
	dirty_impedance = true;
}

void driver::calculateParameters(void)
{
	double pi = 3.141592654;


	if (F0!=0)
	{
		if (Dm==0)
		{
			Dm=10;
		}
		// Full-circuit ideal-piston model: this normalized capacitance models
		// the driver's radiation resistance. The simplified model uses the
		// separate 0 dB-normalized pistonLowPassInductance/pistonLowPassCapacitance approximation instead.
		radiationCapacitance=1/(2*pi*34000/Dm);
		motionalResistance=Qms*Rdc/Qes;
		motionalCapacitance=Qes/(2*pi*Rdc*F0);
		motionalInductance=Rdc/(2*pi*Qes*F0);
		//************************************************************************************************
		//double ii;
		//for (int i=99; i>-1; i--)
		//{
		//  ii=i;
		//	Cline[i]=StrahlC;
		//	Lline[i]=1/(pow((2*pi*50*(ii+1)/100),2.0)*Cline[i]);
		//	Rline[i]= 2*pi*50*(ii+1)/100*Lline[i]/2.0;
		//}
		//************************************************************************************************


		if ((Vb==0)||(enclosureTypeProposal==EnclosureType::OpenBaffle))
		{
			acousticHighPassCapacitance=Qts/(2*pi*F0);
			acousticHighPassInductance=1/(Qts*2*pi*F0);
			enclosureType=EnclosureType::OpenBaffle;
		}
		else
		{
			acousticHighPassCapacitance=Qts/(2*pi*F0);
			acousticHighPassInductance=1/(2*pi*F0*Qts*(Vas/Vb+1));
			enclosureType=EnclosureType::Sealed;
			//}

			if ((Fb!=0)&&(static_cast<int>(enclosureTypeProposal)>=static_cast<int>(EnclosureType::Vented)))
			{
				enclosureType=EnclosureType::Vented;
				enclosureBranchInductance=Vb*motionalInductance/Vas;
				enclosureBranchCapacitance=1/( enclosureBranchInductance*pow((2*pi*Fb),2.0) );
				enclosureBranchResistance=(2*pi*Fb*enclosureBranchInductance/Ql); // Legacy source questioned an alternative formula here: sqrt(C/L)/Ql; ?
				ventedDenominatorA0=pow((Fb/F0),2.0);
				ventedDenominatorA1=ventedDenominatorA0/Qts + Fb/(Ql*F0);
				ventedDenominatorA2=1 + ventedDenominatorA0 + Fb/(Ql*F0*Qts) + Vas/Vb;
				ventedDenominatorA3=1/Qts + Fb/(Ql*F0);
				if ((V2!=0)&&(static_cast<int>(enclosureTypeProposal)>=static_cast<int>(EnclosureType::Bandpass)))
				{
					enclosureType=EnclosureType::Bandpass;
					// Preserve the historical recalculation before applying the V2
					// modification to the motional branch.
					enclosureBranchInductance=Vb*motionalInductance/Vas;
					motionalInductance=1/(1/motionalInductance+Vas/(V2*motionalInductance));
				}
			}
			else         // -> Fb ist jetzt gleich Null s.o.
			{
				enclosureType=EnclosureType::Sealed;
				sealedEffectiveMotionalInductance=1/(1/motionalInductance+Vas/(Vb*motionalInductance));
			}
		}			//Vb==0
	}
	//if f0 != 0

	// Historical 0 dB-normalized approximation of the natural upper roll-off
	// of an ideal piston radiator. pistonLowPassQ/pistonLowPassFrequency currently have no
	// productive setter/UI path; keep the model dormant until the original
	// parameter derivation has been reconstructed.
	if ((pistonLowPassQ!=0) && (pistonLowPassFrequency!=0))
	{
		pistonLowPassInductance = 1/(2*pi*pistonLowPassQ*pistonLowPassFrequency);
		pistonLowPassCapacitance = pistonLowPassQ/(2*pi*pistonLowPassFrequency);
		pistonLowPassActive  = true;
	}
	else
	{
		pistonLowPassActive = false;
	}

	// Preserve the historical full-circuit normalization exactly. The calibration
	// gain is numerically about +22 dB; its original absolute reference is unknown.
	fullCircuitNormalizationFactor=sqrt(8/Rdc)*LegacyFullCircuitCalibrationGain * sqrt(2.0);  //{/sqrt(1 + 1/Qms )}

}

void driver::calculatePressureResponse(void)
{
	if (dirty_pressure)
	{
		calculateParameters();
		double omega = 125.6637061;
		for (std::size_t sampleIndex=0; sampleIndex<resultPressure.size(); ++sampleIndex)
		{
			const int lastSectionIndex = findLastNetworkSectionIndex();
			std::complex<double> response{1.0, 0.0};
			std::complex<double> networkImpedance = calculateEquivalentCircuit(omega);
			for (int sectionIndex = lastSectionIndex; sectionIndex >= 0; --sectionIndex)
			{
				const NetworkSection& section = network[static_cast<std::size_t>(sectionIndex)];
				calculateParallelBranch(networkImpedance, omega, section.parallel);
				const std::complex<double> terminationImpedance = networkImpedance;
				calculateSeriesBranch(networkImpedance, omega, section.series);
				response *= terminationImpedance / networkImpedance;
			}
			if (F0 != 0)
			{
				calculateAcousticResponse(response, omega);
			}
			//berechneaktivefilter;
			//ausgleichberechnen;
			resultPressure[sampleIndex] = (phaseInverted ? -response : response) * gain;
			omega = omega*KFilterFrequencyStep;
		}
		dirty_pressure = false;
	}
}

void driver::calculateImpedanceResponse(void)
{
	if (dirty_impedance)
	{
		calculateParameters();
		double omega = 125.6637061;
		for (std::size_t sampleIndex=0; sampleIndex<resultImpedance.size(); ++sampleIndex)
		{
			const int lastSectionIndex = findLastNetworkSectionIndex();
			std::complex<double> networkImpedance = calculateEquivalentCircuit(omega);
			for (int sectionIndex = lastSectionIndex; sectionIndex >= 0; --sectionIndex)
			{
				const NetworkSection& section = network[static_cast<std::size_t>(sectionIndex)];
				calculateParallelBranch(networkImpedance, omega, section.parallel);
				calculateSeriesBranch(networkImpedance, omega, section.series);
			}
			resultImpedance[sampleIndex] = networkImpedance;
			omega=omega*KFilterFrequencyStep;
		}
		dirty_impedance = false;
	}
}

std::complex<double> driver::calculateEquivalentCircuit(double omega)
{
	if (F0 == 0)
	{
		return {Rdc, omega * Lsp};
	}

	std::complex<double> admittance;

	switch (enclosureType)
	{
	case EnclosureType::Sealed :
		admittance = {1/motionalResistance, omega*motionalCapacitance-1/(omega*sealedEffectiveMotionalInductance)};
		break;
	case EnclosureType::Vented : case EnclosureType::Bandpass :
		{
			admittance = {1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			const std::complex<double> branchImpedance{enclosureBranchResistance, omega*enclosureBranchInductance-1/(omega*enclosureBranchCapacitance)};
			admittance += 1.0 / branchImpedance;
			break;
		}
	case EnclosureType::OpenBaffle :
		admittance = {1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
		break;
	}
	return std::complex<double>{Rdc, omega*Lsp} + 1.0 / admittance;
}

void driver::calculateAcousticResponse(std::complex<double>& response, double omega)
{
	std::complex<double> networkImpedance;
	std::complex<double> terminationImpedance;
	double factor;
	double bw;
	double bu;
	double bx;//,hilfe;

	//if hub then BEGIN berechnehub;exit END;

	// Physical Driver-model component, not the user-configurable Active Filter
	// low-pass. The historical implementation contributes magnitude only and
	// is normalized to 0 dB in its pass band.
	if (pistonLowPassActive)
	{
		networkImpedance = 1.0 / std::complex<double>{1.0, omega * pistonLowPassCapacitance};
		terminationImpedance = networkImpedance;
		networkImpedance += std::complex<double>{0.0, omega * pistonLowPassInductance};
		response *= std::abs(terminationImpedance / networkImpedance);
	}

	switch (enclosureType)
	{
	case EnclosureType::OpenBaffle :  if (fullCircuitFlag)
			  {
				  //********************************************************************************test
				  //xa=1; ya=f*Cline[99];	inverse(&xa,&ya);
				  //x=xa+Rline[99];	y=ya+f*Lline[99];	Quotient();

				  //for (int i=98; i>-1; i--){
				  //xa=x; ya=y+f*Cline[i];	inverse(&xa,&ya);
				  //x=xa+Rline[i];	y=ya+f*Lline[i];	Quotient();
				  //}

				  //********************************************************************************test

				  terminationImpedance = 1.0 / std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
				  networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				  response *= terminationImpedance / networkImpedance;
				  terminationImpedance = {1.0, 0.0};
				  networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				  response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				  response *= fullCircuitNormalizationFactor;
			  }
		else
		{
			networkImpedance = 1.0 / std::complex<double>{1.0, -1/(omega*acousticHighPassInductance)};
			terminationImpedance = networkImpedance;
			networkImpedance += std::complex<double>{0.0, -1/(omega*acousticHighPassCapacitance)};
			const std::complex<double> transfer = terminationImpedance / networkImpedance;
			response *= transfer;

		}         //ELSE von realschall
		break;

	case EnclosureType::Sealed :  if (fullCircuitFlag)
			  {

				  terminationImpedance = 1.0 / std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*sealedEffectiveMotionalInductance)};
				  networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				  response *= terminationImpedance / networkImpedance;
				  terminationImpedance = {1.0, 0.0};
				  networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				  response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				  response *= fullCircuitNormalizationFactor;
			  }
		else
		{
			networkImpedance = 1.0 / std::complex<double>{1.0, -1/(omega*acousticHighPassInductance)};
			terminationImpedance = networkImpedance;
			networkImpedance += std::complex<double>{0.0, -1/(omega*acousticHighPassCapacitance)};
			const std::complex<double> transfer = terminationImpedance / networkImpedance;
			response *= transfer;

		}         //ELSE von realschall
		break;

	case EnclosureType::Vented :
		if (fullCircuitFlag)
		{
			terminationImpedance = {0.0, omega*enclosureBranchInductance};
			networkImpedance = {enclosureBranchResistance, omega*enclosureBranchInductance-1/(omega*enclosureBranchCapacitance)};
			response *= terminationImpedance / networkImpedance;
			networkImpedance = 1.0 / networkImpedance;
			terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			terminationImpedance = 1.0 / terminationImpedance;
			networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
			response *= terminationImpedance / networkImpedance;
			terminationImpedance = {1.0, 0.0};
			networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
			response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
			response *= fullCircuitNormalizationFactor;
		}
		else
		{
			if (!show_reflex_only)
			{
				bw=omega*0.159154943/F0;    //omega/(2*pi)
				bu=pow(bw,2.0);
				bx=pow(bu,2.0);
				factor=bx/sqrt(pow(bx-ventedDenominatorA2*bu+ventedDenominatorA0,2.0)+pow(ventedDenominatorA1*bw-ventedDenominatorA3*bu*bw,2.0));
				response *= factor;
			}
			else
			{
				terminationImpedance = {0.0, -1/(omega*enclosureBranchCapacitance)};
				networkImpedance = {enclosureBranchResistance, -1/(omega*enclosureBranchCapacitance)+omega*enclosureBranchInductance};
				response *= terminationImpedance / networkImpedance;
				networkImpedance = 1.0 / networkImpedance;
				terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
				terminationImpedance = 1.0 / terminationImpedance;
				networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				response *= terminationImpedance / networkImpedance;
				terminationImpedance = {1.0, 0.0};
				networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				response *= fullCircuitNormalizationFactor;
			}
		}
		break;
	case EnclosureType::Bandpass :
		{
			terminationImpedance = {0.0, -1/(omega*enclosureBranchCapacitance)};
			networkImpedance = {enclosureBranchResistance, -1/(omega*enclosureBranchCapacitance)+omega*enclosureBranchInductance};
			response *= terminationImpedance / networkImpedance;
			networkImpedance = 1.0 / networkImpedance;
			terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			terminationImpedance = 1.0 / terminationImpedance;
			networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
			response *= terminationImpedance / networkImpedance;
			terminationImpedance = {1.0, 0.0};
			networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
			response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
			response *= fullCircuitNormalizationFactor;
		}
	}       //switch
}

int driver::findLastNetworkSectionIndex(void) const
{
	for (int sectionIndex = static_cast<int>(network.size()) - 1; sectionIndex >= 0; --sectionIndex)
	{
		const NetworkSection& section = network[static_cast<std::size_t>(sectionIndex)];
		if (section.series.resistance != 0.0 ||
			section.series.capacitance != 0.0 ||
			section.series.inductance != 0.0 ||
			section.parallel.resistance != 0.0 ||
			section.parallel.capacitance != 0.0 ||
			section.parallel.inductance != 0.0)
		{
			return sectionIndex;
		}
	}
	return -1;
}

void driver::calculateParallelBranch(std::complex<double>& networkImpedance, double omega, const NetworkBranch& branch)
{
	std::complex<double> branchImpedance;

	const double resistance = branch.resistance;
	if (branch.capacitance==0)
	{
		if (branch.inductance==0)
		{
			if (branch.resistance==0)
			{
				return;
			}
			else
			{
				branchImpedance = {resistance, 0.0};
			}
		}
		else
		{
			branchImpedance = {resistance, omega*branch.inductance};
		}
	}
	else
	{
		if (branch.inductance==0)
		{
			branchImpedance = {resistance, -1/(omega*branch.capacitance)};
		}
		else
		{
			branchImpedance = {resistance, omega*branch.inductance - 1/(omega*branch.capacitance)};
		}
	}
	networkImpedance = 1.0 / (1.0 / networkImpedance + 1.0 / branchImpedance);
}

void driver::calculateSeriesBranch(std::complex<double>& networkImpedance, double omega, const NetworkBranch& branch)
{
	if (branch.capacitance==0)
	{
		if (branch.inductance==0)
		{
			networkImpedance += std::complex<double>{branch.resistance, 0.0};
		}
		else
		{
			networkImpedance += std::complex<double>{branch.resistance, omega*branch.inductance};
		}
	}
	else
	{
		double susceptance;
		if (branch.inductance==0)
		{
			susceptance = omega*branch.capacitance;
		}
		else
		{
			susceptance = omega*branch.capacitance - 1/(omega*branch.inductance);
		}
		if (branch.resistance==0)
		{
			networkImpedance += std::complex<double>{0.0, -1/susceptance};
		}
		else
		{
			const std::complex<double> branchAdmittance{1/branch.resistance, susceptance};
			networkImpedance += 1.0 / branchAdmittance;
		}
	}
}

const driver::ResponseArray& driver::pressureResponse() const
{
	return resultPressure;
}

const driver::ResponseArray& driver::impedanceResponse() const
{
	return resultImpedance;
}

const DriverPlotState& driver::plotState() const
{
	return plotState_;
}

void driver::setPlotState(const DriverPlotState& state)
{
	plotState_ = state;
}

QString driver::getTitle() const
{
	return m_qstringTitle;
}

void driver::setTitle( const QString& a_qstringTitle )
{
	m_qstringTitle = a_qstringTitle;
}
/** Sets Rdc value */
void driver::setRdc(double rdc){
Rdc = rdc;
setModified();
}
/** Sets Lsp value */
void driver::setLsp(double lsp){
Lsp = lsp;
setModified();
}
/** Sets F0 value*/
void driver::setF0(double f0){
F0 = f0;
setModified();
}
/** Sets total driver Q (Qts). */
void driver::setQts(double qts){
Qts = qts;
setModified();
}
/** Sets Qes value */
void driver::setQes(double qes){
Qes = qes;
setModified();
}
/** Sets the full circuit flag */
void driver::setFullCircuit(bool toggle){
fullCircuitFlag = toggle;
setModified();
}
/** Gives the full circuit flag */
bool driver::getFullCircuit() const{
return fullCircuitFlag;
}
/** Sets Qms value */
void driver::setQms(double qms){
Qms = qms;
setModified();
}
/** Sets Vas value */
void driver::setVas(double vas){
Vas = vas;
setModified();
}
/** Sets Dm value */
void driver::setDm(double dm){
Dm = dm;
setModified();
}
/** Sets Vb value */
void driver::setVb(double vb){
Vb = vb;
setModified();
}
/** Sets Fb value */
void driver::setFb(double fb){
Fb = fb;
setModified();
}
/** Sets Ql value */
void driver::setQl(double ql){
Ql = ql;
setModified();
}
/** Sets V2 value */
void driver::setV2(double v2){
V2 = v2;
setModified();
}
/** Sets linear gain value */
void driver::setGainLinear(double linearGain){
gain = linearGain;
setModified();
}
/** Sets pressure-response phase inversion (polarity reversal). */
void driver::setPhaseInverted(bool inverted){
phaseInverted = inverted;
setModified();
}
/** Sets requested enclosure alignment. */
void driver::setEnclosureTypeProposal(EnclosureType type){
enclosureTypeProposal = type;
setModified();
}
/** No descriptions */
double driver::getRdc() const{
return Rdc;
}
/** No descriptions */
double driver::getLsp() const{
return Lsp;
}
/** No descriptions */
double driver::getF0() const{
return F0;
}
/** No descriptions */
double driver::getQts() const{
return Qts;
}
/** No descriptions */
double driver::getQes() const{
return Qes;
}
/** No descriptions */
double driver::getQms() const{
return Qms;
}
/** No descriptions */
double driver::getVas() const{
return Vas;
}
/** No descriptions */
double driver::getDm() const{
return Dm;
}
/** Gets Vb value */
double driver::getVb() const{
return Vb;
}
/** Gets Fb value */
double driver::getFb() const{
return Fb;
}
/** Gets Ql value */
double driver::getQl() const{
return Ql;
}
/** Gets V2 value */
double driver::getV2() const{
return V2;
}
/** Gets linear gain value */
double driver::getGainLinear() const{
return gain;
}
/** Returns pressure-response phase inversion state. */
bool driver::isPhaseInverted() const{
return phaseInverted;
}
/** Returns requested enclosure alignment. */
EnclosureType driver::getEnclosureTypeProposal() const{
return enclosureTypeProposal;
}

driver::NetworkBranch* driver::networkBranch(int sectionIndex, NetworkBranchType branch)
{
	if (sectionIndex < 0 || sectionIndex >= static_cast<int>(NetworkSectionCount))
	{
		return nullptr;
	}

	NetworkSection& section = network[static_cast<std::size_t>(sectionIndex)];
	switch (branch)
	{
	case NetworkBranchType::Series:
		return &section.series;
	case NetworkBranchType::Shunt:
		return &section.parallel;
	}
	return nullptr;
}

const driver::NetworkBranch* driver::networkBranch(int sectionIndex, NetworkBranchType branch) const
{
	if (sectionIndex < 0 || sectionIndex >= static_cast<int>(NetworkSectionCount))
	{
		return nullptr;
	}

	const NetworkSection& section = network[static_cast<std::size_t>(sectionIndex)];
	switch (branch)
	{
	case NetworkBranchType::Series:
		return &section.series;
	case NetworkBranchType::Shunt:
		return &section.parallel;
	}
	return nullptr;
}

void driver::setNetworkValue(int sectionIndex,
                             NetworkBranchType branch,
                             NetworkComponent component,
                             double value)
{
	NetworkBranch* selectedBranch = networkBranch(sectionIndex, branch);
	if (selectedBranch == nullptr)
	{
		return;
	}

	switch (component)
	{
	case NetworkComponent::Resistance:
		selectedBranch->resistance = value;
		break;
	case NetworkComponent::Capacitance:
		selectedBranch->capacitance = value;
		break;
	case NetworkComponent::Inductance:
		selectedBranch->inductance = value;
		break;
	default:
		return;
	}
	setModified();
}

double driver::getNetworkValue(int sectionIndex,
                               NetworkBranchType branch,
                               NetworkComponent component) const
{
	const NetworkBranch* selectedBranch = networkBranch(sectionIndex, branch);
	if (selectedBranch == nullptr)
	{
		return -1.0;
	}

	switch (component)
	{
	case NetworkComponent::Resistance:
		return selectedBranch->resistance;
	case NetworkComponent::Capacitance:
		return selectedBranch->capacitance;
	case NetworkComponent::Inductance:
		return selectedBranch->inductance;
	}
	return -1.0;
}

