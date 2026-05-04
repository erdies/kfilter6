/***************************************************************************
                          kfilter.cpp  -  description
                             -------------------
    begin                : Son Jul 28 17:34:18 CEST 2002
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

// include files for QT
#include <qdir.h>
#include <qprinter.h>
#include <qpainter.h>

// include files for KDE
#include <kiconloader.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kmenubar.h>
#include <kstatusbar.h>
#include <klocale.h>
#include <kconfig.h>
#include <kstdaction.h>
#include <kstddirs.h>

// application specific includes
#include "kfilter.h"
#include "kfilterview.h"
#include "kfilterdoc.h"
#include "colordialog.h"
#include "resource.h"

#define ID_STATUS_MSG 1

KFilterApp::KFilterApp(QWidget* , const char* name):KMainWindow(0, name)
{
  config=kapp->config();
  ///////////////////////////////////////////////////////////////////
  // call inits to invoke all other construction parts
  initStatusBar();
  initActions();
  initDocument();
  initView();

  readOptions();

  ///////////////////////////////////////////////////////////////////
  // disable actions at startup
  fileSave->setEnabled(false);
  fileSaveAs->setEnabled(false);
  filePrint->setEnabled(false);
  editCut->setEnabled(false);
  editCopy->setEnabled(false);
  editPaste->setEnabled(false);
}

KFilterApp::~KFilterApp()
{

}

void KFilterApp::initActions()
{
  fileNewWindow = new KAction(i18n("New &Window"), 0, 0, this, SLOT(slotFileNewWindow()), actionCollection(),"file_new_window");
  fileNew = KStdAction::openNew(this, SLOT(slotFileNew()), actionCollection());
  fileOpen = KStdAction::open(this, SLOT(slotFileOpen()), actionCollection());
  fileOpenRecent = KStdAction::openRecent(this, SLOT(slotFileOpenRecent(const KURL&)), actionCollection());
  fileSave = KStdAction::save(this, SLOT(slotFileSave()), actionCollection());
  fileSaveAs = KStdAction::saveAs(this, SLOT(slotFileSaveAs()), actionCollection());
  fileClose = KStdAction::close(this, SLOT(slotFileClose()), actionCollection());
  filePrint = KStdAction::print(this, SLOT(slotFilePrint()), actionCollection());
  fileQuit = KStdAction::quit(this, SLOT(slotFileQuit()), actionCollection());
  editCut = KStdAction::cut(this, SLOT(slotEditCut()), actionCollection());
  editCopy = KStdAction::copy(this, SLOT(slotEditCopy()), actionCollection());
  editPaste = KStdAction::paste(this, SLOT(slotEditPaste()), actionCollection());
  viewToolBar = KStdAction::showToolbar(this, SLOT(slotViewToolBar()), actionCollection());
  viewStatusBar = KStdAction::showStatusbar(this, SLOT(slotViewStatusBar()), actionCollection());

  driverParameter = new KAction ( i18n("&Parameter"), "driver.xpm",  ALT+Key_1,this, SLOT(slotDriverParam()),  actionCollection(), "driver_parameter" );
  driverNetwork =   new KAction ( i18n("&Network"), "mini-network.xpm", ALT+Key_2,this, SLOT(slotDriverNetwork()),actionCollection(), "driver_network" );
  driverVolume =    new KAction ( i18n("&Volume"), "mini-volume.xpm", ALT+Key_3,this, SLOT(slotDriverVolume()),actionCollection(), "driver_volume" );
  toolsWizard =    new KAction ( i18n("&Wizard"), "filterwiz.xpm", ALT+Key_5,this, SLOT(slotToolsWizard()),actionCollection(), "tools_wizard" );
  optionsColors =    new KAction ( i18n("&Colors"), "fill.png", ALT+Key_4,this, SLOT(slotOptionsColors()),actionCollection(), "options_colors" );

  fileNewWindow->setStatusText(i18n("Opens a new application window"));
  fileNew->setStatusText(i18n("Creates a new document"));
  fileOpen->setStatusText(i18n("Opens an existing document"));
  fileOpenRecent->setStatusText(i18n("Opens a recently used file"));
  fileSave->setStatusText(i18n("Saves the actual document"));
  fileSaveAs->setStatusText(i18n("Saves the actual document as..."));
  fileClose->setStatusText(i18n("Closes the actual document"));
  filePrint ->setStatusText(i18n("Prints out the actual document"));
  fileQuit->setStatusText(i18n("Quits the application"));
  editCut->setStatusText(i18n("Cuts the selected section and puts it to the clipboard"));
  editCopy->setStatusText(i18n("Copies the selected section to the clipboard"));
  editPaste->setStatusText(i18n("Pastes the clipboard contents to actual position"));
  viewToolBar->setStatusText(i18n("Enables/disables the toolbar"));
  viewStatusBar->setStatusText(i18n("Enables/disables the statusbar"));

  driverParameter->setStatusText(i18n("Specifies the Thiele Small parameters of the driver"));

  // use the absolute path to your kfilterui.rc file for testing purpose in createGUI();
  QString qstringPath = locate ("data", "kfilter/kfilterui.rc");
  createGUI(qstringPath);

  ///////////////////////////////////////////////////////////////////
  /*m_DriverMenu = new QPopupMenu();
  m_DriverMenu->insertItem(SmallIcon("driver.xpm"), i18n("&Parameter"), ID_DRIVER_PARAM);
  m_DriverMenu->insertItem(SmallIcon("mini-network.xpm"), i18n("&Network"), ID_DRIVER_NETWORK);
  m_DriverMenu->insertItem(SmallIcon("mini-volume.xpm"), i18n("&Volume"), ID_DRIVER_VOLUME);
  menuBar()->insertItem(i18n("&Driver"), m_DriverMenu);

  connect( m_DriverMenu, SIGNAL(activated(int)), SLOT(commandCallback(int)));
  connect( m_DriverMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
  ///////////////////////////////////////////////////////////////////
  m_ToolsMenu = new QPopupMenu();
  m_ToolsMenu->insertItem(SmallIcon("filterwiz.xpm"), i18n("&FilterWizard"), ID_TOOLS_WIZARD);
  menuBar()->insertItem(i18n("&Tools"), m_ToolsMenu);

  connect( m_ToolsMenu, SIGNAL(activated(int)), SLOT(commandCallback(int)));
  connect( m_ToolsMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));
  ///////////////////////////////////////////////////////////////////
  m_OptionsMenu = new QPopupMenu();
  m_OptionsMenu->insertItem(SmallIcon("fill.png"), i18n("&Colors"), ID_OPTIONS_COLORS);
  menuBar()->insertItem(i18n("&Options"), m_OptionsMenu);

  connect( m_OptionsMenu, SIGNAL(activated(int)), SLOT(commandCallback(int)));
  connect( m_OptionsMenu, SIGNAL(highlighted(int)), SLOT(statusCallback(int)));    */
}


void KFilterApp::initStatusBar()
{
  ///////////////////////////////////////////////////////////////////
  // STATUSBAR
  // TODO: add your own items you need for displaying current application status.
  statusBar()->insertItem(i18n("Ready."), ID_STATUS_MSG);
}

void KFilterApp::initDocument()
{
  doc = new KFilterDoc(this);
  doc->newDocument();
	QObject::connect(doc, SIGNAL(forceviewrefresh() ), this, SLOT( slotrefreshview() ) );
}

void KFilterApp::initView()
{ 
  ////////////////////////////////////////////////////////////////////
  // create the main widget here that is managed by KTMainWindow's view-region and
  // connect the widget to your document to display document contents.

  view = new KFilterView(this);
  doc->addView(view);
  setCentralWidget(view);	
  setCaption(doc->URL().fileName(),false);

}

void KFilterApp::openDocumentFile(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));

    if(!url.isEmpty())
    {
      doc->openDocument(url);
      doc->setURL(url);
      setCaption(url.fileName(), false);
      fileOpenRecent->addURL( url );
      fileSave->setEnabled(false);
      fileSaveAs->setEnabled(true);
    }
  slotStatusMsg(i18n("Ready."));
}


KFilterDoc *KFilterApp::getDocument() const
{
  return doc;
}

void KFilterApp::saveOptions()
{	
  config->setGroup("General Options");
  config->writeEntry("Geometry", size());
  config->writeEntry("Show Toolbar", viewToolBar->isChecked());
  config->writeEntry("Show Statusbar",viewStatusBar->isChecked());
  config->writeEntry("ToolBarPos", (int) toolBar("mainToolBar")->barPos());
  fileOpenRecent->saveEntries(config,"Recent Files");

  config->writeEntry("BackgroundColor", view->backgroundColor());
  config->writeEntry("GridColor", view->gridColor());
  config->writeEntry("PressureColor", view->pressureColor());
  config->writeEntry("ImpedanceColor", view->impedanceColor());
  config->writeEntry("PressureSColor", view->pressureSummaryColor());
  config->writeEntry("ImpedanceSColor", view->impedanceSummaryColor());
  config->writeEntry("ScalarPressureSColor", view->scalarPressureSummaryColor());
}


void KFilterApp::readOptions()
{

  config->setGroup("General Options");

// dieser Eintrag ist temporär, hier werden später die config-files gelesen !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  QColor qcolorBackgroundColor( 150, 52, 52 );
  qcolorBackgroundColor = config->readColorEntry( "BackgroundColor", &qcolorBackgroundColor );
  view->setBackgroundColor( qcolorBackgroundColor );

	QColor qcolorGridColor( black );
	qcolorGridColor = config->readColorEntry( "GridColor", &qcolorGridColor );
	view->setGridColor( qcolorGridColor );

	QColor qcolorPressureColor( white );
	qcolorPressureColor = config->readColorEntry( "PressureColor", &qcolorPressureColor );
	view->setPressureColor( qcolorPressureColor );

	QColor qcolorImpedanceColor( yellow );
	qcolorImpedanceColor = config->readColorEntry( "ImpedanceColor", &qcolorImpedanceColor );
	view->setImpedanceColor( qcolorImpedanceColor );

	QColor qcolorPressureSummaryColor( blue );
	qcolorPressureSummaryColor = config->readColorEntry( "PressureSColor", &qcolorPressureSummaryColor );
	view->setPressureSummaryColor( qcolorPressureSummaryColor );

	QColor qcolorImpedanceSummaryColor( green );
	qcolorImpedanceSummaryColor = config->readColorEntry( "ImpedanceSColor", &qcolorImpedanceSummaryColor );
	view->setImpedanceSummaryColor( qcolorImpedanceSummaryColor );

	QColor qcolorScalarPressureSummaryColor( red );
	qcolorScalarPressureSummaryColor = config->readColorEntry( "ScalarPressureSColor",
		&qcolorScalarPressureSummaryColor );
	view->setScalarPressureSummaryColor( qcolorScalarPressureSummaryColor );





  // bar status settings
  bool bViewToolbar = config->readBoolEntry("Show Toolbar", true);
  viewToolBar->setChecked(bViewToolbar);
  slotViewToolBar();

  bool bViewStatusbar = config->readBoolEntry("Show Statusbar", true);
  viewStatusBar->setChecked(bViewStatusbar);
  slotViewStatusBar();


  // bar position settings
  KToolBar::BarPosition toolBarPos;
  toolBarPos=(KToolBar::BarPosition) config->readNumEntry("ToolBarPos", KToolBar::Top);
  toolBar("mainToolBar")->setBarPos(toolBarPos);
	
  // initialize the recent file list
  fileOpenRecent->loadEntries(config,"Recent Files");

  QSize size=config->readSizeEntry("Geometry");
  if(!size.isEmpty())
  {
    resize(size);
  }
}

void KFilterApp::saveProperties(KConfig *_cfg)
{
  if(doc->URL().fileName()!=i18n("Untitled") && !doc->isModified())
  {
    // saving to tempfile not necessary

  }
  else
  {
    KURL url=doc->URL();	
    _cfg->writeEntry("filename", url.url());
    _cfg->writeEntry("modified", doc->isModified());
    QString tempname = kapp->tempSaveName(url.url());
    QString tempurl= KURL::encode_string(tempname);
    KURL _url(tempurl);
    doc->saveDocument(_url);
  }
}


void KFilterApp::readProperties(KConfig* _cfg)
{
  QString filename = _cfg->readEntry("filename", "");
  KURL url(filename);
  bool modified = _cfg->readBoolEntry("modified", false);
  if(modified)
  {
    bool canRecover;
    QString tempname = kapp->checkRecoverFile(filename, canRecover);
    KURL _url(tempname);
  	
    if(canRecover)
    {
      doc->openDocument(_url);
      doc->setModified();
      setCaption(_url.fileName(),true);
      QFile::remove(tempname);
    }
  }
  else
  {
    if(!filename.isEmpty())
    {
      doc->openDocument(url);
      setCaption(url.fileName(),false);
    }
  }
}		

bool KFilterApp::queryClose()
{
  return doc->saveModified();
}

bool KFilterApp::queryExit()
{
  saveOptions();
  return true;
}

/////////////////////////////////////////////////////////////////////
// SLOT IMPLEMENTATION
/////////////////////////////////////////////////////////////////////

void KFilterApp::slotFileNewWindow()
{
  slotStatusMsg(i18n("Opening a new application window..."));
	
  KFilterApp *new_window= new KFilterApp();
  new_window->show();

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileNew()
{
  slotStatusMsg(i18n("Creating new document..."));

  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {	
    doc->newDocument();
    setCaption(doc->URL().fileName(), false);
  }

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileOpen()
{
  slotStatusMsg(i18n("Opening file..."));
	
  if(!doc->saveModified())
  {
     // here saving wasn't successful

  }
  else
  {
////////////////
/*
		QString qstringFileName = KFileDialog::getOpenFileName( QDir::homeDirPath(),
			i18n( "*.kfp|KFilter project files\n*.*|All FIles"), this, i18n( "Open File..." ) );

		if( !qstringFileName.isEmpty() )
		{
			m_pfilterdocFilterDocument->openDocument( qstringFileName );
			QString qstringCaption = kapp->caption();
			setCaption( qstringCaption + "KFilter: " + m_pfilterdocFilterDocument->getTitle() );
			addRecentFile( qstringFileName );
			EnableCommand( ID_FILE_SAVE );
			EnableCommand( ID_FILE_SAVE_AS );
*/
/////////////////
    KURL url=KFileDialog::getOpenURL(QString::null,
        i18n("*.kfp|KFilter project files\n*|All files"), this, i18n("Open File..."));
    openDocumentFile(url);
/*    if(!url.isEmpty())
    {
      doc->openDocument(url);
      doc->setURL(url);
      setCaption(url.fileName(), false);
      fileOpenRecent->addURL( url );
      fileSave->setEnabled(false);
      fileSaveAs->setEnabled(true);
      slotStatusMsg(i18n("Ready."));
    }
*/
  }
}

void KFilterApp::slotFileOpenRecent(const KURL& url)
{
  slotStatusMsg(i18n("Opening file..."));
	
  if(!doc->saveModified())
  {
     // here saving wasn't successful
  }
  else
  {
    if(!url.isEmpty())
      {    
      doc->openDocument(url);
      doc->setURL(url);
      setCaption(url.fileName(), false);
      fileSave->setEnabled(false);
      fileSaveAs->setEnabled(true);
      }
  }

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileSave()
{
  slotStatusMsg(i18n("Saving file..."));
	
  doc->saveDocument(doc->URL());

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileSaveAs()
{
  slotStatusMsg(i18n("Saving file with a new filename..."));

  KURL url=KFileDialog::getSaveURL(QDir::currentDirPath(),
        i18n("*.kfp|KFilter project files\n*|All files"), this, i18n("Save as..."));
  if(!url.isEmpty())
  {
//////////////////////////////////////////////////////////////////////////////////
    // check for the right extension
    QString filename = url.fileName();
    QString qstringExtension = ".kfp";
    uint uintPosition = filename.findRev( qstringExtension, -1, FALSE );
    if ( uintPosition != filename.length() - qstringExtension.length() )
    {
      // that means that the the extension is not the end of the filename
      filename += qstringExtension;
    }
    url.setFileName(filename);
    doc->setURL(url);
    fileSave->setEnabled(true);
//////////////////////////////////////////////////////////////////////////////////
    doc->saveDocument(url);
    fileOpenRecent->addURL(url);
    setCaption(url.fileName(),doc->isModified());
    fileSave->setEnabled(true);
  }

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileClose()
{
  slotStatusMsg(i18n("Closing file..."));
	
  close();

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFilePrint()
{
  slotStatusMsg(i18n("Printing..."));

  QPrinter printer;
  if (printer.setup(this))
  {
    view->print(&printer);
  }

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotFileQuit()
{
  slotStatusMsg(i18n("Exiting..."));
  saveOptions();
  // close the first window, the list makes the next one the first again.
  // This ensures that queryClose() is called on each window to ask for closing
  KMainWindow* w;
  if(memberList)
  {
    for(w=memberList->first(); w!=0; w=memberList->first())
    {
      // only close the window if the closeEvent is accepted. If the user presses Cancel on the saveModified() dialog,
      // the window and the application stay open.
      if(!w->close())
	break;
    }
  }	
}

void KFilterApp::slotEditCut()
{
  slotStatusMsg(i18n("Cutting selection..."));

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotEditCopy()
{
  slotStatusMsg(i18n("Copying selection to clipboard..."));

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotEditPaste()
{
  slotStatusMsg(i18n("Inserting clipboard contents..."));

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotViewToolBar()
{
  slotStatusMsg(i18n("Toggling toolbar..."));
  ///////////////////////////////////////////////////////////////////
  // turn Toolbar on or off
  if(!viewToolBar->isChecked())
  {
    toolBar("mainToolBar")->hide();
  }
  else
  {
    toolBar("mainToolBar")->show();
  }		

  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotViewStatusBar()
{
  slotStatusMsg(i18n("Toggle the statusbar..."));
  ///////////////////////////////////////////////////////////////////
  //turn Statusbar on or off
  if(!viewStatusBar->isChecked())
  {
    statusBar()->hide();
  }
  else
  {
    statusBar()->show();
  }

  slotStatusMsg(i18n("Ready."));
}


void KFilterApp::slotStatusMsg(const QString &text)
{
  ///////////////////////////////////////////////////////////////////
  // change status message permanently
  statusBar()->clear();
  statusBar()->changeItem(text, ID_STATUS_MSG);
}

void KFilterApp::slotDriverParam()
{
  doc->initParamDialog();
  fileSave->setEnabled(true);
  fileSaveAs->setEnabled(true);
  driverParameter->setEnabled(false);
  doc->setModified();
  slotStatusMsg(i18n("Editing driver parameters..."));
}

void KFilterApp::slotDriverNetwork()
{
  doc->initNetworkDialog();
  fileSave->setEnabled(true);
  fileSaveAs->setEnabled(true);
  driverNetwork->setEnabled(false);
  doc->setModified();
  slotStatusMsg(i18n("Editing network parameters..."));
}

void KFilterApp::slotDriverVolume()
{
  doc->initVolumeDialog();
  fileSave->setEnabled(true);
  fileSaveAs->setEnabled(true);
  driverVolume->setEnabled(false);
  doc->setModified();
  slotStatusMsg(i18n("Editing box parameters..."));
}

void KFilterApp::slotToolsWizard()
{
  doc->initToolsWizard();
  fileSave->setEnabled(true);
  fileSaveAs->setEnabled(true);
  doc->setModified();
}

void KFilterApp::slotOptionsColors()
{
  ColorDialog* colordialogDialog = new ColorDialog( view, i18n("Color Selection") );
  colordialogDialog->show();
  optionsColors->setEnabled(false);
  slotStatusMsg(i18n("Editing plot and background colors..."));
  QObject::connect( colordialogDialog, SIGNAL( isclosed() ), this, SLOT( slotEnableOptionsColors() ) );
}

void KFilterApp::slotrefreshview()
{
  view->repaint();
}

void KFilterApp::slotEnableDriverParam()
{
  driverParameter->setEnabled(true);
  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotEnableDriverNetwork()
{
  driverNetwork->setEnabled(true);
  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotEnableDriverVolume()
{
  driverVolume->setEnabled(true);
  slotStatusMsg(i18n("Ready."));
}

void KFilterApp::slotEnableOptionsColors()
{
  optionsColors->setEnabled(true);
  slotStatusMsg(i18n("Ready."));
}

