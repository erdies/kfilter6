/***************************************************************************
colordialog.h  -  description
-------------------
begin                : Mon Jan 8 2001
copyright            : (C) 2001 by Martin Erdtmann
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

#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <kcolordlg.h>

class KFilterView;

/**
*@author Martin Erdtmann
*/

class ColorDialog : public QDialog
{
  Q_OBJECT
public: 
  ColorDialog( KFilterView* a_pfilterviewView, const char* a_pcharName );
  ~ColorDialog();

private:

  void closeEvent(QCloseEvent *);

private slots: // Private slots
	/** Sets the Backgroundcolor */
	void slot_Background();
	/** sets the gridcolor */
	void slot_Grid();
	/** sets the pressurecolor */
	void slot_Pressure();
	/** sets the impedancecolor */
	void slot_Impedance();
	/** sets the impedancecolor */
	void slot_ImpedanceS();
	/** sets the impedancecolor */
	void slot_PressureS();
	/** sets the impedancecolor */
	void slot_ScalarPressureS();
	/** closes the dialog */
	void slotcloseButton();
	/**set the color defaults */
	void slotdefaultButton();

signals:

void isclosed();

protected:
	KFilterView*	m_pfilterviewView;
};

#endif
