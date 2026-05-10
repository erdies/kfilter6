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
	Faktor=1.047128548;
	Anzahl = 0;
	Rdc=5.1;
	Lsp=0.00017;
	F0=307;
	Qtc=1.14;
	Qms=1.9;
	Qe=2.87;
	Vas=10;
	Dm=7.3;
	gain = 1.0;
	PressureisActive=false;
	ImpedanzisActive=false;
	SummaryisActive=false;
	ScalarSummaryisActive=false;
	ImpedanzSummaryisActive=false;
	InvertPhase=false;
	show_reflex_only=false;
	Vb=0;
	V2=0;
	Fb=0;
	F3=0;
	Fs=0;
	Ql=10;
	TiefpassL=0;
	TiefpassC=0;
	TiefpassQ=0;
	Tiefpassfc=0;
	GTypProposal = 0;
	Phase_flag=1;
	Parameter_flag=1;
	Tiefpass_flag=0;
	AkustikESB_flag=1;
	Realschall_flag=0;
	for (int i=0;i<49;i++)
	{
		Unit[i]=0;
	}
	calibrate = 12.58925412;
	m_qstringTitle = "This is a default driver";
	Berechneparameter();
	setmodified();
}

void driver::setmodified(void)
{
	dirty_schall = true;
	dirty_impedanz = true;
}

void driver::Berechneparameter(void)
{
	double pi = 3.141592654;

	if (F0!=0)
	{
		if (Dm==0)
		{
			Dm=10;
		}
		StrahlC=1/(2*pi*34000/Dm);	// <----- noch nicht aktiv
		R=Qms*Rdc/Qe;				// <----- noch nicht aktiv
		C=Qe/(2*pi*Rdc*F0);
		L=Rdc/(2*pi*Qe*F0);
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


		if ((Vb==0)||(GTypProposal==0))
		{
			Cakustik=Qtc/(2*pi*F0);
			Lakustik=1/(Qtc*2*pi*F0);
			L2=L;
			GTyp=0;
		}
		else
		{
			Cakustik=Qtc/(2*pi*F0);
			Lakustik=1/(2*pi*F0*Qtc*(Vas/Vb+1));
			Fs=F0*sqrt(Vas/Vb+1);
			SystemQ=Qtc*sqrt(Vas/Vb+1);
			GTyp=1;
			//}

			if ((Fb!=0)&&(GTypProposal>=2))
			{
				GTyp=2;
				Phase_flag = 0;
				L2=Vb*L/Vas;
				C2=1/( L2*pow((2*pi*Fb),2.0) );
				R2=(2*pi*Fb*L2/Ql); //R2:=sqrt(C2/L2)/Ql; ?
				Consta=pow((Fb/F0),2.0);
				Constb=Consta/Qtc + Fb/(Ql*F0);
				Constc=1 + Consta + Fb/(Ql*F0*Qtc) + Vas/Vb;
				Constd=1/Qtc + Fb/(Ql*F0);
				if ((V2!=0)&&(GTypProposal>=3))
				{
					Phase_flag = 1;
					GTyp=3;
					L2=Vb*L/Vas;
					L=1/(1/L+Vas/(V2*L));
				}
			}
			else         // -> Fb ist jetzt gleich Null s.o.
			{
				GTyp=1;
				L2=1/(1/L+Vas/(Vb*L));
			}
		}			//Vb==0
	}
	//if f0 != 0
	else
	{
		Parameter_flag = 0;
		AkustikESB_flag = 0;
	}

	if ((TiefpassQ!=0) && (Tiefpassfc!=0))
	{
		TiefpassL = 1/(2*pi*TiefpassQ*Tiefpassfc);
		TiefpassC = TiefpassQ/(2*pi*Tiefpassfc);
		Tiefpass_flag  = 1;
	}
	else
	{
		Tiefpass_flag = 0;
	}

	Norm=sqrt(8/Rdc)*calibrate * sqrt(2.0);  //{/sqrt(1 + 1/Qms )}

}

void driver::Schall(void)
{
	if (dirty_schall)
	{
		//	double a,b,bw,fakt;
		Berechneparameter();
		f = 125.6637061;
		for (int j=0;j<300;j=j+2)
		{
			i=Anzahlcheck()+1;
			//xwert:=xkoordinate(f);
			qx=1; qy=0;
			ESBberechnen();
			while (i > 1)
			{
				i=i-6;
				Parallelberechnung();
				xa=x; ya=y;
				Reihenberechnung();
				Quotient();
			}
			if (AkustikESB_flag)
			{
				Akustik();
			}
			//berechneaktivefilter;
			//ausgleichberechnen;
			if (InvertPhase)
			{
				ResultSchall[j]=-qx*gain;
				ResultSchall[j+1]=-qy*gain;
			}
			else
			{
				ResultSchall[j]=qx*gain;
				ResultSchall[j+1]=qy*gain;
			}
			f = f*Faktor;
		}
		dirty_schall = false;
	}
}

void driver::Impedanz(void)
{
	if (dirty_impedanz)
	{

		// double   a;
		Berechneparameter();
		f = 125.6637061;
		for (int j=0;j<300;j=j+2)
		{
			i=Anzahlcheck()+1;
			ESBberechnen();
			while (i > 1)
			{
				i=i-6;
				Parallelberechnung();
				Reihenberechnung();
			}
			//a=f*0.159154943;
			ResultImpedanz[j]=x; ResultImpedanz[j+1]=y;
			f=f*Faktor;
		}
		dirty_impedanz = false;
	}
}

void driver::ESBberechnen()
{
	double a,b,a2,b2;

	//if Parameter_flag then
	//BEGIN
	switch (GTyp)
	{
	case 1 :
		b=f*C-1/(f*L2);
		a=1/R;break;
	case 2 : case 3 :
		a=1/R;
		b=f*C-1/(f*L);
		a2=R2;
		b2=f*L2-1/(f*C2);
		inverse(&a2,&b2);
		a=a+a2;
		b=b+b2;break;
	case 0 :
		a=1/R;
		b=f*C-1/(f*L);break;
	}
    inverse(&a,&b);
    x=Rdc+a;
    y=f*Lsp+b;
	/*   END
	ELSE
	BEGIN
	x:=Rdc[j];
	y:=f*Lsp[j];
	END*/
}

void driver::inverse(double *a,double *b)
{
	double hilfe;
	if ((*a) == 0)
	{
		(*b) = -1.0 / (*b);
	}
	else
	{
		if ((*a) == 1.0)
		{
			(*a) =  1.0 / (1.0 + pow((*b),2.0));
			(*b) = (-*b) * (*a);
		}
		else
		{
			hilfe = 1.0/(pow((*a),2.0)+pow((*b),2.0));
			(*a) =  (*a) * hilfe;
			(*b) = (-*b) * hilfe;
		}
	}
}

void driver::Akustik(void)
{
	double fakt;
	double a;
	double b;
	double bw;
	double bu;
	double bx;//,hilfe;

	//if hub then BEGIN berechnehub;exit END;

	if (Tiefpass_flag)
	{
		x=1; y = f * TiefpassC;
		inverse(&x,&y);
		xa=x;
		ya=y;
		y= y + f * TiefpassL;
		bw = pow(x,2.0);
		fakt = 1/(bw+pow(y,2.0));
		a = (bw+ya*y) * fakt;       // a = (xa*x+ya*y)/(sqr(x)+sqr(y));
		b = x*(ya-y) * fakt;        // b = (-xa*y+x*ya)/(sqr(x)+sqr(y));
		fakt=sqrt(pow(a,2.0)+pow(b,2.0));
		qx=qx * fakt;
		qy=qy * fakt;
	}

	switch (GTyp)
	{
	case 0 :  if (Realschall_flag)
			  {
				  //********************************************************************************test
				  //xa=1; ya=f*Cline[99];	inverse(&xa,&ya);
				  //x=xa+Rline[99];	y=ya+f*Lline[99];	Quotient();

				  //for (int i=98; i>-1; i--){
				  //xa=x; ya=y+f*Cline[i];	inverse(&xa,&ya);
				  //x=xa+Rline[i];	y=ya+f*Lline[i];	Quotient();
				  //}

				  //********************************************************************************test

				  xa=1/R;
				  ya=f*C-1/(f*L2);				inverse(&xa,&ya);
				  x=xa+Rdc;
				  y=ya+f*Lsp;					Quotient();
				  xa=1;
				  ya=0;
				  x=1;
				  y=-1/(f*StrahlC);
				  Quotient(); //Strahlungswiederstand
				  qx=qx*Norm;
				  qy=qy*Norm;
			  }
		else
		{
			x=1;
			y=-1/(f*Lakustik);
			inverse(&x,&y);
			xa=x;
			ya=y;
			y=y-1/(f*Cakustik);
			bw = pow(x,2.0);
			fakt = bw+pow(y,2.0);
			a = (bw+ya*y) / fakt;   // a = (xa*x+ya*y)/(sqr(x)+sqr(y));
			b = x*(ya-y) / fakt;    // b = (-xa*y+x*ya)/(sqr(x)+sqr(y));
			if (Phase_flag)
			{
				fakt = qx;
				qx = qx*a-qy*b;
				qy = fakt*b+qy*a;
			}
			else
			{
				fakt=sqrt(pow(a,2.0)+pow(b,2.0));
				qx=qx*fakt;
				qy=qy*fakt;
			}

		}         //ELSE von realschall
		break;

	case 1 :  if (Realschall_flag)
			  {

				  xa=1/R;
				  ya=f*C-1/(f*L2);				inverse(&xa,&ya);
				  x=xa+Rdc;
				  y=ya+f*Lsp;					Quotient();
				  xa=1;
				  ya=0;
				  x=1;
				  y=-1/(f*StrahlC);
				  Quotient(); //Strahlungswiederstand
				  qx=qx*Norm;
				  qy=qy*Norm;
			  }
		else
		{
			x=1;
			y=-1/(f*Lakustik);
			inverse(&x,&y);
			xa=x;
			ya=y;
			y=y-1/(f*Cakustik);
			bw = pow(x,2.0);
			fakt = bw+pow(y,2.0);
			a = (bw+ya*y) / fakt;   // a = (xa*x+ya*y)/(sqr(x)+sqr(y));
			b = x*(ya-y) / fakt;    // b = (-xa*y+x*ya)/(sqr(x)+sqr(y));
			if (Phase_flag)
			{
				fakt = qx;
				qx = qx*a-qy*b;
				qy = fakt*b+qy*a;
			}
			else
			{
				fakt=sqrt(pow(a,2.0)+pow(b,2.0));
				qx=qx*fakt;
				qy=qy*fakt;
			}

		}         //ELSE von realschall
		break;

	case 2 :
		if (Realschall_flag)
		{
			xa=0;
			ya=f*L2;
			x=R2;
			y=ya-1/(f*C2);
			Quotient();
			inverse(&x,&y);
			xa=x+1/R;
			ya=y+f*C-1/(f*L);
			inverse(&xa,&ya);
			x=xa+Rdc;
			y=ya+f*Lsp;
			Quotient();
			xa=1;
			ya=0;
			x=1;
			y=-1/(f*StrahlC);
			Quotient(); //Strahlungswiederstand
			qx=qx*Norm;
			qy=qy*Norm;
		}
		else
		{
			if (!show_reflex_only)
			{
				bw=f*0.159154943/F0;    //f/(2*pi)
				bu=pow(bw,2.0);
				bx=pow(bu,2.0);
				fakt=bx/sqrt(pow(bx-Constc*bu+Consta,2.0)+pow(Constb*bw-Constd*bu*bw,2.0));
				qx=qx*fakt;
				qy=qy*fakt;
			}
			else
			{
				xa=0;
				ya=-1/(f*C2);
				x=R2;
				y=ya+f*L2;
				Quotient();
				inverse(&x,&y);
				xa=x+1/R;
				ya=y+f*C-1/(f*L);
				inverse(&xa,&ya);
				x=xa+Rdc;
				y=ya+f*Lsp;
				Quotient();
				xa=1;
				ya=0;
				x=1;
				y=-1/(f*StrahlC);
				Quotient(); //Strahlungswiederstand
				qx=qx*Norm;
				qy=qy*Norm;
			}
		}
		break;
	case 3 :
		{
			xa=0;
			ya=-1/(f*C2);
			x=R2;
			y=ya+f*L2;
			Quotient();
			inverse(&x,&y);
			xa=x+1/R;
			ya=y+f*C-1/(f*L);
			inverse(&xa,&ya);
			x=xa+Rdc;
			y=ya+f*Lsp;
			Quotient();
			xa=1;
			ya=0;
			x=1;
			y=-1/(f*StrahlC);
			Quotient(); //Strahlungswiederstand
			qx=qx*Norm;
			qy=qy*Norm;
		}
	}		//switch
}

void driver::Quotient(void)

{
	double a;
	double b;
	double hilfe;

	hilfe = pow(x,2.0)+pow(y,2.0);
	if (x == xa)
	{
		a = (pow(x,2.0)+ya*y) / hilfe;
		b = x*(ya-y) / hilfe;
	}
	else
	{
		if (y == ya)
		{
			a = (xa*x+ya*y) / hilfe;
			b = y*(x-xa) / hilfe;
		}
		else
		{
			a = (xa*x+ya*y) / hilfe;
			b = (-xa*y+x*ya) / hilfe;
		}
	}
	hilfe=qx;
	qx=qx*a-qy*b;
	qy=hilfe*b+qy*a;
}



int driver::Anzahlcheck(void)
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

void driver::Parallelberechnung(void)
{
	double a;
	double b;
	double invx;
	double invy;

	a = Unit[i+3];
	if (Unit[i+4]==0)
	{
		if (Unit[i+5]==0)
		{
			if (Unit[i+3]==0)
			{
				return;
			}
			else
			{
				b = 0;
			}
		}
		else
		{
			b = f*Unit[i+5];
		}
	}
	else
	{
		if (Unit[i+5]==0)
		{
			b = -1/(f*Unit[i+4]);
		}
		else
		{
			b = f*Unit[i+5] - 1/(f*Unit[i+4]);
		}
	}
	invx=x;
	invy=y;
	inverse(&invx,&invy);
	inverse(&a,&b);
	a=invx+a;
	b=invy+b;
	inverse(&a,&b);
	x=a;
	y=b;
}

void driver::Reihenberechnung(void)
{
	double a,b;
	if (Unit[i+1]==0)
    {
		if (Unit[i+2]==0)
		{
			x = x + Unit[i];
		}
		else
		{
			x = x + Unit[i];
			y = y + f*Unit[i+2];
		}
    }
	else
	{
		if (Unit[i+2]==0)
		{
			b = f*Unit[i+1];
		}
		else
		{
			b = f*Unit[i+1] - 1/(f*Unit[i+2]);
		}
		if (Unit[i]==0)
		{
			y = y + (-1 / b);
		}
		else
		{
			a = 1 / Unit[i];
			inverse(&a,&b);
			x = x+a;
			y = y+b;
		}
	}
}

void driver::invertImpedanz(void)
{
	for (int i=0;i<300;i=i+2)
	{
		inverse(&ResultImpedanz[i],&ResultImpedanz[i+1]);
	}
}

QString driver::GetTitle()
{
	return m_qstringTitle;
}

void driver::SetTitle( const QString& a_qstringTitle )
{
	m_qstringTitle = a_qstringTitle;
}
/** Sets Rdc value */
void driver::setRdc(double rdc){
Rdc = rdc;
}
/** Sets Lsp value */
void driver::setLsp(double lsp){
Lsp = lsp;
}
/** Sets F0 value*/
void driver::setF0(double f0){
F0 = f0;
}
/** Sets Qtc value */
void driver::setQtc(double qtc){
Qtc = qtc;
}
/** Sets Qes value */
void driver::setQes(double qes){
Qe = qes;
}
/** Sets the full circuit flag */
void driver::setFullCircuit(bool toggle){
if (toggle)
  Realschall_flag = 1;
    else
      Realschall_flag = 0;
}
/** Gives the full circuit flag */
bool driver::getFullCircuit(){
return (Realschall_flag == 1);
}
/** Sets Qms value */
void driver::setQms(double qms){
Qms = qms;
}
/** Sets Vas value */
void driver::setVas(double vas){
Vas = vas;
}
/** Sets Dm value */
void driver::setDm(double dm){
Dm = dm;
}
/** No descriptions */
double driver::getRdc(){
return Rdc;
}
/** No descriptions */
double driver::getLsp(){
return Lsp;
}
/** No descriptions */
double driver::getF0(){
return F0;
}
/** No descriptions */
double driver::getQtc(){
return Qtc;
}
/** No descriptions */
double driver::getQes(){
return Qe;
}
/** No descriptions */
double driver::getQms(){
return Qms;
}
/** No descriptions */
double driver::getVas(){
return Vas;
}
/** No descriptions */
double driver::getDm(){
return Dm;
}

/** Sets network units values */
void driver::setUnit(int unit, double val){
if ((unit>-1)&&(unit<49))
	{
	Unit[unit] = val;
  this->setmodified();
	}
}

/** No descriptions */
double driver::getUnit(int unit){
	if ((unit>-1)&&(unit<49))
	return Unit[unit]; else
	return -1;
}

void driver::cleanupNetwork(void){
	for ( int intI = 0; intI < 49; intI++ )
		Unit[intI] = 0;
}

