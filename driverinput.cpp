/***************************************************************************
driverinput.cpp  -  description
-------------------
begin                : Sun May 7 2000
copyright            : (C) 2001 by Martin Erdtmann  /  Stefan Okrongli
email                : martin.erdtmann@gmx.de  /  s_okrongli@gmx.net
***************************************************************************/

/***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "driverinput.h"
#include <kapp.h>
#include <klocale.h>
#include <kstddirs.h>
#include <qlabel.h>
#include <stdio.h>
#include <stdlib.h>
#include <qpainter.h>
#include <qpixmap.h>
#include <qgroupbox.h>
#include <kmessagebox.h>

#include <qmessagebox.h>

driverinput::driverinput(const char *name = 0, driver *driverptr0,driver *driverptr1,driver *driverptr2,driver *driverptr3)
{
	drv0 = driverptr0;
	drv1 = driverptr1;
	drv2 = driverptr2;
	drv3 = driverptr3;
	initValues();
	this->setFixedSize(410,320);

	//QString path = kapp->kde_icondir();
	//path.append("/driverparam.xpm");
	QString path = locate ("data", "kfilter/pics/driverparam.xpm");

	setCaption(name);

	QWidget* firstdriver = new QWidget( this );
	firstdriver->setFixedSize(400,280); //250
	firstdriver->setGeometry(0,0,200,60);

	QLineEdit* Rdc = new QLineEdit(firstdriver);

	QLabel* label_Rdc = new QLabel("Rdc",firstdriver);
	label_Rdc->setGeometry(10,15,30,20);

	QLabel* label_Rdcx = new QLabel("Ohm",firstdriver);
	label_Rdcx->setGeometry(95,15,30,20);

	Rdc->setGeometry(40,15,50,20);
	sprintf (text0[0], "%6.2f", drv0->getRdc());
	Rdc->setText(text0[0]);
	QObject::connect(Rdc, SIGNAL(textChanged(const QString&) ), this, SLOT( Rdc_0(const QString&) ) );
	QLineEdit* Lsp = new QLineEdit(firstdriver);
	QLabel* label_Lsp = new QLabel("Lsp",firstdriver);
	label_Lsp->setGeometry(10,45,30,20);
	QLabel* label_Lspx = new QLabel("mH",firstdriver);label_Lspx->setGeometry(95,45,20,20);
	Lsp->setGeometry(40,45,50,20);
	sprintf (text0[1], "%6.2f", drv0->getLsp()*1000);
	Lsp->setText(text0[1]);
	QObject::connect(Lsp, SIGNAL(textChanged(const QString&) ), this, SLOT( Lsp_0(const QString&) ) );
	QLineEdit* F0 = new QLineEdit(firstdriver);
	QLabel* label_F0 = new QLabel("F0",firstdriver);
	label_F0->setGeometry(10,75,30,20);
	QLabel* label_F0x = new QLabel("Hz",firstdriver);label_F0x->setGeometry(95,75,30,20);
	F0->setGeometry(40,75,50,20);
	sprintf (text0[2], "%5.0f", drv0->getF0());
	F0->setText(text0[2]);
	QObject::connect(F0, SIGNAL(textChanged(const QString&) ), this, SLOT( F0_0(const QString&) ) );
	QLineEdit* Qtc = new QLineEdit(firstdriver);pQtc[0]=Qtc;
	QLabel* label_Qtc = new QLabel("Qts",firstdriver);
	label_Qtc->setGeometry(10,105,30,20);
	Qtc->setGeometry(40,105,50,20);
	sprintf (text0[3], "%6.2f", drv0->getQtc());
	Qtc->setText(text0[3]);
	QObject::connect(Qtc, SIGNAL(textChanged(const QString&) ), this, SLOT( Qtc_0(const QString&) ) );
	QLineEdit* Qes = new QLineEdit(firstdriver);pQes[0]=Qes;
	QLabel* label_Qes = new QLabel("Qes",firstdriver);
	label_Qes->setGeometry(10,135,30,20);
	Qes->setGeometry(40,135,50,20);
	sprintf (text0[4], "%6.2f", drv0->getQes());
	Qes->setText(text0[4]);
	QObject::connect(Qes, SIGNAL(textChanged(const QString&) ), this, SLOT( Qes_0(const QString&) ) );
	QLineEdit* Qms = new QLineEdit(firstdriver);pQms[0]=Qms;
	QLabel* label_Qms = new QLabel("Qms",firstdriver);
	label_Qms->setGeometry(10,165,30,20);
	Qms->setGeometry(40,165,50,20);
	sprintf (text0[5], "%6.2f", drv0->getQms());
	Qms->setText(text0[5]);
	QObject::connect(Qms, SIGNAL(textChanged(const QString&) ), this, SLOT( Qms_0(const QString&) ) );
	QLineEdit* Vas = new QLineEdit(firstdriver);
	QLabel* label_Vas = new QLabel("Vas",firstdriver);
	label_Vas->setGeometry(10,195,30,20);
	QLabel* label_Vasx = new QLabel("L",firstdriver);label_Vasx->setGeometry(95,195,30,20);
	Vas->setGeometry(40,195,50,20);
	sprintf (text0[6], "%6.2f", drv0->getVas());
	Vas->setText(text0[6]);
	QObject::connect(Vas, SIGNAL(textChanged(const QString&) ), this, SLOT( Vas_0(const QString&) ) );
	QLineEdit* Dm  = new QLineEdit(firstdriver);
	QLabel* label_Dm = new QLabel("Dm",firstdriver);
	label_Dm->setGeometry(10,225,30,20);
	QLabel* label_Dmx = new QLabel("cm",firstdriver);label_Dmx->setGeometry(95,225,30,20);
	Dm ->setGeometry(40,225,50,20);
	sprintf (text0[7], "%6.2f", drv0->getDm());
	Dm->setText(text0[7]);
	QObject::connect(Dm, SIGNAL(textChanged(const QString&) ), this, SLOT( Dm_0(const QString&) ) );
	/////////////////////////////////////
	QGroupBox* box = new QGroupBox(firstdriver);
	box->setGeometry(130,15,260,235);  // ... 205);
	box->setTitle(i18n("Options"));

	QSlider* slider0 = new QSlider(0,24,1,-20*log10(drv0->gain)+12,QSlider::Vertical,firstdriver);
	slider0->setGeometry(360,90,13,115);
	QLabel* slabel0 = new QLabel("Gain",firstdriver);slabel0->setGeometry(325,135,35,20);
	QLabel* max0 = new QLabel("+12dB",firstdriver);max0->setGeometry(315,90,40,20);
	QLabel* min0 = new QLabel("-12dB",firstdriver);min0->setGeometry(315,180,40,20);
	QObject::connect(slider0, SIGNAL(valueChanged(int) ), this, SLOT( slot_gain0(int) ) );

	QCheckBox* Pressure0 = new QCheckBox(firstdriver);PressurePointer[0]=Pressure0;
	Pressure0->setChecked(activePressure[0]);
	Pressure0->setGeometry(140,35,20,20);
	QLabel* label_enabled0 = new QLabel(i18n("Activate pressure"),firstdriver);
	label_enabled0->setGeometry(160,35,120,20);

	QCheckBox* Impedanz0 = new QCheckBox(firstdriver);ImpedanzPointer[0]=Impedanz0;
	Impedanz0->setChecked(activeImpedanz[0]);
	Impedanz0->setGeometry(140,65,20,20);
	QLabel* label_Impedanz0 = new QLabel(i18n("Activate Impedance"),firstdriver);
	label_Impedanz0->setGeometry(160,65,120,20);

	QCheckBox* Summary0 = new QCheckBox(firstdriver);SummaryPointer[0]=Summary0;
	Summary0->setChecked(activeSummary[0]);
	Summary0->setGeometry(140,95,20,20);
	QLabel* label_Summary0 = new QLabel(i18n("Add to summary"),firstdriver);
	label_Summary0->setGeometry(160,95,120,20);

	QCheckBox* ScalarSummary0 = new QCheckBox(firstdriver);ScalarSummaryPointer[0]=ScalarSummary0;
	ScalarSummary0->setChecked(activeScalarSummary[0]);
	ScalarSummary0->setGeometry(140,125,20,20);
	QLabel* label_ScalarSummary0 = new QLabel(i18n("Add to scalar sum"),firstdriver);
	label_ScalarSummary0->setGeometry(160,125,120,20);

	QCheckBox* ImpedanzSummary0 = new QCheckBox(firstdriver);ImpedanzSummaryPointer[0]=ImpedanzSummary0;
	ImpedanzSummary0->setChecked(activeImpedanzSummary[0]);
	ImpedanzSummary0->setGeometry(140,155,20,20);
	QLabel* label_ImpedanzSummary0 = new QLabel(i18n("Add to impedance sum"),firstdriver);
	label_ImpedanzSummary0->setGeometry(160,155,135,20);

	QCheckBox* InvertPhase0 = new QCheckBox(firstdriver);InvertPhasePointer[0]=InvertPhase0;
	InvertPhase0->setChecked(invertPhase[0]);
	InvertPhase0->setGeometry(140,185,20,20);
	QLabel* label_InvertPhase0 = new QLabel(i18n("Invert Phase"),firstdriver);
	label_InvertPhase0->setGeometry(160,185,120,20);

  QCheckBox* ToggleFullCircuit0 = new QCheckBox(firstdriver); ToggleFullCircuitPointer[0]=ToggleFullCircuit0;
  ToggleFullCircuit0->setChecked(toggleFullCircuit[0]);
  ToggleFullCircuit0->setGeometry(140,215,20,20);
  QLabel* label_ToggleFullCircuit0 = new QLabel(i18n("Toggle full circuit design"),firstdriver);
  label_ToggleFullCircuit0->setGeometry(160,215,170,20);

	QLabel* picture0 = new QLabel(firstdriver);
	picture0->setGeometry(300,35,80,50);
	picture0->setPixmap(QPixmap(path));

	QLabel* m_label0 = new QLabel(i18n("Driver Title :"),firstdriver);
	m_label0 -> setGeometry(130,255,100,20);

	QLineEdit* m_title0 = new QLineEdit(firstdriver);pDriverTitle[0]=m_title0;
	m_title0->setGeometry(208,255,180,20);
	m_title0->setText((const char*)drv0->GetTitle());

	addTab (firstdriver, i18n("Driver 1"));

	//////////////////////////////////////////////////////////////////////////////////////////////

	QWidget* seconddriver = new QWidget( this );
	seconddriver->setFixedSize(400,280);
	seconddriver->setGeometry(0,0,200,60);
	QLineEdit* Rdc2 = new QLineEdit(seconddriver);
	QLabel* label_Rdc2 = new QLabel("Rdc",seconddriver);
	label_Rdc2->setGeometry(10,15,30,20);
	QLabel* label_Rdc2x = new QLabel("Ohm",seconddriver);
	label_Rdc2x->setGeometry(95,15,30,20);
	Rdc2->setGeometry(40,15,50,20);
	sprintf (text1[0], "%6.2f", drv1->getRdc());
	Rdc2->setText(text1[0]);
	QObject::connect(Rdc2, SIGNAL(textChanged(const QString&) ), this, SLOT( Rdc_1(const QString&) ) );
	QLineEdit* Lsp2 = new QLineEdit(seconddriver);
	QLabel* label_Lsp2 = new QLabel("Lsp",seconddriver);
	label_Lsp2->setGeometry(10,45,30,20);
	QLabel* label_Lsp2x = new QLabel("mH",seconddriver);label_Lsp2x->setGeometry(95,45,20,20);
	Lsp2->setGeometry(40,45,50,20);
	sprintf (text1[1], "%6.2f", drv1->getLsp()*1000);
	Lsp2->setText(text1[1]);
	QObject::connect(Lsp2, SIGNAL(textChanged(const QString&) ), this, SLOT( Lsp_1(const QString&) ) );
	QLineEdit* F02 = new QLineEdit(seconddriver);
	QLabel* label_F02 = new QLabel("F0",seconddriver);
	label_F02->setGeometry(10,75,30,20);
	QLabel* label_F02x = new QLabel("Hz",seconddriver);label_F02x->setGeometry(95,75,30,20);
	F02->setGeometry(40,75,50,20);
	sprintf (text1[2], "%5.0f", drv1->getF0());
	F02->setText(text1[2]);
	QObject::connect(F02, SIGNAL(textChanged(const QString&) ), this, SLOT( F0_1(const QString&) ) );
	QLineEdit* Qtc2 = new QLineEdit(seconddriver);pQtc[1]=Qtc2;
	QLabel* label_Qtc2 = new QLabel("Qts",seconddriver);
	label_Qtc2->setGeometry(10,105,30,20);
	Qtc2->setGeometry(40,105,50,20);
	sprintf (text1[3], "%6.2f", drv1->getQtc());
	Qtc2->setText(text1[3]);
	QObject::connect(Qtc2, SIGNAL(textChanged(const QString&) ), this, SLOT( Qtc_1(const QString&) ) );
	QLineEdit* Qes2 = new QLineEdit(seconddriver);pQes[1]=Qes2;
	QLabel* label_Qes2 = new QLabel("Qes",seconddriver);
	label_Qes2->setGeometry(10,135,30,20);
	Qes2->setGeometry(40,135,50,20);
	sprintf (text1[4], "%6.2f", drv1->getQes());
	Qes2->setText(text1[4]);
	QObject::connect(Qes2, SIGNAL(textChanged(const QString&) ), this, SLOT( Qes_1(const QString&) ) );
	QLineEdit* Qms2 = new QLineEdit(seconddriver);pQms[1]=Qms2;
	QLabel* label_Qms2 = new QLabel("Qms",seconddriver);
	label_Qms2->setGeometry(10,165,30,20);
	Qms2->setGeometry(40,165,50,20);
	sprintf (text1[5], "%6.2f", drv1->getQms());
	Qms2->setText(text1[5]);
	QObject::connect(Qms2, SIGNAL(textChanged(const QString&) ), this, SLOT( Qms_1(const QString&) ) );
	QLineEdit* Vas2 = new QLineEdit(seconddriver);
	QLabel* label_Vas2 = new QLabel("Vas",seconddriver);
	label_Vas2->setGeometry(10,195,30,20);
	QLabel* label_Vas2x = new QLabel("L",seconddriver);label_Vas2x->setGeometry(95,195,30,20);
	Vas2->setGeometry(40,195,50,20);
	sprintf (text1[6], "%6.2f", drv1->getVas());
	Vas2->setText(text1[6]);
	QObject::connect(Vas2, SIGNAL(textChanged(const QString&) ), this, SLOT( Vas_1(const QString&) ) );
	QLineEdit* Dm2  = new QLineEdit(seconddriver);
	QLabel* label_Dm2 = new QLabel("Dm",seconddriver);
	label_Dm2->setGeometry(10,225,30,20);
	QLabel* label_Dm2x = new QLabel("cm",seconddriver);label_Dm2x->setGeometry(95,225,30,20);
	Dm2 ->setGeometry(40,225,50,20);
	sprintf (text1[7], "%6.2f", drv1->getDm());
	Dm2->setText(text1[7]);
	QObject::connect(Dm2, SIGNAL(textChanged(const QString&) ), this, SLOT( Dm_1(const QString&) ) );
	////////////////////////////
	QGroupBox* box2 = new QGroupBox(seconddriver);
	box2->setGeometry(130,15,260,235);
	box2->setTitle(i18n("Options"));

	QSlider* slider1 = new QSlider(0,24,1,-20*log10(drv1->gain)+12,QSlider::Vertical,seconddriver);
	slider1->setGeometry(360,90,13,115);
	QLabel* slabel1 = new QLabel("Gain",seconddriver);slabel1->setGeometry(325,135,35,20);
	QLabel* max1 = new QLabel("+12dB",seconddriver);max1->setGeometry(315,90,40,20);
	QLabel* min1 = new QLabel("-12dB",seconddriver);min1->setGeometry(315,180,40,20);
	QObject::connect(slider1, SIGNAL(valueChanged(int) ), this, SLOT( slot_gain1(int) ) );

	QCheckBox* Pressure1 = new QCheckBox(seconddriver);PressurePointer[1]=Pressure1;
	Pressure1->setChecked(activePressure[1]);
	Pressure1->setGeometry(140,35,20,20);
	QLabel* label_enabled1 = new QLabel(i18n("Activate pressure"),seconddriver);
	label_enabled1->setGeometry(160,35,110,20);

	QCheckBox* Impedanz1 = new QCheckBox(seconddriver);ImpedanzPointer[1]=Impedanz1;
	Impedanz1->setChecked(activeImpedanz[1]);
	Impedanz1->setGeometry(140,65,20,20);
	QLabel* label_Impedanz1 = new QLabel(i18n("Activate Impedance"),seconddriver);
	label_Impedanz1->setGeometry(160,65,120,20);

	QCheckBox* Summary1 = new QCheckBox(seconddriver);SummaryPointer[1]=Summary1;
	Summary1->setChecked(activeSummary[1]);
	Summary1->setGeometry(140,95,20,20);
	QLabel* label_Summary1 = new QLabel(i18n("Add to summary"),seconddriver);
	label_Summary1->setGeometry(160,95,120,20);

	QCheckBox* ScalarSummary1 = new QCheckBox(seconddriver);ScalarSummaryPointer[1]=ScalarSummary1;
	ScalarSummary1->setChecked(activeScalarSummary[1]);
	ScalarSummary1->setGeometry(140,125,20,20);
	QLabel* label_ScalarSummary1 = new QLabel(i18n("Add to scalar sum"),seconddriver);
	label_ScalarSummary1->setGeometry(160,125,120,20);

	QCheckBox* ImpedanzSummary1 = new QCheckBox(seconddriver);ImpedanzSummaryPointer[1]=ImpedanzSummary1;
	ImpedanzSummary1->setChecked(activeImpedanzSummary[1]);
	ImpedanzSummary1->setGeometry(140,155,20,20);
	QLabel* label_ImpedanzSummary1 = new QLabel(i18n("Add to impedance sum"),seconddriver);
	label_ImpedanzSummary1->setGeometry(160,155,135,20);

	QCheckBox* InvertPhase1 = new QCheckBox(seconddriver);InvertPhasePointer[1]=InvertPhase1;
	InvertPhase1->setChecked(invertPhase[1]);
	InvertPhase1->setGeometry(140,185,20,20);
	QLabel* label_InvertPhase1 = new QLabel(i18n("Invert Phase"),seconddriver);
	label_InvertPhase1->setGeometry(160,185,120,20);

  QCheckBox* ToggleFullCircuit1 = new QCheckBox(seconddriver); ToggleFullCircuitPointer[1]=ToggleFullCircuit1;
  ToggleFullCircuit1->setChecked(toggleFullCircuit[1]);
  ToggleFullCircuit1->setGeometry(140,215,20,20);
  QLabel* label_ToggleFullCircuit1 = new QLabel(i18n("Toggle full circuit design"),seconddriver);
  label_ToggleFullCircuit1->setGeometry(160,215,170,20);

	QLabel* picture1 = new QLabel(seconddriver);
	picture1->setGeometry(300,35,80,50);
	picture1->setPixmap(QPixmap(path));

	QLabel* m_label1 = new QLabel(i18n("Driver Title :"),seconddriver);
	m_label1 -> setGeometry(130,255,100,20);

	QLineEdit* m_title1 = new QLineEdit(seconddriver);pDriverTitle[1]=m_title1;
	m_title1->setGeometry(208,255,180,20);
	m_title1->setText((const char*)drv1->GetTitle());

	addTab (seconddriver, i18n("Driver 2"));
	///////////////////////////////////////////////////////////////////////////////////////////
	QWidget* thirddriver = new QWidget( this );
	seconddriver->setFixedSize(400,280);
	seconddriver->setGeometry(0,0,200,60);
	QLineEdit* Rdc3 = new QLineEdit(thirddriver);
	QLabel* label_Rdc3 = new QLabel("Rdc",thirddriver);
	label_Rdc3->setGeometry(10,15,30,20);
	QLabel* label_Rdc3x = new QLabel("Ohm",thirddriver);
	label_Rdc3x->setGeometry(95,15,30,20);
	Rdc3->setGeometry(40,15,50,20);
	sprintf (text2[0], "%6.2f", drv2->getRdc());
	Rdc3->setText(text2[0]);
	QObject::connect(Rdc3, SIGNAL(textChanged(const QString&) ), this, SLOT( Rdc_2(const QString&) ) );
	QLineEdit* Lsp3 = new QLineEdit(thirddriver);
	QLabel* label_Lsp3 = new QLabel("Lsp",thirddriver);
	label_Lsp3->setGeometry(10,45,30,20);
	QLabel* label_Lsp3x = new QLabel("mH",thirddriver);label_Lsp3x->setGeometry(95,45,20,20);
	Lsp3->setGeometry(40,45,50,20);
	sprintf (text2[1], "%6.2f", drv2->getLsp()*1000);
	Lsp3->setText(text2[1]);
	QObject::connect(Lsp3, SIGNAL(textChanged(const QString&) ), this, SLOT( Lsp_2(const QString&) ) );
	QLineEdit* F03 = new QLineEdit(thirddriver);
	QLabel* label_F03 = new QLabel("F0",thirddriver);
	label_F03->setGeometry(10,75,30,20);
	QLabel* label_F03x = new QLabel("Hz",thirddriver);label_F03x->setGeometry(95,75,30,20);
	F03->setGeometry(40,75,50,20);
	sprintf (text2[2], "%5.0f", drv2->getF0());
	F03->setText(text2[2]);
	QObject::connect(F03, SIGNAL(textChanged(const QString&) ), this, SLOT( F0_2(const QString&) ) );
	QLineEdit* Qtc3 = new QLineEdit(thirddriver);pQtc[2]=Qtc3;
	QLabel* label_Qtc3 = new QLabel("Qts",thirddriver);
	label_Qtc3->setGeometry(10,105,30,20);
	Qtc3->setGeometry(40,105,50,20);
	sprintf (text2[3], "%6.2f", drv2->getQtc());
	Qtc3->setText(text2[3]);
	QObject::connect(Qtc3, SIGNAL(textChanged(const QString&) ), this, SLOT( Qtc_2(const QString&) ) );

	QLineEdit* Qes3 = new QLineEdit(thirddriver);pQes[2]=Qes3;
	QLabel* label_Qes3 = new QLabel("Qes",thirddriver);
	label_Qes3->setGeometry(10,135,30,20);
	Qes3->setGeometry(40,135,50,20);
	sprintf (text2[4], "%6.2f", drv2->getQes());
	Qes3->setText(text2[4]);
	QObject::connect(Qes3, SIGNAL(textChanged(const QString&) ), this, SLOT( Qes_2(const QString&) ) );
	QLineEdit* Qms3 = new QLineEdit(thirddriver);pQms[2]=Qms3;
	QLabel* label_Qms3 = new QLabel("Qms",thirddriver);
	label_Qms3->setGeometry(10,165,30,20);
	Qms3->setGeometry(40,165,50,20);
	sprintf (text2[5], "%6.2f", drv2->getQms());
	Qms3->setText(text2[5]);
	QObject::connect(Qms3, SIGNAL(textChanged(const QString&) ), this, SLOT( Qms_2(const QString&) ) );
	QLineEdit* Vas3 = new QLineEdit(thirddriver);
	QLabel* label_Vas3 = new QLabel("Vas",thirddriver);
	label_Vas3->setGeometry(10,195,30,20);
	QLabel* label_Vas3x = new QLabel("L",thirddriver);label_Vas3x->setGeometry(95,195,30,20);
	Vas3->setGeometry(40,195,50,20);
	sprintf (text2[6], "%6.2f", drv2->getVas());
	Vas3->setText(text2[6]);
	QObject::connect(Vas3, SIGNAL(textChanged(const QString&) ), this, SLOT( Vas_2(const QString&) ) );
	QLineEdit* Dm3  = new QLineEdit(thirddriver);
	QLabel* label_Dm3 = new QLabel("Dm",thirddriver);
	label_Dm3->setGeometry(10,225,30,20);
	QLabel* label_Dm3x = new QLabel("cm",thirddriver);label_Dm3x->setGeometry(95,225,30,20);
	Dm3 ->setGeometry(40,225,50,20);
	sprintf (text2[7], "%6.2f", drv2->getDm());
	Dm3->setText(text2[7]);
	QObject::connect(Dm3, SIGNAL(textChanged(const QString&) ), this, SLOT( Dm_2(const QString&) ) );
	//////////////////////////////////
	QGroupBox* box3 = new QGroupBox(thirddriver);
	box3->setGeometry(130,15,260,235);
	box3->setTitle(i18n("Options"));

	QSlider* slider2 = new QSlider(0,24,1,-20*log10(drv2->gain)+12,QSlider::Vertical,thirddriver);
	slider2->setGeometry(360,90,13,115);
	QLabel* slabel2 = new QLabel("Gain",thirddriver);slabel2->setGeometry(325,135,35,20);
	QLabel* max2 = new QLabel("+12dB",thirddriver);max2->setGeometry(315,90,40,20);
	QLabel* min2 = new QLabel("-12dB",thirddriver);min2->setGeometry(315,180,40,20);
	QObject::connect(slider2, SIGNAL(valueChanged(int) ), this, SLOT( slot_gain2(int) ) );

	QCheckBox* Pressure2 = new QCheckBox(thirddriver);PressurePointer[2]=Pressure2;
	Pressure2->setChecked(activePressure[2]);
	Pressure2->setGeometry(140,35,20,20);
	QLabel* label_enabled2 = new QLabel(i18n("Activate pressure"),thirddriver);
	label_enabled2->setGeometry(160,35,120,20);

	QCheckBox* Impedanz2 = new QCheckBox(thirddriver);ImpedanzPointer[2]=Impedanz2;
	Impedanz2->setChecked(activeImpedanz[2]);
	Impedanz2->setGeometry(140,65,20,20);
	QLabel* label_Impedanz2 = new QLabel(i18n("Activate Impedance"),thirddriver);
	label_Impedanz2->setGeometry(160,65,120,20);

	QCheckBox* Summary2 = new QCheckBox(thirddriver);SummaryPointer[2]=Summary2;
	Summary2->setChecked(activeSummary[2]);
	Summary2->setGeometry(140,95,20,20);
	QLabel* label_Summary2 = new QLabel(i18n("Add to summary"),thirddriver);
	label_Summary2->setGeometry(160,95,120,20);

	QCheckBox* ScalarSummary2 = new QCheckBox(thirddriver);ScalarSummaryPointer[2]=ScalarSummary2;
	ScalarSummary2->setChecked(activeScalarSummary[2]);
	ScalarSummary2->setGeometry(140,125,20,20);
	QLabel* label_ScalarSummary2 = new QLabel(i18n("Add to scalar sum"),thirddriver);
	label_ScalarSummary2->setGeometry(160,125,120,20);

	QCheckBox* ImpedanzSummary2 = new QCheckBox(thirddriver);ImpedanzSummaryPointer[2]=ImpedanzSummary2;
	ImpedanzSummary2->setChecked(activeImpedanzSummary[2]);
	ImpedanzSummary2->setGeometry(140,155,20,20);
	QLabel* label_ImpedanzSummary2 = new QLabel(i18n("Add to impedance sum"),thirddriver);
	label_ImpedanzSummary2->setGeometry(160,155,135,20);

	QCheckBox* InvertPhase2 = new QCheckBox(thirddriver);InvertPhasePointer[2]=InvertPhase2;
	InvertPhase2->setChecked(invertPhase[2]);
	InvertPhase2->setGeometry(140,185,20,20);
	QLabel* label_InvertPhase2 = new QLabel(i18n("Invert Phase"),thirddriver);
	label_InvertPhase2->setGeometry(160,185,120,20);

  QCheckBox* ToggleFullCircuit2 = new QCheckBox(thirddriver); ToggleFullCircuitPointer[2]=ToggleFullCircuit2;
  ToggleFullCircuit2->setChecked(toggleFullCircuit[2]);
  ToggleFullCircuit2->setGeometry(140,215,20,20);
  QLabel* label_ToggleFullCircuit2 = new QLabel(i18n("Toggle full circuit design"),thirddriver);
  label_ToggleFullCircuit2->setGeometry(160,215,170,20);

	QLabel* picture2 = new QLabel(thirddriver);
	picture2->setGeometry(300,35,80,50);
	picture2->setPixmap(QPixmap(path));

	QLabel* m_label2 = new QLabel(i18n("Driver Title :"),thirddriver);
	m_label2 -> setGeometry(130,255,100,20);

	QLineEdit* m_title2 = new QLineEdit(thirddriver);pDriverTitle[2]=m_title2;
	m_title2->setGeometry(208,255,180,20);
	m_title2->setText((const char*)drv2->GetTitle());

	addTab (thirddriver, i18n("Driver 3"));
	///////////////////////////////////////////////////////////////////////////////////////////

	QWidget* fourthdriver = new QWidget( this );
	fourthdriver->setFixedSize(400,280);
	fourthdriver->setGeometry(0,0,200,60);
	QLineEdit* Rdc4 = new QLineEdit(fourthdriver);
	QLabel* label_Rdc4 = new QLabel("Rdc",fourthdriver);
	label_Rdc4->setGeometry(10,15,30,20);
	QLabel* label_Rdc4x = new QLabel("Ohm",fourthdriver);
	label_Rdc4x->setGeometry(95,15,30,20);
	Rdc4->setGeometry(40,15,50,20);
	sprintf (text3[0], "%6.2f", drv3->getRdc());
	Rdc4->setText(text3[0]);
	QObject::connect(Rdc4, SIGNAL(textChanged(const QString&) ), this, SLOT( Rdc_3(const QString&) ) );
	QLineEdit* Lsp4 = new QLineEdit(fourthdriver);
	QLabel* label_Lsp4 = new QLabel("Lsp",fourthdriver);
	label_Lsp4->setGeometry(10,45,30,20);
	QLabel* label_Lsp4x = new QLabel("mH",fourthdriver);label_Lsp4x->setGeometry(95,45,20,20);
	Lsp4->setGeometry(40,45,50,20);
	sprintf (text3[1], "%6.2f", drv3->getLsp()*1000);
	Lsp4->setText(text3[1]);
	QObject::connect(Lsp4, SIGNAL(textChanged(const QString&) ), this, SLOT( Lsp_3(const QString&) ) );
	QLineEdit* F04 = new QLineEdit(fourthdriver);
	QLabel* label_F04 = new QLabel("F0",fourthdriver);
	label_F04->setGeometry(10,75,30,20);
	QLabel* label_F04x = new QLabel("Hz",fourthdriver);label_F04x->setGeometry(95,75,30,20);
	F04->setGeometry(40,75,50,20);
	sprintf (text3[2], "%5.0f", drv3->getF0());
	F04->setText(text3[2]);
	QObject::connect(F04, SIGNAL(textChanged(const QString&) ), this, SLOT( F0_3(const QString&) ) );
	QLineEdit* Qtc4 = new QLineEdit(fourthdriver);pQtc[3]=Qtc4;
	QLabel* label_Qtc4 = new QLabel("Qts",fourthdriver);
	label_Qtc4->setGeometry(10,105,30,20);
	Qtc4->setGeometry(40,105,50,20);
	sprintf (text3[3], "%6.2f", drv3->getQtc());
	Qtc4->setText(text3[3]);
	QObject::connect(Qtc4, SIGNAL(textChanged(const QString&) ), this, SLOT( Qtc_3(const QString&) ) );
	QLineEdit* Qes4 = new QLineEdit(fourthdriver);pQes[3]=Qes4;
	QLabel* label_Qes4 = new QLabel("Qes",fourthdriver);
	label_Qes4->setGeometry(10,135,30,20);
	Qes4->setGeometry(40,135,50,20);
	sprintf (text3[4], "%6.2f", drv3->getQes());
	Qes4->setText(text3[4]);
	QObject::connect(Qes4, SIGNAL(textChanged(const QString&) ), this, SLOT( Qes_3(const QString&) ) );
	QLineEdit* Qms4 = new QLineEdit(fourthdriver);pQms[3]=Qms4;
	QLabel* label_Qms4 = new QLabel("Qms",fourthdriver);
	label_Qms4->setGeometry(10,165,30,20);
	Qms4->setGeometry(40,165,50,20);
	sprintf (text3[5], "%6.2f", drv3->getQms());
	Qms4->setText(text3[5]);
	QObject::connect(Qms4, SIGNAL(textChanged(const QString&) ), this, SLOT( Qms_3(const QString&) ) );
	QLineEdit* Vas4 = new QLineEdit(fourthdriver);
	QLabel* label_Vas4 = new QLabel("Vas",fourthdriver);
	label_Vas4->setGeometry(10,195,30,20);
	QLabel* label_Vas4x = new QLabel("L",fourthdriver);label_Vas4x->setGeometry(95,195,30,20);
	Vas4->setGeometry(40,195,50,20);
	sprintf (text3[6], "%6.2f", drv3->getVas());
	Vas4->setText(text3[6]);
	QObject::connect(Vas4, SIGNAL(textChanged(const QString&) ), this, SLOT( Vas_3(const QString&) ) );
	QLineEdit* Dm4  = new QLineEdit(fourthdriver);
	QLabel* label_Dm4 = new QLabel("Dm",fourthdriver);
	label_Dm4->setGeometry(10,225,30,20);
	QLabel* label_Dm4x = new QLabel("cm",fourthdriver);label_Dm4x->setGeometry(95,225,30,20);
	Dm4 ->setGeometry(40,225,50,20);
	sprintf (text3[7], "%6.2f", drv3->getDm());
	Dm4->setText(text3[7]);
	QObject::connect(Dm4, SIGNAL(textChanged(const QString&) ), this, SLOT( Dm_3(const QString&) ) );
	///////////////////////////////////////////////
	QGroupBox* box4 = new QGroupBox(fourthdriver);
	box4->setGeometry(130,15,260,235);
	box4->setTitle(i18n("Options"));

	QSlider* slider3 = new QSlider(0,24,1,-20*log10(drv3->gain)+12,QSlider::Vertical,fourthdriver);
	slider3->setGeometry(360,90,13,115);
	QLabel* slabel3 = new QLabel("Gain",fourthdriver);slabel3->setGeometry(325,135,35,20);
	QLabel* max3 = new QLabel("+12dB",fourthdriver);max3->setGeometry(315,90,40,20);
	QLabel* min3 = new QLabel("-12dB",fourthdriver);min3->setGeometry(315,180,40,20);
	QObject::connect(slider3, SIGNAL(valueChanged(int) ), this, SLOT( slot_gain3(int) ) );

	QCheckBox* Pressure3 = new QCheckBox(fourthdriver);PressurePointer[3]=Pressure3;
	Pressure3->setChecked(activePressure[3]);
	Pressure3->setGeometry(140,35,20,20);
	QLabel* label_enabled3 = new QLabel(i18n("Activate pressure"),fourthdriver);
	label_enabled3->setGeometry(160,35,120,20);

	QCheckBox* Impedanz3 = new QCheckBox(fourthdriver);ImpedanzPointer[3]=Impedanz3;
	Impedanz3->setChecked(activeImpedanz[3]);
	Impedanz3->setGeometry(140,65,20,20);
	QLabel* label_Impedanz3 = new QLabel(i18n("Activate Impedance"),fourthdriver);
	label_Impedanz3->setGeometry(160,65,120,20);

	QCheckBox* Summary3 = new QCheckBox(fourthdriver);SummaryPointer[3]=Summary3;
	Summary3->setChecked(activeSummary[3]);
	Summary3->setGeometry(140,95,20,20);
	QLabel* label_Summary3 = new QLabel(i18n("Add to summary"),fourthdriver);
	label_Summary3->setGeometry(160,95,120,20);

	QCheckBox* ScalarSummary3 = new QCheckBox(fourthdriver);ScalarSummaryPointer[3]=ScalarSummary3;
	ScalarSummary3->setChecked(activeScalarSummary[3]);
	ScalarSummary3->setGeometry(140,125,20,20);
	QLabel* label_ScalarSummary3 = new QLabel(i18n("Add to scalar sum"),fourthdriver);
	label_ScalarSummary3->setGeometry(160,125,120,20);

	QCheckBox* ImpedanzSummary3 = new QCheckBox(fourthdriver);ImpedanzSummaryPointer[3]=ImpedanzSummary3;
	ImpedanzSummary3->setChecked(activeImpedanzSummary[3]);
	ImpedanzSummary3->setGeometry(140,155,20,20);
	QLabel* label_ImpedanzSummary3 = new QLabel(i18n("Add to impedance sum"),fourthdriver);
	label_ImpedanzSummary3->setGeometry(160,155,135,20);

	QCheckBox* InvertPhase3 = new QCheckBox(fourthdriver);InvertPhasePointer[3]=InvertPhase3;
	InvertPhase3->setChecked(invertPhase[3]);
	InvertPhase3->setGeometry(140,185,20,20);
	QLabel* label_InvertPhase3 = new QLabel(i18n("Invert Phase"),fourthdriver);
	label_InvertPhase3->setGeometry(160,185,120,20);

  QCheckBox* ToggleFullCircuit3 = new QCheckBox(fourthdriver); ToggleFullCircuitPointer[3]=ToggleFullCircuit3;
  ToggleFullCircuit3->setChecked(toggleFullCircuit[3]);
  ToggleFullCircuit3->setGeometry(140,215,20,20);
  QLabel* label_ToggleFullCircuit3 = new QLabel(i18n("Toggle full circuit design"),fourthdriver);
  label_ToggleFullCircuit3->setGeometry(160,215,170,20);

	QLabel* picture3 = new QLabel(fourthdriver);
	picture3->setGeometry(300,35,80,50);
	picture3->setPixmap(QPixmap(path));

	QLabel* m_label3 = new QLabel(i18n("Driver Title :"),fourthdriver);
	m_label3 -> setGeometry(130,255,100,20);

	QLineEdit* m_title3 = new QLineEdit(fourthdriver);pDriverTitle[3]=m_title3;
	m_title3->setGeometry(208,255,180,20);
	m_title3->setText((const char*)drv3->GetTitle());

	addTab (fourthdriver, i18n("Driver 4"));

	setOKButton();
	setDefaultButton(i18n("Apply"));
	setCancelButton();

	QObject::connect(this, SIGNAL(applyButtonPressed() ), this, SLOT( applyClicked() ) );
	QObject::connect(this, SIGNAL(defaultButtonPressed() ), this, SLOT( defaultClicked() ) );
	QObject::connect(this, SIGNAL(cancelButtonPressed() ), this, SLOT( cancelClicked() ) );
}

driverinput::~driverinput()
{
}

void driverinput::applyClicked()
{
	// copy data from dialog into applicaiton
	updateValues();
	drv0->Berechneparameter();
	drv1->Berechneparameter();
	drv2->Berechneparameter();
	drv3->Berechneparameter();
	drv0->setmodified();
	drv1->setmodified();
	drv2->setmodified();
	drv3->setmodified();
	emit paramchanged();
	emit isclosed();
	accept();
}

void driverinput::cancelClicked()
{
	//do not copy data from the dialog into application
	reject();
	emit isclosed();
}

void driverinput::defaultClicked(){
	//restore the dialogs data
	updateValues();
	drv0->Berechneparameter();
	drv1->Berechneparameter();
	drv2->Berechneparameter();
	drv3->Berechneparameter();
	drv0->setmodified();
	drv1->setmodified();
	drv2->setmodified();
	drv3->setmodified();
	emit paramchanged();
}

void driverinput::initValues(){

	activePressure[0] = drv0->PressureisActive;
	activeImpedanz[0] = drv0->ImpedanzisActive;
	activeSummary[0] = drv0->SummaryisActive;
	activeScalarSummary[0] = drv0->ScalarSummaryisActive;
	activeImpedanzSummary[0] = drv0->ImpedanzSummaryisActive;
  invertPhase[0] = drv0->InvertPhase;
  toggleFullCircuit[0] = drv0->getFullCircuit();

	value0[0] = drv0->getRdc();
	value0[1] = drv0->getLsp();
	value0[2] = drv0->getF0();
	value0[3] = drv0->getQtc();
	value0[4] = drv0->getQes();
	value0[5] = drv0->getQms();
	value0[6] = drv0->getVas();
	value0[7] = drv0->getDm();

	activePressure[1] = drv1->PressureisActive;
	activeImpedanz[1] = drv1->ImpedanzisActive;
	activeSummary[1] = drv1->SummaryisActive;
	activeScalarSummary[1] = drv1->ScalarSummaryisActive;
	activeImpedanzSummary[1] = drv1->ImpedanzSummaryisActive;
	invertPhase[1] = drv1->InvertPhase;
  toggleFullCircuit[1] = drv1->getFullCircuit();

	value1[0] = drv1->getRdc();
	value1[1] = drv1->getLsp();
	value1[2] = drv1->getF0();
	value1[3] = drv1->getQtc();
	value1[4] = drv1->getQes();
	value1[5] = drv1->getQms();
	value1[6] = drv1->getVas();
	value1[7] = drv1->getDm();

	activePressure[2] = drv2->PressureisActive;
	activeImpedanz[2] = drv2->ImpedanzisActive;
	activeSummary[2] = drv2->SummaryisActive;
	activeScalarSummary[2] = drv2->ScalarSummaryisActive;
	activeImpedanzSummary[2] = drv2->ImpedanzSummaryisActive;
	invertPhase[2] = drv2->InvertPhase;
  toggleFullCircuit[2] = drv2->getFullCircuit();

	value2[0] = drv2->getRdc();
	value2[1] = drv2->getLsp();
	value2[2] = drv2->getF0();
	value2[3] = drv2->getQtc();
	value2[4] = drv2->getQes();
	value2[5] = drv2->getQms();
	value2[6] = drv2->getVas();
	value2[7] = drv2->getDm();

	activePressure[3] = drv3->PressureisActive;
	activeImpedanz[3] = drv3->ImpedanzisActive;
	activeSummary[3] = drv3->SummaryisActive;
	activeScalarSummary[3] = drv3->ScalarSummaryisActive;
	activeImpedanzSummary[3] = drv3->ImpedanzSummaryisActive;
	invertPhase[3] = drv3->InvertPhase;
  toggleFullCircuit[3] = drv3->getFullCircuit();

	value3[0] = drv3->getRdc();
	value3[1] = drv3->getLsp();
	value3[2] = drv3->getF0();
	value3[3] = drv3->getQtc();
	value3[4] = drv3->getQes();
	value3[5] = drv3->getQms();
	value3[6] = drv3->getVas();
	value3[7] = drv3->getDm();
}

void driverinput::updateValues()
{
	consistence();
	drv0->PressureisActive=PressurePointer[0]->isChecked();
	drv0->ImpedanzisActive=ImpedanzPointer[0]->isChecked();
	drv0->SummaryisActive=SummaryPointer[0]->isChecked();
	drv0->ScalarSummaryisActive=ScalarSummaryPointer[0]->isChecked();
	drv0->ImpedanzSummaryisActive=ImpedanzSummaryPointer[0]->isChecked();
	drv0->InvertPhase=InvertPhasePointer[0]->isChecked();
  drv0->setFullCircuit(ToggleFullCircuitPointer[0]->isChecked());
	drv0->setRdc(value0[0]);
	drv0->setLsp(value0[1]);
	drv0->setF0 (value0[2]);
	drv0->setQtc(value0[3]);
	drv0->setQes(value0[4]);
	drv0->setQms(value0[5]);
	drv0->setVas(value0[6]);
	drv0->setDm (value0[7]);
	drv0->SetTitle(pDriverTitle[0]->text());

	drv1->PressureisActive=PressurePointer[1]->isChecked();
	drv1->ImpedanzisActive=ImpedanzPointer[1]->isChecked();
	drv1->SummaryisActive=SummaryPointer[1]->isChecked();
	drv1->ScalarSummaryisActive=ScalarSummaryPointer[1]->isChecked();
	drv1->ImpedanzSummaryisActive=ImpedanzSummaryPointer[1]->isChecked();
	drv1->InvertPhase=InvertPhasePointer[1]->isChecked();
  drv1->setFullCircuit(ToggleFullCircuitPointer[1]->isChecked());
	drv1->setRdc(value1[0]);
	drv1->setLsp(value1[1]);
	drv1->setF0 (value1[2]);
	drv1->setQtc(value1[3]);
	drv1->setQes (value1[4]);
	drv1->setQms(value1[5]);
	drv1->setVas(value1[6]);
	drv1->setDm (value1[7]);
	drv1->SetTitle(pDriverTitle[1]->text());

	drv2->PressureisActive=PressurePointer[2]->isChecked();
	drv2->ImpedanzisActive=ImpedanzPointer[2]->isChecked();
	drv2->SummaryisActive=SummaryPointer[2]->isChecked();
	drv2->ScalarSummaryisActive=ScalarSummaryPointer[2]->isChecked();
	drv2->ImpedanzSummaryisActive=ImpedanzSummaryPointer[2]->isChecked();
	drv2->InvertPhase=InvertPhasePointer[2]->isChecked();
  drv2->setFullCircuit(ToggleFullCircuitPointer[2]->isChecked());
	drv2->setRdc(value2[0]);
	drv2->setLsp(value2[1]);
	drv2->setF0 (value2[2]);
	drv2->setQtc(value2[3]);
	drv2->setQes (value2[4]);
	drv2->setQms(value2[5]);
	drv2->setVas(value2[6]);
	drv2->setDm (value2[7]);
	drv2->SetTitle(pDriverTitle[2]->text());

	drv3->PressureisActive=PressurePointer[3]->isChecked();
	drv3->ImpedanzisActive=ImpedanzPointer[3]->isChecked();
	drv3->SummaryisActive=SummaryPointer[3]->isChecked();
	drv3->ScalarSummaryisActive=ScalarSummaryPointer[3]->isChecked();
	drv3->ImpedanzSummaryisActive=ImpedanzSummaryPointer[3]->isChecked();
	drv3->InvertPhase=InvertPhasePointer[3]->isChecked();
  drv3->setFullCircuit(ToggleFullCircuitPointer[3]->isChecked());
	drv3->setRdc(value3[0]);
	drv3->setLsp(value3[1]);
	drv3->setF0 (value3[2]);
	drv3->setQtc(value3[3]);
	drv3->setQes (value3[4]);
	drv3->setQms(value3[5]);
	drv3->setVas(value3[6]);
	drv3->setDm (value3[7]);
	drv3->SetTitle(pDriverTitle[3]->text());
}

void driverinput::Rdc_0(const QString &textparam)
{
	value0[0] = atof((const char*) textparam);
}

void driverinput::Lsp_0(const QString &textparam)
{
	value0[1] = atof((const char*) textparam)/1000;
}

void driverinput::F0_0(const QString &textparam)
{
	value0[2] = atof((const char*) textparam);
}

void driverinput::Qtc_0(const QString &textparam)
{
	value0[3] = atof((const char*) textparam);
}

void driverinput::Qes_0(const QString &textparam)
{
	value0[4] = atof((const char*) textparam);
}

void driverinput::Qms_0(const QString &textparam)
{
	value0[5] = atof((const char*) textparam);
}

void driverinput::Vas_0(const QString &textparam)
{
	value0[6] = atof((const char*) textparam);
}

void driverinput::Dm_0(const QString &textparam)
{
	value0[7] = atof((const char*) textparam);
}
////////////////////////////////////////////////////////////////////////////////////
void driverinput::Rdc_1(const QString &textparam)
{
	value1[0] = atof((const char*) textparam);
}

void driverinput::Lsp_1(const QString &textparam)
{
	value1[1] = atof((const char*) textparam)/1000;
}

void driverinput::F0_1(const QString &textparam)
{
	value1[2] = atof((const char*) textparam);
}

void driverinput::Qtc_1(const QString &textparam)
{
	value1[3] = atof((const char*) textparam);
}

void driverinput::Qes_1(const QString &textparam)
{
	value1[4] = atof((const char*) textparam);
}

void driverinput::Qms_1(const QString &textparam)
{
	value1[5] = atof((const char*) textparam);
}

void driverinput::Vas_1(const QString &textparam)
{
	value1[6] = atof((const char*) textparam);
}

void driverinput::Dm_1(const QString &textparam)
{
	value1[7] = atof((const char*) textparam);
}
////////////////////////////////////////////////////////////////////////////////////
void driverinput::Rdc_2(const QString &textparam)
{
	value2[0] = atof((const char*) textparam);
}

void driverinput::Lsp_2(const QString &textparam)
{
	value2[1] = atof((const char*) textparam)/1000;
}

void driverinput::F0_2(const QString &textparam)
{
	value2[2] = atof((const char*) textparam);
}

void driverinput::Qtc_2(const QString &textparam)
{
	value2[3] = atof((const char*) textparam);
}

void driverinput::Qes_2(const QString &textparam)
{
	value2[4] = atof((const char*) textparam);
}

void driverinput::Qms_2(const QString &textparam)
{
	value2[5] = atof((const char*) textparam);
}

void driverinput::Vas_2(const QString &textparam)
{
	value2[6] = atof((const char*) textparam);
}

void driverinput::Dm_2(const QString &textparam)
{
	value2[7] = atof((const char*) textparam);
}
////////////////////////////////////////////////////////////////////////////////////
void driverinput::Rdc_3(const QString &textparam)
{
	value3[0] = atof((const char*) textparam);
}

void driverinput::Lsp_3(const QString &textparam)
{
	value3[1] = atof((const char*) textparam)/1000;
}

void driverinput::F0_3(const QString &textparam)
{
	value3[2] = atof((const char*) textparam);
}

void driverinput::Qtc_3(const QString &textparam)
{
	value3[3] = atof((const char*) textparam);
}

void driverinput::Qes_3(const QString &textparam)
{
	value3[4] = atof((const char*) textparam);
}

void driverinput::Qms_3(const QString &textparam)
{
	value3[5] = atof((const char*) textparam);
}

void driverinput::Vas_3(const QString &textparam)
{
	value3[6] = atof((const char*) textparam);
}

void driverinput::Dm_3(const QString &textparam)
{
	value3[7] = atof((const char*) textparam);
}

void driverinput::slot_gain0(int DB)
{
	double x = -(DB-12);
	drv0->gain = pow(10,x/20);
	drv0->setmodified();
	emit paramchanged();
}

void driverinput::slot_gain1(int DB)
{
	double x = -(DB-12);
	drv1->gain = pow(10,x/20);
	drv1->setmodified();
	emit paramchanged();
}

void driverinput::slot_gain2(int DB)
{
	double x = -(DB-12);
	drv2->gain = pow(10,x/20);
	drv2->setmodified();
	emit paramchanged();
}

void driverinput::slot_gain3(int DB)
{
	double x = -(DB-12);
	drv3->gain = pow(10,x/20);
	drv3->setmodified();
	emit paramchanged();
}

void driverinput::consistence()
{
	double c0 =	fabs(value0[5]*value0[4]/(value0[5]+value0[4]) - value0[3]);
	double c1 =	fabs(value1[5]*value1[4]/(value1[5]+value1[4]) - value1[3]);
	double c2 =	fabs(value2[5]*value2[4]/(value2[5]+value2[4]) - value2[3]);
	double c3 =	fabs(value3[5]*value3[4]/(value3[5]+value3[4]) - value3[3]);

	if ((c0<0.01)&&(c1<0.01)&&(c2<0.01)&&(c3<0.01)) return; else
	{
		//  	KMessageBox* test = new KMessageBox(this, i18n("Consistence"),i18n("These parameters are not consistent!\n"
		//																"Which paramter should I adjust for you?"),8,"Qts","Qes","Qms");
		KMessageBox::information (this, i18n("These parameters are not consistent!\n"
			"I will adjust Qtc for you."), i18n("Consistence"));
		value0[3] = value0[5] * value0[4] / (value0[5]+value0[4]);
		value1[3] = value1[5] * value1[4] / (value1[5]+value1[4]);
		value2[3] = value2[5] * value2[4] / (value2[5]+value2[4]);
		value3[3] = value3[5] * value3[4] / (value3[5]+value3[4]);
		//		}
		//		if (KMessageBox::questionYesNo (this, i18n("Consistence"),i18n("These parameters are not consistent!\n"
		//									"Should I adjust Qes for you?")) == 1)
		//		{
		//					value0[4] = value0[5] / (value0[5] / value0[3]-1);
		//					value1[4] = value1[5] / (value1[5] / value1[3]-1);
		//					value2[4] = value2[5] / (value2[5] / value2[3]-1);
		//					value3[4] = value3[5] / (value3[5] / value3[3]-1);
		//		}
		//		if (KMessageBox::questionYesNo (this, i18n("Consistence"),i18n("These parameters are not consistent!\n"
		//									"Should I adjust Qms for you?")) == 1)
		//		{
		//         value0[5] = value0[4] / (value0[4] / value0[3]-1);
		//         value1[5] = value1[4] / (value1[4] / value1[3]-1);
		//         value2[5] = value2[4] / (value2[4] / value2[3]-1);
		//         value3[5] = value3[4] / (value3[4] / value3[3]-1);
		//		}
		//		switch (test->exec())
		//    	{
		//				case 1:
		//   				value0[3] = value0[5] * value0[4] / (value0[5]+value0[4]);
		//					value1[3] = value1[5] * value1[4] / (value1[5]+value1[4]);
		//					value2[3] = value2[5] * value2[4] / (value2[5]+value2[4]);
		//					value3[3] = value3[5] * value3[4] / (value3[5]+value3[4]);
		//				break;
		//				case 2:
		//					value0[4] = value0[5] / (value0[5] / value0[3]-1);
		//					value1[4] = value1[5] / (value1[5] / value1[3]-1);
		//					value2[4] = value2[5] / (value2[5] / value2[3]-1);
		//					value3[4] = value3[5] / (value3[5] / value3[3]-1);
		//				break;
		//				case 3:
		//          value0[5] = value0[4] / (value0[4] / value0[3]-1);
		//          value1[5] = value1[4] / (value1[4] / value1[3]-1);
		//          value2[5] = value2[4] / (value2[4] / value2[3]-1);
		//          value3[5] = value3[4] / (value3[4] / value3[3]-1);
		//				break;
		//			}
		sprintf (text0[3], "%6.2f", value0[3]);
		pQtc[0]->setText(text0[3]);
		sprintf (text0[4], "%6.2f", value0[4]);
		pQes[0]->setText(text0[4]);
		sprintf (text0[5], "%6.2f", value0[5]);
		pQms[0]->setText(text0[5]);

		sprintf (text1[3], "%6.2f", value1[3]);
		pQtc[1]->setText(text1[3]);
		sprintf (text1[4], "%6.2f", value1[4]);
		pQes[1]->setText(text1[4]);
		sprintf (text1[5], "%6.2f", value1[5]);
		pQms[1]->setText(text1[5]);

		sprintf (text2[3], "%6.2f", value2[3]);
		pQtc[2]->setText(text2[3]);
		sprintf (text2[4], "%6.2f", value2[4]);
		pQes[2]->setText(text2[4]);
		sprintf (text2[5], "%6.2f", value2[5]);
		pQms[2]->setText(text2[5]);

		sprintf (text3[3], "%6.2f", value3[3]);
		pQtc[3]->setText(text3[3]);
		sprintf (text3[4], "%6.2f", value3[4]);
		pQes[3]->setText(text3[4]);
		sprintf (text3[5], "%6.2f", value3[5]);
		pQms[3]->setText(text3[5]);
	}
}


/** forces the dialog to get the
updated values from the application */
void driverinput::slot_initnewvalues()
{

	initValues();
	updateCheckboxes();
}

/** resets relevant checkboxes */
void driverinput::updateCheckboxes()
{
	PressurePointer[0]->setChecked(activePressure[0]);
	PressurePointer[1]->setChecked(activePressure[1]);
	PressurePointer[2]->setChecked(activePressure[2]);
	PressurePointer[3]->setChecked(activePressure[3]);

	ImpedanzPointer[0]->setChecked(activeImpedanz[0]);
	ImpedanzPointer[1]->setChecked(activeImpedanz[1]);
	ImpedanzPointer[2]->setChecked(activeImpedanz[2]);
	ImpedanzPointer[3]->setChecked(activeImpedanz[3]);

  ToggleFullCircuitPointer[0]->setChecked(toggleFullCircuit[0]);
  ToggleFullCircuitPointer[1]->setChecked(toggleFullCircuit[1]);
  ToggleFullCircuitPointer[2]->setChecked(toggleFullCircuit[2]);
  ToggleFullCircuitPointer[3]->setChecked(toggleFullCircuit[3]);
}


/**  */
void driverinput::closeEvent(QCloseEvent *ce)
{
}
