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

	initContents();
}

driver::~driver()
{
}

void driver::initContents()
{
	Rdc=5.1;
	Lsp=0.00017;
	F0=307;
	Qtc=1.14;
	Qms=1.9;
	Qe=2.87;
	Vas=10;
	Dm=7.3;
	gain = 1.0;
	pressureIsActive=false;
	impedanceIsActive=false;
	summaryIsActive=false;
	ScalarSummaryisActive=false;
	ImpedanceSummaryisActive=false;
	InvertPhase=false;
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
	parameterFlag=true;
	pistonLowPassActive=false;
	fullCircuitFlag=false;
	for (int i=0;i<49;i++)
	{
		Unit[i]=0;
	}
	calibrate = 12.58925412;
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

	parameterFlag = true;

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
		motionalResistance=Qms*Rdc/Qe;
		motionalCapacitance=Qe/(2*pi*Rdc*F0);
		motionalInductance=Rdc/(2*pi*Qe*F0);
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
			acousticHighPassCapacitance=Qtc/(2*pi*F0);
			acousticHighPassInductance=1/(Qtc*2*pi*F0);
			L2=motionalInductance;
			enclosureType=EnclosureType::OpenBaffle;
		}
		else
		{
			acousticHighPassCapacitance=Qtc/(2*pi*F0);
			acousticHighPassInductance=1/(2*pi*F0*Qtc*(Vas/Vb+1));
			enclosureType=EnclosureType::Sealed;
			//}

			if ((Fb!=0)&&(static_cast<int>(enclosureTypeProposal)>=static_cast<int>(EnclosureType::Vented)))
			{
				enclosureType=EnclosureType::Vented;
				L2=Vb*motionalInductance/Vas;
				C2=1/( L2*pow((2*pi*Fb),2.0) );
				R2=(2*pi*Fb*L2/Ql); //R2:=sqrt(C2/L2)/Ql; ?
				ventedDenominatorA0=pow((Fb/F0),2.0);
				ventedDenominatorA1=ventedDenominatorA0/Qtc + Fb/(Ql*F0);
				ventedDenominatorA2=1 + ventedDenominatorA0 + Fb/(Ql*F0*Qtc) + Vas/Vb;
				ventedDenominatorA3=1/Qtc + Fb/(Ql*F0);
				if ((V2!=0)&&(static_cast<int>(enclosureTypeProposal)>=static_cast<int>(EnclosureType::Bandpass)))
				{
					enclosureType=EnclosureType::Bandpass;
					L2=Vb*motionalInductance/Vas;
					motionalInductance=1/(1/motionalInductance+Vas/(V2*motionalInductance));
				}
			}
			else         // -> Fb ist jetzt gleich Null s.o.
			{
				enclosureType=EnclosureType::Sealed;
				L2=1/(1/motionalInductance+Vas/(Vb*motionalInductance));
			}
		}			//Vb==0
	}
	//if f0 != 0
	else
	{
		parameterFlag = false;
	}

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

	Norm=sqrt(8/Rdc)*calibrate * sqrt(2.0);  //{/sqrt(1 + 1/Qms )}

}

void driver::calculatePressureResponse(void)
{
	if (dirty_pressure)
	{
		calculateParameters();
		double omega = 125.6637061;
		for (std::size_t sampleIndex=0; sampleIndex<ResultPressure.size(); ++sampleIndex)
		{
			int sectionOffset = findLastNetworkSectionOffset()+1;
			std::complex<double> response{1.0, 0.0};
			std::complex<double> networkImpedance = calculateEquivalentCircuit(omega);
			while (sectionOffset > 1)
			{
				sectionOffset = sectionOffset-6;
				calculateParallelBranch(networkImpedance, omega, sectionOffset);
				const std::complex<double> terminationImpedance = networkImpedance;
				calculateSeriesBranch(networkImpedance, omega, sectionOffset);
				response *= terminationImpedance / networkImpedance;
			}
			if (parameterFlag)
			{
				calculateAcousticResponse(response, omega);
			}
			//berechneaktivefilter;
			//ausgleichberechnen;
			ResultPressure[sampleIndex] = (InvertPhase ? -response : response) * gain;
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
		for (std::size_t sampleIndex=0; sampleIndex<ResultImpedance.size(); ++sampleIndex)
		{
			int sectionOffset = findLastNetworkSectionOffset()+1;
			std::complex<double> networkImpedance = calculateEquivalentCircuit(omega);
			while (sectionOffset > 1)
			{
				sectionOffset = sectionOffset-6;
				calculateParallelBranch(networkImpedance, omega, sectionOffset);
				calculateSeriesBranch(networkImpedance, omega, sectionOffset);
			}
			ResultImpedance[sampleIndex] = networkImpedance;
			omega=omega*KFilterFrequencyStep;
		}
		dirty_impedance = false;
	}
}

std::complex<double> driver::calculateEquivalentCircuit(double omega)
{
	if (!parameterFlag)
	{
		return {Rdc, omega * Lsp};
	}

	std::complex<double> admittance;

	switch (enclosureType)
	{
	case EnclosureType::Sealed :
		admittance = {1/motionalResistance, omega*motionalCapacitance-1/(omega*L2)};
		break;
	case EnclosureType::Vented : case EnclosureType::Bandpass :
		{
			admittance = {1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			const std::complex<double> branchImpedance{R2, omega*L2-1/(omega*C2)};
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

				  terminationImpedance = 1.0 / std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*L2)};
				  networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				  response *= terminationImpedance / networkImpedance;
				  terminationImpedance = {1.0, 0.0};
				  networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				  response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				  response *= Norm;
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

				  terminationImpedance = 1.0 / std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*L2)};
				  networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				  response *= terminationImpedance / networkImpedance;
				  terminationImpedance = {1.0, 0.0};
				  networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				  response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				  response *= Norm;
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
			terminationImpedance = {0.0, omega*L2};
			networkImpedance = {R2, omega*L2-1/(omega*C2)};
			response *= terminationImpedance / networkImpedance;
			networkImpedance = 1.0 / networkImpedance;
			terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			terminationImpedance = 1.0 / terminationImpedance;
			networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
			response *= terminationImpedance / networkImpedance;
			terminationImpedance = {1.0, 0.0};
			networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
			response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
			response *= Norm;
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
				terminationImpedance = {0.0, -1/(omega*C2)};
				networkImpedance = {R2, -1/(omega*C2)+omega*L2};
				response *= terminationImpedance / networkImpedance;
				networkImpedance = 1.0 / networkImpedance;
				terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
				terminationImpedance = 1.0 / terminationImpedance;
				networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
				response *= terminationImpedance / networkImpedance;
				terminationImpedance = {1.0, 0.0};
				networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
				response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
				response *= Norm;
			}
		}
		break;
	case EnclosureType::Bandpass :
		{
			terminationImpedance = {0.0, -1/(omega*C2)};
			networkImpedance = {R2, -1/(omega*C2)+omega*L2};
			response *= terminationImpedance / networkImpedance;
			networkImpedance = 1.0 / networkImpedance;
			terminationImpedance = networkImpedance + std::complex<double>{1/motionalResistance, omega*motionalCapacitance-1/(omega*motionalInductance)};
			terminationImpedance = 1.0 / terminationImpedance;
			networkImpedance = terminationImpedance + std::complex<double>{Rdc, omega*Lsp};
			response *= terminationImpedance / networkImpedance;
			terminationImpedance = {1.0, 0.0};
			networkImpedance = {1.0, -1/(omega*radiationCapacitance)};
			response *= terminationImpedance / networkImpedance; //Strahlungswiederstand
			response *= Norm;
		}
	}       //switch
}

int driver::findLastNetworkSectionOffset(void)
{
	for (int j=48;j>=0;j--)
	{
		if (Unit[j]!=0.0)
		{
			if (fmod(j,6)<0.00001)
			{
				return j;
			}
			else
			{
				return  int(j/6)*6 +6;
			}
		}
	}
	return 0;
}

void driver::calculateParallelBranch(std::complex<double>& networkImpedance, double omega, int sectionOffset)
{
	std::complex<double> branchImpedance;

	const double resistance = Unit[sectionOffset+3];
	if (Unit[sectionOffset+4]==0)
	{
		if (Unit[sectionOffset+5]==0)
		{
			if (Unit[sectionOffset+3]==0)
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
			branchImpedance = {resistance, omega*Unit[sectionOffset+5]};
		}
	}
	else
	{
		if (Unit[sectionOffset+5]==0)
		{
			branchImpedance = {resistance, -1/(omega*Unit[sectionOffset+4])};
		}
		else
		{
			branchImpedance = {resistance, omega*Unit[sectionOffset+5] - 1/(omega*Unit[sectionOffset+4])};
		}
	}
	networkImpedance = 1.0 / (1.0 / networkImpedance + 1.0 / branchImpedance);
}

void driver::calculateSeriesBranch(std::complex<double>& networkImpedance, double omega, int sectionOffset)
{
	if (Unit[sectionOffset+1]==0)
	{
		if (Unit[sectionOffset+2]==0)
		{
			networkImpedance += std::complex<double>{Unit[sectionOffset], 0.0};
		}
		else
		{
			networkImpedance += std::complex<double>{Unit[sectionOffset], omega*Unit[sectionOffset+2]};
		}
	}
	else
	{
		double susceptance;
		if (Unit[sectionOffset+2]==0)
		{
			susceptance = omega*Unit[sectionOffset+1];
		}
		else
		{
			susceptance = omega*Unit[sectionOffset+1] - 1/(omega*Unit[sectionOffset+2]);
		}
		if (Unit[sectionOffset]==0)
		{
			networkImpedance += std::complex<double>{0.0, -1/susceptance};
		}
		else
		{
			const std::complex<double> branchAdmittance{1/Unit[sectionOffset], susceptance};
			networkImpedance += 1.0 / branchAdmittance;
		}
	}
}

void driver::invertImpedance(void)
{
	for (std::complex<double>& impedance : ResultImpedance)
	{
		impedance = 1.0 / impedance;
	}
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
/** Sets Qtc value */
void driver::setQtc(double qtc){
Qtc = qtc;
setModified();
}
/** Sets Qes value */
void driver::setQes(double qes){
Qe = qes;
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
/** Sets Ql value */
void driver::setQl(double ql){
Ql = ql;
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
double driver::getQtc() const{
return Qtc;
}
/** No descriptions */
double driver::getQes() const{
return Qe;
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
/** Gets Ql value */
double driver::getQl() const{
return Ql;
}

/** Sets network units values */
void driver::setUnit(int unit, double val){
if ((unit>-1)&&(unit<49))
	{
	Unit[unit] = val;
  this->setModified();
	}
}

/** No descriptions */
double driver::getUnit(int unit) const{
	if ((unit>-1)&&(unit<49))
	return Unit[unit]; else
	return -1;
}

void driver::cleanupNetwork(void){
	for ( int intI = 0; intI < 49; intI++ )
		Unit[intI] = 0;
	setModified();
}

