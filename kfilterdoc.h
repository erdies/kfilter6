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
#include <QUrl>

#include <array>
#include <cstdint>
#include <complex>

#include "activefiltermodel.h"
#include "activefilterresponse.h"
#include "bafflemodel.h"
#include "baffleresponse.h"
#include "floorreflectionmodel.h"
#include "floorreflectionprocessing.h"
#include "driver.h"
#include "kfilterfrequencygrid.h"
#include "kfiltermeasurementcurve.h"


/** KFilterDoc provides the document and simulation state for KFilter.
  *
  * User-interface dialogs and save prompts are owned by KFilterQt6App. The
  * document remains independent of those widgets and exposes state, project I/O
  * and refresh signals only.
  */
class KFilterDoc : public QObject
{
  Q_OBJECT
  public:
    using ActiveFilterChains = std::array<ActiveFilterChain, 4>;
    using BaffleSettingsPerDriver = std::array<BaffleSettings, 4>;
    using FloorReflectionSettingsPerDriver = std::array<FloorReflectionSettings, 4>;
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

FloorReflectionSettings& floorReflectionSettings(int driverIndex);
const FloorReflectionSettings& floorReflectionSettings(int driverIndex) const;
FloorReflectionSettingsPerDriver& floorReflectionSettingsPerDriver();
const FloorReflectionSettingsPerDriver& floorReflectionSettingsPerDriver() const;
const FloorReflectionResponse& floorReflectionResponse(int driverIndex) const;

//////////////////////////////////////////////////////////
    /** sets the modified flag for the document after a modifying action on the view connected to the document.*/
    void setModified(bool _m=true){ modified=_m; }
    /** returns if the document is modified or not. Use this to determine if your document needs saving by the user on closing.*/
    bool isModified() const { return modified; }
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

  public slots:
    void viewrefresh();

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
    FloorReflectionSettingsPerDriver m_floorReflectionSettings{};
    mutable std::array<BaffleResponseCache, 4> m_baffleResponseCaches{};
    mutable std::array<FloorReflectionResponseCache, 4> m_floorReflectionResponseCaches{};

    const SplCorrectionCache* ensureSplCorrectionCache(int driverIndex) const;
    std::complex<double> effectivePressureSample(
        int driverIndex,
        int sampleIndex,
        const ActiveFilterResponse& activeFilter,
        const BaffleResponse& baffle,
        const FloorReflectionResponse& floorReflection,
        const SplCorrectionCache* correctionCache) const;
    void invalidateSplCorrectionCaches();
    void resetActiveFilterChains();
    void resetBaffleSettings();
    void resetFloorReflectionSettings();
    void markLoadedContentsReady();

};

#endif // KFILTERDOC_H
