/***************************************************************************
networkdialog.h  -  description
-------------------
begin                : Sat Jun 10 2000
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

#ifndef NETWORKDIALOG_H
#define NETWORKDIALOG_H

#include <qscrollview.h>
#include <qlineedit.h>
#include <qmessagebox.h>
#include <klocale.h>
#include <kstddirs.h>
#include <stdio.h>
#include <kapp.h>
#include <stdlib.h>
#include <qlabel.h>
#include <qstring.h>
#include <qtabdialog.h>
#include <qpushbutton.h>
#include "driver.h"
#include "circuitout.h"

/**
*@author Martin Erdtmann
*/

class NetworkDialog : public QTabDialog
{
	Q_OBJECT
public: 
	NetworkDialog(const char *name,driver*, driver*, driver*, driver*);
	~NetworkDialog();
	
private:
	
	double value0[48],value1[48],value2[48],value3[48];
	QLineEdit *f0[48],*f1[48],*f2[48],*f3[48];
	QWidget *m_pqwidgetDriver[4];
	driver* m_pdriverDriver[ 4 ];
	QScrollView *p_prevScroll[4];
	CircuitOut *p_prevCircuit[4];
	
	void initValues();
	void updateValues();
	void drawEdit(QLineEdit* p[48]);
	void Units(QWidget *);
	/**  */
	void closeEvent(QCloseEvent *);
	
private slots:
	void applyClicked();
	void defaultClicked();
	void cancelClicked();

	void slot_reset0();
	void slot_reset1();
	void slot_reset2();
	void slot_reset3();
	void slot_draw0();
	void slot_draw1();
	void slot_draw2();
	void slot_draw3();
	
	void field0_1(const QString&);
	void field0_2(const QString&);
	void field0_3(const QString&);
	void field0_4(const QString&);
	void field0_5(const QString&);
	void field0_6(const QString&);
	void field0_7(const QString&);
	void field0_8(const QString&);
	void field1_1(const QString&);
	void field1_2(const QString&);
	void field1_3(const QString&);
	void field1_4(const QString&);
	void field1_5(const QString&);
	void field1_6(const QString&);
	void field1_7(const QString&);
	void field1_8(const QString&);
	void field2_1(const QString&);
	void field2_2(const QString&);
	void field2_3(const QString&);
	void field2_4(const QString&);
	void field2_5(const QString&);
	void field2_6(const QString&);
	void field2_7(const QString&);
	void field2_8(const QString&);
	void field3_1(const QString&);
	void field3_2(const QString&);
	void field3_3(const QString&);
	void field3_4(const QString&);
	void field3_5(const QString&);
	void field3_6(const QString&);
	void field3_7(const QString&);
	void field3_8(const QString&);
	
	void updateBuffer(const QString&);
	
signals:
	/*if something is changed to the parameter configuration, it will be sent*/
	void paramchanged();
	void isclosed();
	
public slots: // Public slots
	
			  /** The dialog gets updated values
	from the application */
	void slot_initnewvalues();
};

#endif
