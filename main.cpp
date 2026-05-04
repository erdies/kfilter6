/***************************************************************************
                          main.cpp  -  description
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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>

#include "kfilter.h"

static const char *description =
	I18N_NOOP("KFilter");
// INSERT A DESCRIPTION FOR YOUR APPLICATION HERE
	
	
static KCmdLineOptions options[] =
{
  { "+[File]", I18N_NOOP("file to open"), 0 },
  { 0, 0, 0 }
  // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main(int argc, char *argv[])
{

	KAboutData aboutData( "kfilter", I18N_NOOP("KFilter"),
		VERSION, description, KAboutData::License_GPL,
		"(c) 2002, Martin Erdtmann", 0, 0, "martin.erdtmann@gmx.de");
	aboutData.addAuthor("Martin Erdtmann",0, "martin.erdtmann@gmx.de");
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication app;
 
  if (app.isRestored())
  {
    RESTORE(KFilterApp);
  }
  else 
  {
    KFilterApp *kfilter = new KFilterApp();
    kfilter->show();

    KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
		
		if (args->count())
		{
        kfilter->openDocumentFile(args->arg(0));
		}
		else
		{
		  kfilter->openDocumentFile();
		}
		args->clear();
  }

  return app.exec();
}  
