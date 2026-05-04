/***************************************************************************
volumedialog.h  -  description
-------------------
begin                : Sat Jun 24 2000
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

#ifndef VOLUMEDIALOG_H
#define VOLUMEDIALOG_H

#include <qtabdialog.h>
#include <qlineedit.h>
#include <qlabel.h>
#include "driver.h"

/**
*@author Martin Erdtmann
*/

class VolumeDialog : public QTabDialog  {
	Q_OBJECT
public: 
	VolumeDialog(const char* a_pcharName, driver* a_pdriverDriver0, driver* a_pdriverDriver1,
		driver* a_pdriverDriver2, driver* a_pdriverDriver3 );
	~VolumeDialog();
	
	
private slots:
	void applyClicked();
	void defaultClicked();
	void cancelClicked();
	
	void slotButtonPressed( int a_intButtonID );
	void slotHogeClicked();
	void slotVb( const QString& a_qstringString );
	void slotFb( const QString& a_qstringString );
	void slotV2( const QString& a_qstringString );
	void slotBandwidthClicked();
	void slotTubeDiameterEdit( const QString& a_qstringString );
	void slotCurrentTabChanged( QWidget* a_pqwidgetTab );
private:
	
	driver* 	m_pdriverDriver[ 4 ];
	QWidget* 	m_pqwidgetDriver[4];
	QWidget* 	m_pqwidgetDriver1[4];
	QWidget* 	m_pqwidgetDriver2[4];
	QWidget* 	m_pqwidgetDriver3[4];
	
	char 		m_charVbText[ 4 ][ 10 ];
	char 		m_charFbText[ 4 ][ 10 ];
	char		m_charV2Text[ 4 ][ 10 ];
	
	QLabel*		m_pqlabelTubeLength[ 4 ];
	QLineEdit*	m_pqlineeditTubeDiameter[ 4 ];
	
	QLineEdit*	m_pqlineeditVb[ 4 ];
	QLineEdit*	m_pqlineeditFb[ 4 ];
	QLineEdit*	m_pqlineeditV2[ 4 ];
	int			m_intBoxTypeProposal[ 4 ];
	
	double		m_doubleVb[ 4 ];
	double		m_doubleFb[ 4 ];
	double		m_doubleV2[ 4 ];
	
	void initValues();
	void updateValues();
	void closeEvent(QCloseEvent *);
	
signals:
	/*if something is changed to the parameter configuration, it will be sent*/
	void paramchanged();
	void isclosed();
protected: // Protected attributes
  /**  */
  int m_intCurrentVolumeGroup;
};

#endif
