/***************************************************************************
                          driver.h  -  description
                             -------------------
    begin                : Son Jul 28 2002
    copyright            : (C) 2002 by Martin Erdtmann
    email                : martin.erdtmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DRIVER_H
#define DRIVER_H

/**
*the base class which provides mathematical routines for the KFilter App.
*@author Martin Erdtmann
*/

#include <QString>

class driver {
public:
	driver();
	~driver();

	double Vb,Fb,F3,Ql,Fs,V2,gain;

	double	ResultSchall[300],ResultImpedanz[300],Unit[49];
	int		GTyp,GTypProposal,Phase_flag,Parameter_flag,Tiefpass_flag,AkustikESB_flag,Realschall_flag,
		Anzahl,i;
	bool 	PressureisActive,ImpedanzisActive,SummaryisActive,ScalarSummaryisActive,
		ImpedanzSummaryisActive,InvertPhase;

	void setmodified(void);
	void initContents(void);

	void Schall(void);
	void Impedanz (void);
	void Berechneparameter(void);
	void invertImpedanz(void);
	void cleanupNetwork(void);

	QString	GetTitle();
	void	SetTitle( const QString& a_qstringTitle );
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
  void setVas(double vas);
  /** No descriptions */
  void setQms(double qms);
  /** No descriptions */
  void setQes(double qes);
  /** Sets the full circuit flag */
  void setFullCircuit(bool toggle);
  /** Gets the full circuit flag */
  bool getFullCircuit(void);
  /** No descriptions */
  double getDm();
  /** No descriptions */
  double getVas();
  /** No descriptions */
  double getQms();
  /** No descriptions */
  double getQes();
  /** No descriptions */
  double getQtc();
  /** No descriptions */
  double getF0();
  /** No descriptions */
  double getLsp();
  /** No descriptions */
  double getRdc();
  /** Sets network unit values */
  void setUnit(int unit, double val);
  /** No descriptions */
  double getUnit(int unit);
private:

	void ESBberechnen(void);
	void Akustik(void);
	void Quotient(void);
	void inverse(double *a,double *b);
	int  Anzahlcheck(void);
	void Parallelberechnung(void);
	void Reihenberechnung(void);

	bool dirty_schall,dirty_impedanz,show_reflex_only;

	double
		Rdc,Lsp,F0,Qtc,Qms,Qe,Dm,Vas,
		//    Anpassung,Ausgleichsfaktor,Versatz,
		Consta,Constb,Constc,Constd,AktivC1,
		AktivC2,AktivL1,AktivL2,AktivF,TiefpassL,TiefpassC,TiefpassQ,Tiefpassfc,
		AktivQ,MembranDm,StrahlC,Norm,calibrate,Faktor,
		f,qx,qy,x,y,xa,ya, //Real und Im-Teile zur Laufzeit
		SystemQ,C2,L2,R2,C,L,R,Cakustik,Lakustik;

	QString	m_qstringTitle;
};

#endif
