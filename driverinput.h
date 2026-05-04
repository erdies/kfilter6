/***************************************************************************
driverinput.h  -  description
-------------------
begin                : Sun May 7 2000
copyright            : (C) 2000 by Martin Erdtmann
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

#ifndef DRIVERINPUT_H
#define DRIVERINPUT_H

#include <qtabdialog.h>
#include "driver.h"
#include "kfilterdoc.h"
#include <qslider.h>
#include <qcheckbox.h>
#include <math.h>
#include <qlineedit.h>

/**
*@author Martin Erdtmann
*/

class driverinput : public QTabDialog
{
	Q_OBJECT
public:
	driverinput(const char *name = 0, driver *a=0,driver *b=0,driver *c=0,driver *d=0);
	~driverinput();


private slots:
	void applyClicked();
	void defaultClicked();
	void cancelClicked();

	void Rdc_0(const QString&);
	void Lsp_0(const QString&);
	void F0_0(const QString&);
	void Qtc_0(const QString&);
	void Qes_0(const QString&);
	void Qms_0(const QString&);
	void Vas_0(const QString&);
	void Dm_0(const QString&);

	void Rdc_1(const QString&);
	void Lsp_1(const QString&);
	void F0_1(const QString&);
	void Qtc_1(const QString&);
	void Qes_1(const QString&);
	void Qms_1(const QString&);
	void Vas_1(const QString&);
	void Dm_1(const QString&);

	void Rdc_2(const QString&);
	void Lsp_2(const QString&);
	void F0_2(const QString&);
	void Qtc_2(const QString&);
	void Qes_2(const QString&);
	void Qms_2(const QString&);
	void Vas_2(const QString&);
	void Dm_2(const QString&);

	void Rdc_3(const QString&);
	void Lsp_3(const QString&);
	void F0_3(const QString&);
	void Qtc_3(const QString&);
	void Qes_3(const QString&);
	void Qms_3(const QString&);
	void Vas_3(const QString&);
	void Dm_3(const QString&);

	void slot_gain0(int);
	void slot_gain1(int);
	void slot_gain2(int);

	void slot_gain3(int);

private:

	char text0[10][10],text1[10][10],text2[10][10],text3[10][10];

	bool activePressure[4],activeImpedanz[4],activeSummary[4], activeScalarSummary[4],
       activeImpedanzSummary[4], invertPhase[4], toggleFullCircuit[4];

  QCheckBox *PressurePointer[4], *ImpedanzPointer[4], *SummaryPointer[4], *ScalarSummaryPointer[4],
            *ImpedanzSummaryPointer[4], *InvertPhasePointer[4], *ToggleFullCircuitPointer[4];
	//FilterDoc *doc;

	double value0[8],value1[8],value2[8],value3[8];
	driver *drv0,*drv1,*drv2,*drv3;

	void initValues();
	void updateValues();
	void consistence();
	/** resets relevant checkboxes */
	void updateCheckboxes();
	/**  */
	void closeEvent(QCloseEvent *);


	QLineEdit *pQtc[4],*pQes[4],*pQms[4], *pDriverTitle[4];

signals:
	/*if something is changed to the parameter configuration, it will be sent*/
	void paramchanged();
	void isclosed();

public slots: // Public slots
			  /** forces the dialog to get the
	updated values from the application */
	void slot_initnewvalues();
};

#endif
