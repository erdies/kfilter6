/***************************************************************************
networkdialog.cpp  -  description
-------------------
begin                : Sat Jun 10 2000
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


#include "networkdialog.h"


NetworkDialog::NetworkDialog(const char *name,driver *driverptr0,driver *driverptr1,driver *driverptr2,driver *driverptr3)
{
	m_pdriverDriver[ 0 ] = driverptr0;
	m_pdriverDriver[ 1 ] = driverptr1;
	m_pdriverDriver[ 2 ] = driverptr2;
	m_pdriverDriver[ 3 ] = driverptr3;

	for (int i=0;i<4;i++) {p_prevScroll[i]=0; p_prevCircuit[i]=0; };
	
	setCaption(name);
	
	QWidget* pqwidgetDriver0 = new QWidget( this );
	m_pqwidgetDriver[0]=pqwidgetDriver0;
	pqwidgetDriver0->setFixedSize(500,225);
	pqwidgetDriver0->setGeometry(0,0,200,60);
	for (int i=0;i<48;i++) 
	{
		f0[i] = new QLineEdit(pqwidgetDriver0);
	}
	QPushButton* pqpushbuttonDriver0 = new QPushButton(i18n("Reset"),pqwidgetDriver0);
	pqpushbuttonDriver0 -> setGeometry(450,195,42,22);
	QObject::connect(pqpushbuttonDriver0, SIGNAL(pressed() ), this, SLOT( slot_reset0() ) );

	QPushButton* pqpushbuttonDriver01 = new QPushButton(i18n("Draw!"),pqwidgetDriver0);
	pqpushbuttonDriver01 -> setGeometry(450,100,42,22);
	QObject::connect(pqpushbuttonDriver01, SIGNAL(pressed() ), this, SLOT( slot_draw0() ) );

	drawEdit(f0);
	QLabel* pic = new QLabel(pqwidgetDriver0);
	
	//QString net3path = kapp->kde_icondir(); net3path.append("/net3.xpm");
	QString net3path = locate ("data", "kfilter/pics/net3.xpm");
	//printf ("%s\n", (const char *)net3path);
	
	pic->setPixmap(QPixmap(net3path));
	pic->setGeometry(420,10,30,47);
	Units(pqwidgetDriver0);
	addTab (pqwidgetDriver0, i18n("Driver 1"));
	QObject::connect(f0[1], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_1(const QString&) ) );
	QObject::connect(f0[7], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_2(const QString&) ) );
	QObject::connect(f0[13], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_3(const QString&) ) );
	QObject::connect(f0[19], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_4(const QString&) ) );
	QObject::connect(f0[25], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_5(const QString&) ) );
	QObject::connect(f0[31], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_6(const QString&) ) );
	QObject::connect(f0[37], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_7(const QString&) ) );
	QObject::connect(f0[43], SIGNAL(textChanged(const QString&) ), this, SLOT( field0_8(const QString&) ) );
	
	QWidget* pqwidgetDriver1 = new QWidget( this );
	m_pqwidgetDriver[1]=pqwidgetDriver1;
	pqwidgetDriver1->setFixedSize(500,225);
	pqwidgetDriver1->setGeometry(0,0,200,60);
	for (int i=0;i<48;i++) 
	{
		f1[i] = new QLineEdit(pqwidgetDriver1);
	}
	QPushButton* pqpushbuttonDriver1 = new QPushButton(i18n("Reset"),pqwidgetDriver1);
	pqpushbuttonDriver1 -> setGeometry(450,195,42,22);
	QObject::connect(pqpushbuttonDriver1, SIGNAL(pressed() ), this, SLOT( slot_reset1() ) );

	QPushButton* pqpushbuttonDriver1b = new QPushButton(i18n("Draw!"),pqwidgetDriver1);
	pqpushbuttonDriver1b -> setGeometry(450,100,42,22);
	QObject::connect(pqpushbuttonDriver1b, SIGNAL(pressed() ), this, SLOT( slot_draw1() ) );

	drawEdit(f1);
	QLabel* pic1 = new QLabel(pqwidgetDriver1);
	pic1->setPixmap(QPixmap(net3path));
	pic1->setGeometry(420,10,30,47);
	Units(pqwidgetDriver1);
	addTab (pqwidgetDriver1, i18n("Driver 2"));
	QObject::connect(f1[1], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_1(const QString&) ) );
	QObject::connect(f1[7], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_2(const QString&) ) );
	QObject::connect(f1[13], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_3(const QString&) ) );
	QObject::connect(f1[19], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_4(const QString&) ) );
	QObject::connect(f1[25], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_5(const QString&) ) );
	QObject::connect(f1[31], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_6(const QString&) ) );
	QObject::connect(f1[37], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_7(const QString&) ) );
	QObject::connect(f1[43], SIGNAL(textChanged(const QString&) ), this, SLOT( field1_8(const QString&) ) );
	
	QWidget* pqwidgetDriver2 = new QWidget( this );
	m_pqwidgetDriver[2]=pqwidgetDriver2;
	pqwidgetDriver2->setFixedSize(500,225);
	pqwidgetDriver2->setGeometry(0,0,200,60);
	for (int i=0;i<48;i++) 
	{
		f2[i] = new QLineEdit(pqwidgetDriver2);
	}

	QPushButton* pqpushbuttonDriver2 = new QPushButton(i18n("Reset"),pqwidgetDriver2);
	pqpushbuttonDriver2 -> setGeometry(450,195,42,22);
	QObject::connect(pqpushbuttonDriver2, SIGNAL(pressed() ), this, SLOT( slot_reset2() ) );

	QPushButton* pqpushbuttonDriver2b = new QPushButton(i18n("Draw!"),pqwidgetDriver2);
	pqpushbuttonDriver2b -> setGeometry(450,100,42,22);
	QObject::connect(pqpushbuttonDriver2b, SIGNAL(pressed() ), this, SLOT( slot_draw2() ) );


	drawEdit(f2);
	QLabel* pic2= new QLabel(pqwidgetDriver2);
	pic2->setPixmap(QPixmap(net3path));
	pic2->setGeometry(420,10,30,47);
	Units(pqwidgetDriver2);
	addTab (pqwidgetDriver2, i18n("Driver 3"));
	QObject::connect(f2[1], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_1(const QString&) ) );
	QObject::connect(f2[7], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_2(const QString&) ) );
	QObject::connect(f2[13], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_3(const QString&) ) );
	QObject::connect(f2[19], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_4(const QString&) ) );
	QObject::connect(f2[25], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_5(const QString&) ) );
	QObject::connect(f2[31], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_6(const QString&) ) );
	QObject::connect(f2[37], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_7(const QString&) ) );
	QObject::connect(f2[43], SIGNAL(textChanged(const QString&) ), this, SLOT( field2_8(const QString&) ) );
	
	QWidget* pqwidgetDriver3 = new QWidget( this );
	m_pqwidgetDriver[3]=pqwidgetDriver3;
	pqwidgetDriver3->setFixedSize(500,225);
	pqwidgetDriver3->setGeometry(0,0,200,60);
	for (int i=0;i<48;i++) 
	{
		f3[i] = new QLineEdit(pqwidgetDriver3);
	}

	QPushButton* pqpushbuttonDriver3 = new QPushButton(i18n("Reset"),pqwidgetDriver3);
	pqpushbuttonDriver3 -> setGeometry(450,195,42,22);
	QObject::connect(pqpushbuttonDriver3, SIGNAL(pressed() ), this, SLOT( slot_reset3() ) );

	QPushButton* pqpushbuttonDriver3b = new QPushButton(i18n("Draw!"),pqwidgetDriver3);
	pqpushbuttonDriver3b -> setGeometry(450,100,42,22);
	QObject::connect(pqpushbuttonDriver3b, SIGNAL(pressed() ), this, SLOT( slot_draw3() ) );

	drawEdit(f3);
	QLabel* pic3 = new QLabel(pqwidgetDriver3);
	pic3->setPixmap(QPixmap(net3path));
	pic3->setGeometry(420,10,30,47);
	Units(pqwidgetDriver3);
	addTab (pqwidgetDriver3, i18n("Driver 4"));
	QObject::connect(f3[1], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_1(const QString&) ) );
	QObject::connect(f3[7], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_2(const QString&) ) );
	QObject::connect(f3[13], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_3(const QString&) ) );
	QObject::connect(f3[19], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_4(const QString&) ) );
	QObject::connect(f3[25], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_5(const QString&) ) );
	QObject::connect(f3[31], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_6(const QString&) ) );
	QObject::connect(f3[37], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_7(const QString&) ) );
	QObject::connect(f3[43], SIGNAL(textChanged(const QString&) ), this, SLOT( field3_8(const QString&) ) );
	
	this->setFixedSize(500,205);
	
	setOKButton();
	setDefaultButton(i18n("Apply"));
	setCancelButton();
	
	QObject::connect(this, SIGNAL(applyButtonPressed() ), this, SLOT( applyClicked() ) );
	QObject::connect(this, SIGNAL(defaultButtonPressed() ), this, SLOT( defaultClicked() ) );
	QObject::connect(this, SIGNAL(cancelButtonPressed() ), this, SLOT( cancelClicked() ) );
	for (int i=0;i<48;i++) 
	{
		QObject::connect(f0[i], SIGNAL(textChanged(const QString&) ), this, SLOT( updateBuffer(const QString&) ) );
		QObject::connect(f1[i], SIGNAL(textChanged(const QString&) ), this, SLOT( updateBuffer(const QString&) ) );
		QObject::connect(f2[i], SIGNAL(textChanged(const QString&) ), this, SLOT( updateBuffer(const QString&) ) );
		QObject::connect(f3[i], SIGNAL(textChanged(const QString&) ), this, SLOT( updateBuffer(const QString&) ) ); 
	}
	
	initValues();
}

NetworkDialog::~NetworkDialog()
{
	
	for (int i=0;i<48;i++) 
	{
		delete f0[i];
	}
	for (int i=0;i<48;i++) 
	{
		delete f1[i];
	}
	for (int i=0;i<48;i++) 
	{
		delete f2[i];
	}
	for (int i=0;i<48;i++) 
	{
		delete f3[i];
	}
	for (int i=0;i<4;i++) 
	{
		delete m_pqwidgetDriver[i];
	}
}

void NetworkDialog::applyClicked()
{
	// copy data from dialog into applicaiton
	updateValues();
	m_pdriverDriver[ 0 ]->setmodified();
	m_pdriverDriver[ 1 ]->setmodified();
	m_pdriverDriver[ 2 ]->setmodified();
	m_pdriverDriver[ 3 ]->setmodified();
	emit paramchanged();
	emit isclosed();
	accept();
}

void NetworkDialog::cancelClicked()
{
	//don´t copy data from the dialog into application
	emit isclosed();
	reject();
}

void NetworkDialog::defaultClicked()
{
	updateValues();
	m_pdriverDriver[ 0 ]->setmodified();
	m_pdriverDriver[ 1 ]->setmodified();
	m_pdriverDriver[ 2 ]->setmodified();
	m_pdriverDriver[ 3 ]->setmodified();
	emit paramchanged();
}

void NetworkDialog::field0_1(const QString &text)
{
	
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(20,10,50,47);
	pic->show();
}

void NetworkDialog::field0_2(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(70,10,50,47);
	pic->show();
}

void NetworkDialog::field0_3(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(120,10,50,47);
	pic->show();
}

void NetworkDialog::field0_4(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(170,10,50,47);
	pic->show();
}

void NetworkDialog::field0_5(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(220,10,50,47);
	pic->show();
}

void NetworkDialog::field0_6(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(270,10,50,47);
	pic->show();
}

void NetworkDialog::field0_7(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(320,10,50,47);
	pic->show();
}

void NetworkDialog::field0_8(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[0]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(370,10,50,47);
	pic->show();
}

void NetworkDialog::field1_1(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(20,10,50,47);
	pic->show();
}

void NetworkDialog::field1_2(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(70,10,50,47);
	pic->show();
}

void NetworkDialog::field1_3(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(120,10,50,47);
	pic->show();
}

void NetworkDialog::field1_4(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(170,10,50,47);
	pic->show();
}

void NetworkDialog::field1_5(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(220,10,50,47);
	pic->show();
}

void NetworkDialog::field1_6(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(270,10,50,47);
	pic->show();
}

void NetworkDialog::field1_7(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(320,10,50,47);
	pic->show();
}

void NetworkDialog::field1_8(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[1]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(370,10,50,47);
	pic->show();
}

void NetworkDialog::field2_1(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(20,10,50,47);
	pic->show();
}

void NetworkDialog::field2_2(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(70,10,50,47);
	pic->show();
}

void NetworkDialog::field2_3(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(120,10,50,47);
	pic->show();
}

void NetworkDialog::field2_4(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(170,10,50,47);
	pic->show();
}

void NetworkDialog::field2_5(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(220,10,50,47);
	pic->show();
}

void NetworkDialog::field2_6(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(270,10,50,47);
	pic->show();
}

void NetworkDialog::field2_7(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(320,10,50,47);
	pic->show();
}

void NetworkDialog::field2_8(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[2]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(370,10,50,47);
	pic->show();
}

void NetworkDialog::field3_1(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(20,10,50,47);
	pic->show();
}

void NetworkDialog::field3_2(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(70,10,50,47);
	pic->show();
}

void NetworkDialog::field3_3(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(120,10,50,47);
	pic->show();
}

void NetworkDialog::field3_4(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(170,10,50,47);
	pic->show();
}

void NetworkDialog::field3_5(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(220,10,50,47);
	pic->show();
}

void NetworkDialog::field3_6(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(270,10,50,47);
	pic->show();
}

void NetworkDialog::field3_7(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0) 
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(320,10,50,47);
	pic->show();
}

void NetworkDialog::field3_8(const QString &text)
{
	QString netpath = locate ("data", "kfilter/pics/net.xpm");
	QString net2path = locate ("data", "kfilter/pics/net2.xpm");
	//QString netpath = kapp->kde_icondir(); netpath.append("/net.xpm");
	//QString net2path = kapp->kde_icondir(); net2path.append("/net2.xpm");
	QLabel* pic = new QLabel(m_pqwidgetDriver[3]);
	if (atof((const char *)text)==0.0)
	{
		pic->setPixmap(QPixmap(net2path));
	}
	else 
	{
		pic->setPixmap(QPixmap(netpath));
	}
	pic->setGeometry(370,10,50,47);
	pic->show();
}


void NetworkDialog::initValues()
{
	char text[20];
	for (int i=0;i<48;i++)	
	{
		if (i==1||i==4||i==7||i==10||i==13||i==16||i==19||i==22||i==25||i==28||i==31||i==34||i==37||i==40||i==43||i==46)
		{
			value0[i] = m_pdriverDriver[ 0 ]->getUnit(i+1) * 1000000;
			value1[i] = m_pdriverDriver[ 1 ]->getUnit(i+1) * 1000000;
			value2[i] = m_pdriverDriver[ 2 ]->getUnit(i+1) * 1000000;
			value3[i] = m_pdriverDriver[ 3 ]->getUnit(i+1) * 1000000;
		}
		else
		{
			if (i==2||i==5||i==8||i==11||i==14||i==17||i==20||i==23||i==26||i==29||i==32||i==35||i==38||i==41||i==44||i==47)
			{
				value0[i] = m_pdriverDriver[ 0 ]->getUnit(i+1) * 1000;
				value1[i] = m_pdriverDriver[ 1 ]->getUnit(i+1) * 1000;
				value2[i] = m_pdriverDriver[ 2 ]->getUnit(i+1) * 1000;
				value3[i] = m_pdriverDriver[ 3 ]->getUnit(i+1) * 1000;
			}
			else
			{
				value0[i] = m_pdriverDriver[ 0 ]->getUnit(i+1);
				value1[i] = m_pdriverDriver[ 1 ]->getUnit(i+1);						
				value2[i] = m_pdriverDriver[ 2 ]->getUnit(i+1);						
				value3[i] = m_pdriverDriver[ 3 ]->getUnit(i+1);
			}
		}
		double i1=value0[i];
		double i2=value1[i];
		double i3=value2[i];
		double i4=value3[i];
		
		sprintf (text, "%4.1f", i1);
		f0[i]->setText(text);
		sprintf (text, "%4.1f", i2);
		f1[i]->setText(text);
		sprintf (text, "%4.1f", i3);
		f2[i]->setText(text);
		sprintf (text, "%4.1f", i4);
		f3[i]->setText(text);
	}
}

void NetworkDialog::updateValues()
{
	for (int i=0;i<48;i++)
	{
		if (i==1||i==4||i==7||i==10||i==13||i==16||i==19||i==22||i==25||i==28||i==31||i==34||i==37||i==40||i==43||i==46)
		{
			m_pdriverDriver[ 0 ]->setUnit(i+1, value0[i] / 1000000);
			m_pdriverDriver[ 1 ]->setUnit(i+1, value1[i] / 1000000);
			m_pdriverDriver[ 2 ]->setUnit(i+1, value2[i] / 1000000);
			m_pdriverDriver[ 3 ]->setUnit(i+1, value3[i] / 1000000);
		}
		else
		{
			if (i==2||i==5||i==8||i==11||i==14||i==17||i==20||i==23||i==26||i==29||i==32||i==35||i==38||i==41||i==44||i==47)
			{
				m_pdriverDriver[ 0 ]->setUnit(i+1, value0[i] / 1000);
				m_pdriverDriver[ 1 ]->setUnit(i+1, value1[i] / 1000);
				m_pdriverDriver[ 2 ]->setUnit(i+1, value2[i] / 1000);
				m_pdriverDriver[ 3 ]->setUnit(i+1, value3[i] / 1000);
			}
			else
			{
				m_pdriverDriver[ 0 ]->setUnit(i+1, value0[i]);
				m_pdriverDriver[ 1 ]->setUnit(i+1, value1[i]);
				m_pdriverDriver[ 2 ]->setUnit(i+1, value2[i]);
				m_pdriverDriver[ 3 ]->setUnit(i+1, value3[i]);
			}
		}
	}
}

void NetworkDialog::updateBuffer(const QString& textparam)
{
	for (int i=0;i<48;i++)
	{
		value0[i] = atof(f0[i]->text());
		value1[i] = atof(f1[i]->text());
		value2[i] = atof(f2[i]->text());
		value3[i] = atof(f3[i]->text());
	}
}


void NetworkDialog::drawEdit(QLineEdit* p[48])
{
	int a =70;
	int delta=25;
	int b = a+delta;
	int c = b+delta;
	int d = c+delta;
	int e = d+delta;
	int f = e+delta;
	
	p[0]->setGeometry(20,a,35,20);
	p[1]->setGeometry(20,b,35,20);
	p[2]->setGeometry(20,c,35,20);
	p[3]->setGeometry(20,d,35,20);
	p[4]->setGeometry(20,e,35,20);
	p[5]->setGeometry(20,f,35,20);
	
	p[6]->setGeometry(70,a,35,20);
	p[7]->setGeometry(70,b,35,20);
	p[8]->setGeometry(70,c,35,20);
	p[9]->setGeometry(70,d,35,20);
	p[10]->setGeometry(70,e,35,20);
	p[11]->setGeometry(70,f,35,20);
	
	p[12]->setGeometry(120,a,35,20);
	p[13]->setGeometry(120,b,35,20);
	p[14]->setGeometry(120,c,35,20);
	p[15]->setGeometry(120,d,35,20);
	p[16]->setGeometry(120,e,35,20);
	p[17]->setGeometry(120,f,35,20);
	
	p[18]->setGeometry(170,a,35,20);
	p[19]->setGeometry(170,b,35,20);
	p[20]->setGeometry(170,c,35,20);
	p[21]->setGeometry(170,d,35,20);
	p[22]->setGeometry(170,e,35,20);
	p[23]->setGeometry(170,f,35,20);
	
	p[24]->setGeometry(220,a,35,20);
	p[25]->setGeometry(220,b,35,20);
	p[26]->setGeometry(220,c,35,20);
	p[27]->setGeometry(220,d,35,20);
	p[28]->setGeometry(220,e,35,20);
	p[29]->setGeometry(220,f,35,20);
	
	p[30]->setGeometry(270,a,35,20);
	p[31]->setGeometry(270,b,35,20);
	p[32]->setGeometry(270,c,35,20);
	p[33]->setGeometry(270,d,35,20);
	p[34]->setGeometry(270,e,35,20);
	p[35]->setGeometry(270,f,35,20);
	
	p[36]->setGeometry(320,a,35,20);
	p[37]->setGeometry(320,b,35,20);
	p[38]->setGeometry(320,c,35,20);
	p[39]->setGeometry(320,d,35,20);
	p[40]->setGeometry(320,e,35,20);
	p[41]->setGeometry(320,f,35,20);
	
	p[42]->setGeometry(370,a,35,20);
	p[43]->setGeometry(370,b,35,20);
	p[44]->setGeometry(370,c,35,20);
	p[45]->setGeometry(370,d,35,20);
	p[46]->setGeometry(370,e,35,20);
	p[47]->setGeometry(370,f,35,20);
}

void NetworkDialog::Units(QWidget *tab)
{
	QLabel *label0_a = new QLabel(i18n("Ohms"),tab);
	label0_a->setGeometry(415,70,32,20);
	QLabel *label0_b = new QLabel(i18n("\265F"),tab);
	label0_b->setGeometry(415,95,30,20);
	QLabel *label0_c = new QLabel(i18n("mH"),tab);
	label0_c->setGeometry(415,120,30,20);
	QLabel *label0_d = new QLabel(i18n("Ohms"),tab);
	label0_d->setGeometry(415,145,32,20);
	QLabel *label0_e = new QLabel(i18n("\265F"),tab);
	label0_e->setGeometry(415,170,30,20);
	QLabel *label0_f = new QLabel(i18n("mH"),tab);
	label0_f->setGeometry(415,195,30,20);
}

/** The dialog gets updated values
from the application */
void NetworkDialog::slot_initnewvalues()
{
	initValues();
	//emit paramchanged();
}

void NetworkDialog::slot_reset0()
{
	for (int i=0;i<49;i++)
		f0[i] -> setText("0.0");
}

void NetworkDialog::slot_reset1()
{
	for (int i=0;i<49;i++)
		f1[i] -> setText("0.0");
}

void NetworkDialog::slot_reset2()
{
	for (int i=0;i<49;i++)
		f2[i] -> setText("0.0");
}

void NetworkDialog::slot_reset3()
{
	for (int i=0;i<49;i++)
		f3[i] -> setText("0.0");
}

void NetworkDialog::slot_draw0()
{
	double network[49];
  for (int i=1;i<49;i++)	network[i] = m_pdriverDriver[0]->getUnit(i);
	QScrollView *pscroll = new QScrollView();
  CircuitOut *CircuitOut1 = new CircuitOut( pscroll->viewport() );
	delete p_prevScroll[0];
	p_prevScroll[0] = pscroll;
	pscroll->addChild(CircuitOut1);
  pscroll->setCaption(m_pdriverDriver[0]->GetTitle() );
	pscroll->setGeometry(0,0,700,320);
  pscroll->setMaximumHeight(315);
  pscroll->setMaximumWidth(1275);
	CircuitOut1->setvalues(network);
	CircuitOut1->setGeometry(0,0,1270,310);
	pscroll->show();
}

void NetworkDialog::slot_draw1()
{
	double network[49];
  for (int i=1;i<49;i++)	network[i] = m_pdriverDriver[1]->getUnit(i);
	QScrollView *pscroll = new QScrollView();
  CircuitOut *CircuitOut1 = new CircuitOut( pscroll->viewport() );
	delete p_prevScroll[1];
	p_prevScroll[1] = pscroll;
	pscroll->addChild(CircuitOut1);
  pscroll->setCaption(m_pdriverDriver[1]->GetTitle() );
	pscroll->setGeometry(0,0,700,320);
  pscroll->setMaximumHeight(315);
  pscroll->setMaximumWidth(1275);
	CircuitOut1->setvalues(network);
	CircuitOut1->setGeometry(0,0,1270,310);
	pscroll->show();
}

void NetworkDialog::slot_draw2()
{
	double network[49];
  for (int i=1;i<49;i++)	network[i] = m_pdriverDriver[2]->getUnit(i);
	QScrollView *pscroll = new QScrollView();
  CircuitOut *CircuitOut1 = new CircuitOut( pscroll->viewport() );
	delete p_prevScroll[2];
	p_prevScroll[2] = pscroll;
	pscroll->addChild(CircuitOut1);
  pscroll->setCaption(m_pdriverDriver[2]->GetTitle() );
	pscroll->setGeometry(0,0,700,320);
  pscroll->setMaximumHeight(315);
  pscroll->setMaximumWidth(1275);
	CircuitOut1->setvalues(network);
	CircuitOut1->setGeometry(0,0,1270,310);
	pscroll->show();
}

void NetworkDialog::slot_draw3()
{
	double network[49];
  for (int i=1;i<49;i++)	network[i] = m_pdriverDriver[3]->getUnit(i);
	QScrollView *pscroll = new QScrollView();
  CircuitOut *CircuitOut1 = new CircuitOut( pscroll->viewport() );
	delete p_prevScroll[3];
	p_prevScroll[3] = pscroll;
	pscroll->addChild(CircuitOut1);
  pscroll->setCaption(m_pdriverDriver[3]->GetTitle() );
	pscroll->setGeometry(0,0,700,320);
  pscroll->setMaximumHeight(315);
  pscroll->setMaximumWidth(1275);
	CircuitOut1->setvalues(network);
	CircuitOut1->setGeometry(0,0,1270,310);
	pscroll->show();
}

/**  */
void NetworkDialog::closeEvent(QCloseEvent *ce)
{
}
