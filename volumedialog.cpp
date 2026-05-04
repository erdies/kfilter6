/***************************************************************************
volumedialog.cpp  -  description
-------------------
begin                : Sun May 7 2000
copyright            : (C) 2001 by Martin Erdtmann  /  Stefan Okrongli
email                : martin.erdtmann@gmx.de  /  s_okrongli@gmx.net
***************************************************************************/

/***************************************************************************
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include <klocale.h>
#include <kstddirs.h>
#include <qradiobutton.h>
#include <qbuttongroup.h>
#include <qlabel.h>
#include <stdlib.h>
// SO 2001March24 - Added <stdio.h> for sprintf declaration
#include <stdio.h>
#include <math.h>
#include "volumedialog.h"
#include "qpushbutton.h"

#include <qmessagebox.h>

VolumeDialog::VolumeDialog(const char *name,driver *driverptr0,driver *driverptr1,driver *driverptr2,driver *driverptr3)
{
	m_pdriverDriver[ 0 ] = driverptr0;
	m_pdriverDriver[ 1 ] = driverptr1;
	m_pdriverDriver[ 2 ] = driverptr2;
	m_pdriverDriver[ 3 ] = driverptr3;
	initValues();
	setCaption(name);
	
	this->setFixedSize(350,260);
	
	
	QGroupBox*	pqgroupboxGroup = 0;
    QRadioButton* pqbuttonButton[ 4 ][ 4 ];
	QLabel*		pqlabelButtonLabel[ 4 ][ 4 ];
	QButtonGroup* pqbuttongroupGroup[ 4 ];
	QPushButton* pqpushbuttonHogeButton[ 4 ];
	QPushButton* pqpushbuttonBandwidthButton[ 4 ];
	QLabel* 	pqlabelVbLabel[ 4 ];
	QLabel* 	pqlabelVbLabelX[ 4 ];
	QLabel* 	pqlabelFbLabel[ 4 ];
	QLabel* 	pqlabelFbLabelX[ 4 ];
	QLabel* 	pqlabelV2Label[ 4 ];
	QLabel* 	pqlabelV2LabelX[ 4 ];
	QLabel* 	pqlabelTubeDiameterLabel[ 4 ];
	QLabel* 	pqlabelTubeDiameterLabelX[ 4 ];
	QLabel* 	pqlabelTubeLengthLabel[ 4 ];
	QLabel* 	pqlabelTubeLengthLabelX[ 4 ];
	
	char		charTabText[ 32 ];
		
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		m_pqwidgetDriver[ intCount ] = new QWidget( this );
		m_pqwidgetDriver[ intCount ]->setFixedSize( 340, 255 );
		m_pqwidgetDriver[ intCount ]->setGeometry( 0, 0, 200, 60 );
	
		pqgroupboxGroup = new QGroupBox( m_pqwidgetDriver[ intCount ] );
		pqgroupboxGroup->setGeometry( 10, 6, 230, 138 );
		pqgroupboxGroup->setTitle( i18n("Types") );
	
		pqbuttongroupGroup[ intCount ] = new QButtonGroup();
		
		for( int intButtonIndex = 0; intButtonIndex < 4; intButtonIndex++ )
		{
			pqbuttonButton[ intCount ][ intButtonIndex ] = new QRadioButton( m_pqwidgetDriver[ intCount ] );
			pqbuttongroupGroup[ intCount ]->insert( pqbuttonButton[ intCount ][ intButtonIndex ], intButtonIndex );
		}
		
		pqbuttonButton[ intCount ][ 0 ]->setGeometry( 20, 25, 20, 20);
		pqlabelButtonLabel[ intCount ][ 0 ] = new QLabel( i18n("No Volume"), m_pqwidgetDriver[ intCount ] );
		pqlabelButtonLabel[ intCount ][ 0 ]->setGeometry( 40, 25, 90, 20);
	
		pqbuttonButton[ intCount ][ 1 ]->setGeometry( 20, 55, 20,20 );
		pqlabelButtonLabel[ intCount ][ 1 ] = new QLabel( i18n("Closed Box"), m_pqwidgetDriver[ intCount ] );
		pqlabelButtonLabel[ intCount ][ 1 ]->setGeometry( 40, 55, 100, 20);
	
		pqbuttonButton[ intCount ][ 2 ]->setGeometry( 20, 85, 20, 20 );
		pqlabelButtonLabel[ intCount ][ 2 ] = new QLabel( i18n("Ventilated Box"), m_pqwidgetDriver[ intCount ] );
		pqlabelButtonLabel[ intCount ][ 2 ]->setGeometry( 40, 85, 130, 20);
	
		pqbuttonButton[ intCount ][ 3 ]->setGeometry( 20, 115 , 20, 20 );
		pqlabelButtonLabel[ intCount ][ 3 ] = new QLabel( i18n("Bandpass Box"), m_pqwidgetDriver[ intCount ] );
		pqlabelButtonLabel[ intCount ][ 3 ]->setGeometry( 40, 115, 170, 20 );
		
		pqbuttongroupGroup[ intCount ]->setButton( m_intBoxTypeProposal[ intCount ] );
		
		m_intCurrentVolumeGroup = intCount;
		
		this->slotButtonPressed( m_intBoxTypeProposal[ intCount ] );
		QObject::connect( pqbuttongroupGroup[ intCount ], SIGNAL( pressed( int ) ), this, \
			SLOT( slotButtonPressed( int ) ) );
	
		pqpushbuttonHogeButton[ intCount ] = new QPushButton( i18n("Calculate via Hoge"), \
			m_pqwidgetDriver[ intCount ] );
		pqpushbuttonHogeButton[ intCount ]->setGeometry( 130, 165, 150, 25 );
		pqpushbuttonHogeButton[ intCount ]->show();
		QObject::connect( pqpushbuttonHogeButton[ intCount ], SIGNAL( clicked() ), this, \
			SLOT( slotHogeClicked() ) );
	
		pqpushbuttonBandwidthButton[ intCount ] = new QPushButton( i18n("Syncronize Bandwidth"), \
			m_pqwidgetDriver[ intCount ] );
		pqpushbuttonBandwidthButton[ intCount ]->setGeometry( 130, 205, 150, 25 );
		pqpushbuttonBandwidthButton[ intCount ]->show();
		QObject::connect( pqpushbuttonBandwidthButton[ intCount ], SIGNAL( clicked() ), this, \
			SLOT( slotBandwidthClicked() ) );

		pqlabelVbLabel[ intCount ] = new QLabel( "Vb", m_pqwidgetDriver[ intCount ] );
		pqlabelVbLabel[ intCount ]->setGeometry( 15, 155, 30, 20 );
		pqlabelVbLabelX[ intCount ] = new QLabel( "L", m_pqwidgetDriver[ intCount ] );
		pqlabelVbLabelX[ intCount ]->setGeometry( 100, 155, 20, 20 );
		m_pqlineeditVb[ intCount ] = new QLineEdit( m_pqwidgetDriver[ intCount ] );
		m_pqlineeditVb[ intCount ]->setGeometry( 45, 155, 50, 20 );
		sprintf( m_charVbText[ intCount ], "%6.2f", m_doubleVb[ intCount ] );
		m_pqlineeditVb[ intCount ]->setText( m_charVbText[ intCount ] );
		QObject::connect( m_pqlineeditVb[ intCount ], SIGNAL( textChanged( const QString&) ), this, \
			SLOT( slotVb(const QString& ) ) );
		
		pqlabelFbLabel[ intCount ] = new QLabel( "Fb", m_pqwidgetDriver[ intCount ] );
		pqlabelFbLabel[ intCount ]->setGeometry( 15, 185, 30, 20 );
		pqlabelFbLabelX[ intCount ] = new QLabel( "Hz", m_pqwidgetDriver[ intCount ] );
		pqlabelFbLabelX[ intCount ]->setGeometry( 100, 185, 20, 20 );
		m_pqlineeditFb[ intCount ] = new QLineEdit( m_pqwidgetDriver[ intCount ] );
		m_pqlineeditFb[ intCount ]->setGeometry( 45, 185, 50, 20 );
		sprintf ( m_charFbText[ 0 ], "%6.2f", m_doubleFb[ intCount ] );
		m_pqlineeditFb[ intCount ]->setText( m_charFbText[ 0 ] );
		QObject::connect( m_pqlineeditFb[ intCount ], SIGNAL( textChanged(const QString&) ), this, \
			SLOT( slotFb( const QString& ) ) );
		
		pqlabelV2Label[ intCount ] = new QLabel( "V2", m_pqwidgetDriver[ intCount ] );
		pqlabelV2Label[ intCount ]->setGeometry( 15, 215, 30, 20 );
		pqlabelV2LabelX[ intCount ] = new QLabel( "L", m_pqwidgetDriver[ intCount ] );
		pqlabelV2LabelX[ intCount ]->setGeometry( 100, 215, 20, 20 );
		m_pqlineeditV2[ intCount ] = new QLineEdit( m_pqwidgetDriver[ intCount ] );
		m_pqlineeditV2[ intCount ]->setGeometry( 45, 215, 50, 20 );
		sprintf ( m_charV2Text[ 0 ], "%6.2f", m_doubleV2[ intCount ] );
		m_pqlineeditV2[ intCount ]->setText( m_charV2Text[ 0 ] );
		QObject::connect( m_pqlineeditV2[ intCount ], SIGNAL(textChanged(const QString&) ), this, \
			SLOT( slotV2( const QString& ) ) );
	
		pqlabelTubeDiameterLabel[ intCount ] = new QLabel( i18n("Tube length"), m_pqwidgetDriver[ intCount ] );
		pqlabelTubeDiameterLabel[ intCount ]->setGeometry( 250, 68, 90, 20 );
		pqlabelTubeDiameterLabelX[ intCount ] = new QLabel( "cm", m_pqwidgetDriver[ intCount ] );
		pqlabelTubeDiameterLabelX[ intCount ]->setGeometry( 295, 37, 30, 20 );
		m_pqlineeditTubeDiameter[ intCount ] = new QLineEdit( m_pqwidgetDriver[ intCount ] );
		m_pqlineeditTubeDiameter[ intCount ]->setGeometry( 250, 37, 40, 20 );
		m_pqlineeditTubeDiameter[ intCount ]->setText("0.0");
	
		pqlabelTubeLengthLabel[ intCount ] = new QLabel( i18n("Tube diam."), m_pqwidgetDriver[ intCount ] );
		pqlabelTubeLengthLabel[ intCount ]->setGeometry( 250, 12, 90, 20 );
		pqlabelTubeLengthLabelX[ intCount ] = new QLabel( "cm", m_pqwidgetDriver[ intCount ] );
		pqlabelTubeLengthLabelX[ intCount ]->setGeometry( 295, 87, 30, 20 );
		m_pqlabelTubeLength[ intCount ] = new QLabel( m_pqwidgetDriver[ intCount ] );
		m_pqlabelTubeLength[ intCount ]->setGeometry( 250, 87, 40, 20 );
		m_pqlabelTubeLength[ intCount ]->setText("0.0");
	
		QObject::connect( m_pqlineeditTubeDiameter[ intCount ] , SIGNAL(textChanged(const QString&) ), this, \
			SLOT( slotTubeDiameterEdit( const QString& ) ) );
			
		sprintf( charTabText, "Driver %d", intCount + 1 );
		
		addTab( m_pqwidgetDriver[ intCount ], i18n( (const char*)charTabText ) );
	}	
	
	setOKButton();
	setDefaultButton(i18n("Apply"));
	setCancelButton();
	
	m_intCurrentVolumeGroup = 0;
	
	QObject::connect(this, SIGNAL(applyButtonPressed() ), this, SLOT( applyClicked() ) );
	QObject::connect(this, SIGNAL(defaultButtonPressed() ), this, SLOT( defaultClicked() ) );
	QObject::connect(this, SIGNAL(cancelButtonPressed() ), this, SLOT( cancelClicked() ) );
	QObject::connect(this, SIGNAL(currentChanged(QWidget*) ), this, SLOT( slotCurrentTabChanged(QWidget*) ) );
}

VolumeDialog::~VolumeDialog()
{
}

void VolumeDialog::applyClicked()
{
	// copy data from dialog into applicaiton
	updateValues();
	
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		m_pdriverDriver[ intCount ]->Berechneparameter();
		m_pdriverDriver[ intCount ]->setmodified();
	}
	emit paramchanged();
	emit isclosed();
	accept();
}

void VolumeDialog::cancelClicked()
{
	//don´t copy data from the dialog into application
	emit isclosed();
	reject();
}

void VolumeDialog::defaultClicked()
{
	
	updateValues();
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		m_pdriverDriver[ intCount ]->Berechneparameter();
		m_pdriverDriver[ intCount ]->setmodified();
	}
	emit paramchanged();
}

void VolumeDialog::initValues()
{
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		if ( m_pdriverDriver[ intCount ] != 0 )
		{
			m_doubleVb[ intCount ] = m_pdriverDriver[ intCount ]->Vb;
			m_doubleFb[ intCount ] = m_pdriverDriver[ intCount ]->Fb;
			m_doubleV2[ intCount ] = m_pdriverDriver[ intCount ]->V2;
			m_intBoxTypeProposal[ intCount ] = m_pdriverDriver[ intCount ]->GTyp;
		}
	}	
}

void VolumeDialog::updateValues()
{
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		m_pdriverDriver[ intCount ]->Vb = m_doubleVb[ intCount ];
		m_pdriverDriver[ intCount ]->Fb = m_doubleFb[ intCount ];
		m_pdriverDriver[ intCount ]->V2 = m_doubleV2[ intCount ];
		m_pdriverDriver[ intCount ]->GTypProposal = m_intBoxTypeProposal[ intCount ];
	}
}

void VolumeDialog::slotVb(const QString & a_qstringText )
{
	m_doubleVb[ m_intCurrentVolumeGroup ] = atof( (const char*)a_qstringText );
}

void VolumeDialog::slotFb(const QString & a_qstringText )
{
	m_doubleFb[ m_intCurrentVolumeGroup ] = atof( (const char*)a_qstringText );
}

void VolumeDialog::slotV2(const QString & a_qstringText )
{
	m_doubleV2[ m_intCurrentVolumeGroup ] = atof( (const char*)a_qstringText );
}

void VolumeDialog::slotCurrentTabChanged( QWidget* a_pqwidgetTab )
{
	for ( int intCount = 0; intCount < 4; intCount++ )
	{
		if ( m_pqwidgetDriver[ intCount ] == a_pqwidgetTab )
		{
			m_intCurrentVolumeGroup = intCount;
			return;
		}
	}
	m_intCurrentVolumeGroup = 0;
}

void VolumeDialog::slotButtonPressed( int a_intButtonID )
{
	QString qstringPath0 = locate ("data", "kfilter/pics/volume0.xpm");
	QString qstringPath1 = locate ("data", "kfilter/pics/volume1.xpm");
	QString qstringPath2 = locate ("data", "kfilter/pics/volume2.xpm");
	QString qstringPath3 = locate ("data", "kfilter/pics/volume3.xpm");
	m_intBoxTypeProposal[ m_intCurrentVolumeGroup ] = a_intButtonID;
	QLabel* pqlabelPicture = new QLabel( m_pqwidgetDriver[ m_intCurrentVolumeGroup ] );
	pqlabelPicture->setGeometry( 180, 20, 50, 50 );
	
	switch( a_intButtonID )
	{
	case 0 :
		pqlabelPicture->setPixmap( QPixmap( qstringPath0 ) );
		break;
	case 1 :
		pqlabelPicture->setPixmap( QPixmap( qstringPath1 ) );
		break;
	case 2 :
		pqlabelPicture->setPixmap( QPixmap( qstringPath2 ) );
		break;
	case 3 :
		pqlabelPicture->setPixmap( QPixmap( qstringPath3 ) );
		break;
	}
	pqlabelPicture->show();
	this->defaultClicked();
}

void VolumeDialog::slotHogeClicked()
{
	
	char charText[ 30 ];
	if ( m_pdriverDriver[ m_intCurrentVolumeGroup ]->getQtc() < 0.8 )
	{
		m_doubleVb[ m_intCurrentVolumeGroup ] = exp( log( m_pdriverDriver[ m_intCurrentVolumeGroup ]->getQtc() )*2.87 ) *\
			m_pdriverDriver[ m_intCurrentVolumeGroup ]->getVas() * 15.0;
		sprintf( charText, "%6.2f", m_doubleVb[ m_intCurrentVolumeGroup ] );
		m_pqlineeditVb[ m_intCurrentVolumeGroup ]->setText( charText );
		m_doubleFb[ m_intCurrentVolumeGroup ] = 0.42 * m_pdriverDriver[ m_intCurrentVolumeGroup ]->getF0() / ( exp( log( \
			m_pdriverDriver[ m_intCurrentVolumeGroup ]->getQtc() ) * 0.9 ) );
		sprintf ( charText, "%6.2f", m_doubleFb[ m_intCurrentVolumeGroup ] );
		m_pqlineeditFb[ m_intCurrentVolumeGroup ]->setText( charText );
	}
	else
	{
		QMessageBox::warning( this, "Information", "Driver with Qts > 0.8 cannot be used for " \
			"base reflex speakers!\nPlease buy a new one ;-)" );
	}
	this->updateValues();
}

void VolumeDialog::slotBandwidthClicked()
{
	char charText[ 30 ];
	if ( m_doubleV2[ m_intCurrentVolumeGroup ] != 0.0 )
	{
		m_doubleFb[ m_intCurrentVolumeGroup ] = m_pdriverDriver[ m_intCurrentVolumeGroup ]->getF0() * \
			sqrt( m_pdriverDriver[ m_intCurrentVolumeGroup ]->getVas() / m_doubleV2[ m_intCurrentVolumeGroup ] + 1 );
	}
	sprintf( charText, "%6.2f", m_doubleFb[ m_intCurrentVolumeGroup ] );
	m_pqlineeditFb[ m_intCurrentVolumeGroup ]->setText( charText );
}

void VolumeDialog::slotTubeDiameterEdit( const QString & a_qstringText )
{
	if ( ( m_doubleFb[ m_intCurrentVolumeGroup ] != 0.0 ) && \
		( m_doubleVb[ m_intCurrentVolumeGroup ] != 0.0 ) )
	{
		double doubleDiameter = atof( (const char*)a_qstringText );
		double doubleSurface  = pow( ( 0.5 * doubleDiameter ), 2 ) * 3.1415;
		double doubleLength = 29830 * doubleSurface / ( pow( m_doubleFb[ m_intCurrentVolumeGroup ], 2 ) * \
			m_doubleVb[ m_intCurrentVolumeGroup ] ) - 0.825 * sqrt( doubleSurface );
		char charResult[20];
		sprintf( charResult, "%5.1f", doubleLength );
		m_pqlabelTubeLength[ m_intCurrentVolumeGroup ]->setText( charResult );
		m_pqlabelTubeLength[ m_intCurrentVolumeGroup ]-> show();
	}
	else 
	{
		m_pqlabelTubeLength[ m_intCurrentVolumeGroup ]->setText( i18n( "n. a." ) );
	}
}


void VolumeDialog::closeEvent(QCloseEvent* /*a_pqcloseeventEvent*/ )
{
}
