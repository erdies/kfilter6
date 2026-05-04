/***************************************************************************
colordialog.cpp  -  description
-------------------
begin                : Mon Jan 8 2001
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

#include <qdialog.h>
#include <qpushbutton.h>
#include <qlabel.h>
#include <klocale.h>
#include <qstring.h>
#include <qcolor.h>
#include <qgroupbox.h>

#include "colordialog.h"
#include "kfilterview.h"

ColorDialog::ColorDialog( KFilterView* a_pfilterviewView, const char* a_pcharName )
{
	
	m_pfilterviewView = a_pfilterviewView;
	setCaption( a_pcharName );
	this->setFixedSize(200,310);
	
	QGroupBox* pqgroupboxGroup = new QGroupBox( this );
	pqgroupboxGroup->setGeometry( 15, 5, 170, 250 );
	pqgroupboxGroup->setTitle( i18n("Colors") );
	
	QPushButton* pqpushbuttonClose = new QPushButton(i18n("Close"),this);
	pqpushbuttonClose->setGeometry( 140, 267, 50, 25 );
	pqpushbuttonClose->show();
	pqpushbuttonClose->setDefault( true );
	QObject::connect( pqpushbuttonClose, SIGNAL( clicked() ), this, SLOT( slotcloseButton() ) );
	
	QPushButton* pqpushbuttonDefault = new QPushButton(i18n("Default"),this);
	pqpushbuttonDefault->setGeometry( 20, 267, 70, 25 );
	pqpushbuttonDefault->show();
	QObject::connect( pqpushbuttonDefault, SIGNAL( clicked() ), this, SLOT( slotdefaultButton() ) );
	
	QString qstringLabel[ 7 ] =
		{ i18n("Background"), i18n("Gridcolor"),
		i18n("Pressure"), i18n("Impedance"),
		i18n("PressureS"), i18n("ImpedanceS"), i18n("ScalarPresS") };
	
	QPushButton* pqpushbuttonButton[ 7 ];
	for( int intI = 0; intI < 7; intI++ )
	{
		pqpushbuttonButton[ intI ] = new QPushButton( qstringLabel[ intI ], this );
		pqpushbuttonButton[ intI ]->setGeometry( 26, 25 + intI * 30, 146, 30 );
	}
	QObject::connect( pqpushbuttonButton[ 0 ], SIGNAL( clicked() ), this, SLOT( slot_Background() ) );
	QObject::connect( pqpushbuttonButton[ 1 ], SIGNAL( clicked() ), this, SLOT( slot_Grid() ) );
	QObject::connect( pqpushbuttonButton[ 2 ], SIGNAL( clicked() ), this, SLOT( slot_Pressure() ) );
	QObject::connect( pqpushbuttonButton[ 3 ], SIGNAL( clicked() ), this, SLOT( slot_Impedance() ) );
	QObject::connect( pqpushbuttonButton[ 4 ], SIGNAL( clicked() ), this, SLOT( slot_PressureS() ) );
	QObject::connect( pqpushbuttonButton[ 5 ], SIGNAL( clicked() ), this, SLOT( slot_ImpedanceS() ) );
	QObject::connect( pqpushbuttonButton[ 6 ], SIGNAL( clicked() ), this, SLOT( slot_ScalarPressureS() ) );
}

ColorDialog::~ColorDialog()
{
}

/** Sets the Backgroundcolor */
void ColorDialog::slot_Background()
{
	QColor qcolorColor = m_pfilterviewView->backgroundColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setBackgroundColor( qcolorColor );
	m_pfilterviewView->repaint();
}

/** sets the gridcolor */
void ColorDialog::slot_Grid()
{
	QColor qcolorColor = m_pfilterviewView->gridColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setGridColor( qcolorColor );
	m_pfilterviewView->repaint();
}

/** sets the pressurecolor */
void ColorDialog::slot_Pressure()
{
	QColor qcolorColor = m_pfilterviewView->pressureColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setPressureColor( qcolorColor );
	m_pfilterviewView->repaint();
}

/** sets the impedancecolor */
void ColorDialog::slot_Impedance()
{
	QColor qcolorColor = m_pfilterviewView->impedanceColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setImpedanceColor( qcolorColor );
	m_pfilterviewView->repaint();
}

void ColorDialog::slot_PressureS()
{
	QColor qcolorColor = m_pfilterviewView->pressureSummaryColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setPressureSummaryColor( qcolorColor );
	m_pfilterviewView->repaint();
}

void ColorDialog::slot_ImpedanceS()
{
	QColor qcolorColor = m_pfilterviewView->impedanceSummaryColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setImpedanceSummaryColor( qcolorColor );
	m_pfilterviewView->repaint();
}

void ColorDialog::slot_ScalarPressureS()
{
	QColor qcolorColor = m_pfilterviewView->scalarPressureSummaryColor();
	KColorDialog::getColor( qcolorColor );
	m_pfilterviewView->setScalarPressureSummaryColor( qcolorColor );
	m_pfilterviewView->repaint();
}

/** closes the dialog */
void ColorDialog::slotcloseButton()
{
  emit isclosed();
  accept();
}

/** sets the color defaults */
void ColorDialog::slotdefaultButton()
{
	QColor qcolorColor( 0, 128, 122 );
	m_pfilterviewView->setBackgroundColor( qcolorColor );
	qcolorColor.setRgb( 171, 0, 0 );
	m_pfilterviewView->setGridColor( qcolorColor );
	qcolorColor.setRgb( 255, 255, 255 );
	m_pfilterviewView->setPressureColor( qcolorColor );
	qcolorColor.setRgb( 255, 255, 0 );
	m_pfilterviewView->setImpedanceColor( qcolorColor );
	qcolorColor.setRgb( 0, 0, 255 );
	m_pfilterviewView->setPressureSummaryColor( qcolorColor );
	qcolorColor.setRgb( 0, 255, 0 );
	m_pfilterviewView->setImpedanceSummaryColor( qcolorColor );
	qcolorColor.setRgb( 255, 0, 0 );
	m_pfilterviewView->setScalarPressureSummaryColor( qcolorColor );
}

void ColorDialog::closeEvent(QCloseEvent *ce)
{
}
