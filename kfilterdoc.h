/***************************************************************************
                          kfilterdoc.h  -  description
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

#ifndef KFILTERDOC_H
#define KFILTERDOC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QObject>
#include <QList>
#include <QUrl>

#include "driver.h"

class KFilterView;

/** KFilterDoc provides the document object for KFilter.
  *
  * The first Qt6 porting step keeps this class independent from the legacy
  * KDE3 user interface. Dialog creation is intentionally stubbed out until the
  * corresponding dialogs have been ported to Qt6 widgets.
  */
class KFilterDoc : public QObject
{
  Q_OBJECT
  public:
    /** Constructor for the document object of the application. */
    explicit KFilterDoc(QObject *parent = nullptr, const char *name = nullptr);
    /** Destructor for the document object of the application. */
    ~KFilterDoc() override;

//////////////////////////////////////////////////////////

bool Sound( int a_intIndex );
bool Impedance( int a_intIndex );
bool PressureSummary();
bool ImpedanceSummary();
bool PressureScalarSummary();
double DB( double a_doubleA );

void initParamDialog();
void initNetworkDialog();
void initVolumeDialog();
void initToolsWizard();

double  m_doubleXContainer[ 4 ][ 200 ];
driver m_driverDriver[ 4 ];

//////////////////////////////////////////////////////////
    /** adds a view to the document which represents the document contents. Usually this is your main view. */
    void addView(KFilterView *view);
    /** removes a view from the list of currently connected views */
    void removeView(KFilterView *view);
    /** sets the modified flag for the document after a modifying action on the view connected to the document.*/
    void setModified(bool _m=true){ modified=_m; }
    /** returns if the document is modified or not. Use this to determine if your document needs saving by the user on closing.*/
    bool isModified() const { return modified; }
    /** "save modified" - asks the user for saving if the document is modified */
    bool saveModified();
    /** deletes the document's contents */
    void deleteContents();
    /** initializes the document generally */
    bool newDocument();
    /** closes the actual document */
    void closeDocument();
    /** loads the document by filename and format and emits the updateViews() signal */
    bool openDocument(const QUrl& url, const char *format=nullptr);
    /** saves the document under filename and format.*/
    bool saveDocument(const QUrl& url, const char *format=nullptr);
    /** returns the URL of the document */
    const QUrl& URL() const;
    /** sets the URL of the document */
    void setURL(const QUrl& url);

  signals:
    void forceviewrefresh();
    void refreshDialog();

  public slots:
    /** calls repaint() on all views connected to the document object and is called by the view by which the document has been changed.
     * As this view normally repaints itself, it is excluded from the paintEvent.
     */
    void slotUpdateAllViews(KFilterView *sender);

    void viewrefresh();

  public:
    /** the list of the views currently connected to the document */
    static QList<KFilterView*> *pViewList;

  private:
    /** the modified flag of the current document */
    bool modified = false;
    QUrl doc_url;

    void markLoadedContentsReady();

  private slots:
    /** is called when open dialogs
        need an update */
    void slotUpdateAllDialogs();

};

#endif // KFILTERDOC_H
