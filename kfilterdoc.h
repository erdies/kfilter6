/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef KFILTERDOC_H
#define KFILTERDOC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QObject>
#include <QList>
#include <QUrl>

#include <array>
#include <cstdint>
#include <complex>

#include "activefiltermodel.h"
#include "activefilterresponse.h"
#include "bafflemodel.h"
#include "baffleresponse.h"
#include "driver.h"
#include "kfiltermeasurementcurve.h"

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
    using ActiveFilterChains = std::array<ActiveFilterChain, 4>;
    using BaffleSettingsPerDriver = std::array<BaffleSettings, 4>;
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

KFilterMeasurementCurve& splCorrectionCurve(int driverIndex);
const KFilterMeasurementCurve& splCorrectionCurve(int driverIndex) const;
bool hasMeasurementCurves() const;
bool hasMergeableMeasurementCurves() const;
bool clearMeasurementCurve(int driverIndex);
bool clearMeasurementCurves();
bool measurementMergeEnabled() const;
bool setMeasurementMergeEnabled(bool enabled);
bool measurementHiddenForDriver(int driverIndex) const;
bool setMeasurementHiddenForDriver(int driverIndex, bool hidden);
bool splCorrectionActiveForDriver(int driverIndex) const;
double splCorrectionDb(int driverIndex, int sampleIndex) const;
double splCorrectionAmplitudeFactor(int driverIndex, int sampleIndex) const;

ActiveFilterChain& activeFilterChain(int driverIndex);
const ActiveFilterChain& activeFilterChain(int driverIndex) const;
ActiveFilterChains& activeFilterChains();
const ActiveFilterChains& activeFilterChains() const;
const ActiveFilterResponse& activeFilterResponse(int driverIndex) const;

BaffleSettings& baffleSettings(int driverIndex);
const BaffleSettings& baffleSettings(int driverIndex) const;
BaffleSettingsPerDriver& baffleSettingsPerDriver();
const BaffleSettingsPerDriver& baffleSettingsPerDriver() const;
const BaffleResponse& baffleResponse(int driverIndex) const;

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
    static constexpr int SplCorrectionSampleCount = static_cast<int>(KFilterFrequencyCount);

    struct SplCorrectionCache
    {
        std::array<double, SplCorrectionSampleCount> correctionDb{};
        std::array<double, SplCorrectionSampleCount> amplitudeFactor{};
        std::uint64_t curveRevision = 0;
        bool mergeEnabled = false;
        bool measurementHidden = false;
        bool active = false;
        bool valid = false;
    };

    std::array<KFilterMeasurementCurve, 4> m_splCorrectionCurves;
    mutable std::array<SplCorrectionCache, 4> m_splCorrectionCaches;
    bool m_measurementMergeEnabled = false;
    std::array<bool, 4> m_measurementHiddenForDrivers{};
    ActiveFilterChains m_activeFilterChains{};
    mutable std::array<ActiveFilterResponseCache, 4> m_activeFilterResponseCaches{};
    BaffleSettingsPerDriver m_baffleSettings{};
    mutable std::array<BaffleResponseCache, 4> m_baffleResponseCaches{};

    const SplCorrectionCache* ensureSplCorrectionCache(int driverIndex) const;
    std::complex<double> effectivePressureSample(
        int driverIndex,
        int sampleIndex,
        const ActiveFilterResponse& activeFilter,
        const BaffleResponse& baffle,
        const SplCorrectionCache* correctionCache) const;
    void invalidateSplCorrectionCaches();
    void resetActiveFilterChains();
    void resetBaffleSettings();
    void markLoadedContentsReady();

  private slots:
    /** is called when open dialogs
        need an update */
    void slotUpdateAllDialogs();

};

#endif // KFILTERDOC_H
