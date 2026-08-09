/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterdoc.h"

#include "kfilterprojectio.h"
#include "kfilterfrequencygrid.h"

#include <QDebug>
#include <QString>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#ifdef KFILTER_ENABLE_LEGACY_UI
#include "kfilterview.h"
#endif

namespace
{
QString localFilePathFromUrl(const QUrl& url)
{
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }

    if (url.scheme().isEmpty()) {
        return url.toString();
    }

    return QString();
}

constexpr int PressureSampleCount = static_cast<int>(KFilterFrequencyCount);
}

QList<KFilterView*> *KFilterDoc::pViewList = nullptr;

KFilterDoc::KFilterDoc(QObject *parent, const char *name)
    : QObject(parent)
{
  if (name != nullptr) {
    setObjectName(QString::fromLatin1(name));
  }

  if(!pViewList)
  {
    pViewList = new QList<KFilterView*>();
  }
}

KFilterDoc::~KFilterDoc()
{
}


KFilterMeasurementCurve& KFilterDoc::splCorrectionCurve(int driverIndex)
{
  return m_splCorrectionCurves.at(static_cast<std::size_t>(driverIndex));
}

const KFilterMeasurementCurve& KFilterDoc::splCorrectionCurve(int driverIndex) const
{
  return m_splCorrectionCurves.at(static_cast<std::size_t>(driverIndex));
}

bool KFilterDoc::hasMeasurementCurves() const
{
  return std::any_of(m_splCorrectionCurves.cbegin(),
                     m_splCorrectionCurves.cend(),
                     [](const KFilterMeasurementCurve& curve) { return !curve.isEmpty(); });
}

bool KFilterDoc::hasMergeableMeasurementCurves() const
{
  return std::any_of(m_splCorrectionCurves.cbegin(),
                     m_splCorrectionCurves.cend(),
                     [](const KFilterMeasurementCurve& curve) { return curve.size() >= 2; });
}

bool KFilterDoc::clearMeasurementCurve(int driverIndex)
{
  if (driverIndex < 0 || driverIndex >= static_cast<int>(m_splCorrectionCurves.size())) {
    return false;
  }

  const std::size_t index = static_cast<std::size_t>(driverIndex);
  KFilterMeasurementCurve& curve = m_splCorrectionCurves[index];
  const bool curveChanged = !curve.isEmpty();
  const bool hiddenChanged = m_measurementHiddenForDrivers[index];
  curve.clear();
  m_measurementHiddenForDrivers[index] = false;

  const bool mergeChanged = m_measurementMergeEnabled && !hasMergeableMeasurementCurves();
  if (mergeChanged) {
    m_measurementMergeEnabled = false;
  }
  if (curveChanged || hiddenChanged || mergeChanged) {
    invalidateSplCorrectionCaches();
  }

  return curveChanged || hiddenChanged || mergeChanged;
}

bool KFilterDoc::clearMeasurementCurves()
{
  const bool hiddenChanged = std::any_of(m_measurementHiddenForDrivers.cbegin(),
                                         m_measurementHiddenForDrivers.cend(),
                                         [](bool hidden) { return hidden; });
  const bool changed = hasMeasurementCurves() || m_measurementMergeEnabled || hiddenChanged;
  for (KFilterMeasurementCurve& curve : m_splCorrectionCurves) {
    curve.clear();
  }
  m_measurementMergeEnabled = false;
  m_measurementHiddenForDrivers.fill(false);
  if (changed) {
    invalidateSplCorrectionCaches();
  }
  return changed;
}

bool KFilterDoc::measurementMergeEnabled() const
{
  return m_measurementMergeEnabled;
}

bool KFilterDoc::setMeasurementMergeEnabled(bool enabled)
{
  const bool effectiveEnabled = enabled && hasMergeableMeasurementCurves();
  if (m_measurementMergeEnabled == effectiveEnabled) {
    return false;
  }

  m_measurementMergeEnabled = effectiveEnabled;
  invalidateSplCorrectionCaches();
  return true;
}

bool KFilterDoc::measurementHiddenForDriver(int driverIndex) const
{
  if (driverIndex < 0 || driverIndex >= static_cast<int>(m_measurementHiddenForDrivers.size())) {
    return false;
  }

  return m_measurementHiddenForDrivers[static_cast<std::size_t>(driverIndex)];
}

bool KFilterDoc::setMeasurementHiddenForDriver(int driverIndex, bool hidden)
{
  if (driverIndex < 0 || driverIndex >= static_cast<int>(m_measurementHiddenForDrivers.size())) {
    return false;
  }

  const std::size_t index = static_cast<std::size_t>(driverIndex);
  const bool effectiveHidden = hidden && !m_splCorrectionCurves[index].isEmpty();
  if (m_measurementHiddenForDrivers[index] == effectiveHidden) {
    return false;
  }

  m_measurementHiddenForDrivers[index] = effectiveHidden;
  m_splCorrectionCaches[index].valid = false;
  return true;
}

const KFilterDoc::SplCorrectionCache* KFilterDoc::ensureSplCorrectionCache(int driverIndex) const
{
  if (driverIndex < 0 || driverIndex >= static_cast<int>(m_splCorrectionCurves.size())) {
    return nullptr;
  }

  const std::size_t cacheIndex = static_cast<std::size_t>(driverIndex);
  const KFilterMeasurementCurve& curve = m_splCorrectionCurves[cacheIndex];
  SplCorrectionCache& cache = m_splCorrectionCaches[cacheIndex];
  if (cache.valid &&
      cache.curveRevision == curve.revision() &&
      cache.mergeEnabled == m_measurementMergeEnabled &&
      cache.measurementHidden == m_measurementHiddenForDrivers[cacheIndex]) {
    return &cache;
  }

  cache.correctionDb.fill(0.0);
  cache.amplitudeFactor.fill(1.0);
  cache.curveRevision = curve.revision();
  cache.mergeEnabled = m_measurementMergeEnabled;
  cache.measurementHidden = m_measurementHiddenForDrivers[cacheIndex];
  cache.active = false;
  cache.valid = true;

  if (!m_measurementMergeEnabled ||
      m_measurementHiddenForDrivers[cacheIndex] ||
      curve.size() < 2 ||
      curve.isNeutral() ||
      !curve.overlapsFrequencyRange(kfilterFrequencyGridHz().front(),
                                    kfilterFrequencyGridHz().back())) {
    return &cache;
  }

  bool haveEffectiveCorrection = false;
  for (int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex) {
    double correctionDb = 0.0;
    const double frequencyHz =
        kfilterFrequencyGridHz()[static_cast<std::size_t>(sampleIndex)];
    if (!curve.interpolatedValueAt(frequencyHz, correctionDb) ||
        !std::isfinite(correctionDb)) {
      continue;
    }

    cache.correctionDb[static_cast<std::size_t>(sampleIndex)] = correctionDb;
    if (correctionDb == 0.0) {
      continue;
    }

    const double amplitudeFactor = std::pow(10.0, correctionDb / 20.0);
    if (std::isfinite(amplitudeFactor) && amplitudeFactor > 0.0) {
      cache.amplitudeFactor[static_cast<std::size_t>(sampleIndex)] = amplitudeFactor;
    }
    haveEffectiveCorrection = true;
  }

  cache.active = haveEffectiveCorrection;
  return &cache;
}

void KFilterDoc::invalidateSplCorrectionCaches()
{
  for (SplCorrectionCache& cache : m_splCorrectionCaches) {
    cache.valid = false;
  }
}

bool KFilterDoc::splCorrectionActiveForDriver(int driverIndex) const
{
  const SplCorrectionCache* cache = ensureSplCorrectionCache(driverIndex);
  return cache != nullptr && cache->active;
}

double KFilterDoc::splCorrectionDb(int driverIndex, int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= PressureSampleCount) {
    return 0.0;
  }

  const SplCorrectionCache* cache = ensureSplCorrectionCache(driverIndex);
  if (cache == nullptr) {
    return 0.0;
  }

  return cache->correctionDb[static_cast<std::size_t>(sampleIndex)];
}

double KFilterDoc::splCorrectionAmplitudeFactor(int driverIndex, int sampleIndex) const
{
  if (sampleIndex < 0 || sampleIndex >= PressureSampleCount) {
    return 1.0;
  }

  const SplCorrectionCache* cache = ensureSplCorrectionCache(driverIndex);
  if (cache == nullptr) {
    return 1.0;
  }

  return cache->amplitudeFactor[static_cast<std::size_t>(sampleIndex)];
}


ActiveFilterChain& KFilterDoc::activeFilterChain(int driverIndex)
{
  return m_activeFilterChains.at(static_cast<std::size_t>(driverIndex));
}

const ActiveFilterChain& KFilterDoc::activeFilterChain(int driverIndex) const
{
  return m_activeFilterChains.at(static_cast<std::size_t>(driverIndex));
}

KFilterDoc::ActiveFilterChains& KFilterDoc::activeFilterChains()
{
  return m_activeFilterChains;
}

const KFilterDoc::ActiveFilterChains& KFilterDoc::activeFilterChains() const
{
  return m_activeFilterChains;
}

const ActiveFilterResponse& KFilterDoc::activeFilterResponse(int driverIndex) const
{
  const std::size_t index = static_cast<std::size_t>(driverIndex);
  return m_activeFilterResponseCaches.at(index).responseFor(m_activeFilterChains.at(index));
}

void KFilterDoc::addView(KFilterView *view)
{
  if (view != nullptr) {
    pViewList->append(view);
  }
}

void KFilterDoc::removeView(KFilterView *view)
{
  pViewList->removeAll(view);
}
void KFilterDoc::setURL(const QUrl &url)
{
  doc_url=url;
}

const QUrl& KFilterDoc::URL() const
{
  return doc_url;
}

void KFilterDoc::slotUpdateAllViews(KFilterView *sender)
{
#ifdef KFILTER_ENABLE_LEGACY_UI
  if(pViewList)
  {
    for(KFilterView *w : *pViewList)
    {
      if(w!=sender)
        w->repaint();
    }
  }
#else
  Q_UNUSED(sender);
#endif
}

bool KFilterDoc::saveModified()
{
  if (!modified) {
    return true;
  }

  // The interactive save prompt depends on the legacy KDE3 main window and is
  // intentionally disabled until the application shell has been ported.
  return false;
}

void KFilterDoc::closeDocument()
{
  deleteContents();
}

bool KFilterDoc::newDocument()
{
  modified=false;
  doc_url = QUrl(QStringLiteral("Untitled"));
  deleteContents();
  viewrefresh();
  return true;
}

bool KFilterDoc::openDocument(const QUrl& url, const char *format /*=nullptr*/)
{
  Q_UNUSED(format);

  const QString filePath = localFilePathFromUrl(url);
  if (filePath.isEmpty()) {
    qWarning() << "Cannot open non-local KFilter project URL:" << url;
    return false;
  }

  QString errorMessage;
  if (!KFilterProjectIo::loadFromFile(filePath,
                                      m_driverDriver,
                                      m_splCorrectionCurves,
                                      m_measurementMergeEnabled,
                                      m_measurementHiddenForDrivers,
                                      m_activeFilterChains,
                                      &errorMessage)) {
    qWarning().noquote() << errorMessage;
    return false;
  }

  setURL(url);
  markLoadedContentsReady();
  return true;
}

bool KFilterDoc::saveDocument(const QUrl& url, const char *format /*=nullptr*/)
{
  Q_UNUSED(format);

  const QString filePath = localFilePathFromUrl(url);
  if (filePath.isEmpty()) {
    qWarning() << "Cannot save non-local KFilter project URL:" << url;
    return false;
  }

  QString errorMessage;
  if (!KFilterProjectIo::saveToFile(filePath,
                                    m_driverDriver,
                                    m_splCorrectionCurves,
                                    m_measurementMergeEnabled,
                                    m_measurementHiddenForDrivers,
                                    m_activeFilterChains,
                                    &errorMessage)) {
    qWarning().noquote() << errorMessage;
    return false;
  }

  setURL(url);
  modified=false;
  return true;
}

void KFilterDoc::deleteContents()
{
  for (int intI = 0; intI < 4; intI++ )
  {
    m_driverDriver[ intI ].initContents();
    m_splCorrectionCurves[static_cast<std::size_t>(intI)].clear();
  }
  m_measurementMergeEnabled = false;
  m_measurementHiddenForDrivers.fill(false);
  resetActiveFilterChains();
  invalidateSplCorrectionCaches();
  modified = false;
}

void KFilterDoc::resetActiveFilterChains()
{
  for (ActiveFilterChain& chain : m_activeFilterChains) {
    chain = ActiveFilterChain{};
  }
}

void KFilterDoc::markLoadedContentsReady()
{
  invalidateSplCorrectionCaches();
  for ( int intI = 0; intI < 4; intI++ )
  {
    m_driverDriver[ intI ].Berechneparameter();
    m_driverDriver[ intI ].setmodified();
  }
  emit forceviewrefresh();
  modified = false;
}

double KFilterDoc::DB( double a_doubleA )
{
  return ( 8.685889638 * std::log( a_doubleA ) );
}

void KFilterDoc::initParamDialog()
{
  // Temporarily disabled during the Qt6/KF6 bring-up. The driver parameter
  // dialog still depends on Qt3-era widget APIs and will be ported separately.
}

void KFilterDoc::initNetworkDialog()
{
  // Temporarily disabled during the Qt6/KF6 bring-up. The network dialog still
  // depends on Qt3-era widget APIs and will be ported separately.
}

void KFilterDoc::initVolumeDialog()
{
  // Temporarily disabled during the Qt6/KF6 bring-up. The volume dialog still
  // depends on Qt3-era widget APIs and will be ported separately.
}

void KFilterDoc::initToolsWizard()
{
  // The KDE3 wizard component is intentionally excluded during the first
  // Qt6/KF6 bring-up.
}

std::complex<double> KFilterDoc::effectivePressureSample(
    int driverIndex,
    int sampleIndex,
    const ActiveFilterResponse& activeFilter,
    const SplCorrectionCache* correctionCache) const
{
  if (driverIndex < 0 || driverIndex >= 4 ||
      sampleIndex < 0 || sampleIndex >= PressureSampleCount) {
    return {};
  }

  const int resultIndex = sampleIndex * 2;
  std::complex<double> sample{
      m_driverDriver[driverIndex].ResultSchall[resultIndex],
      m_driverDriver[driverIndex].ResultSchall[resultIndex + 1]};

  // Patch 179: the active-filter stage is the first complex post-driver stage.
  // Unsupported/invalid chains deliberately bypass here; the response status is
  // surfaced in the Active Filter dialog rather than silently applying a
  // partial chain.
  sample = applyActiveFilterResponseSample(activeFilter,
                                           static_cast<std::size_t>(sampleIndex),
                                           sample);

  // Measurement correction remains a real amplitude factor and is applied to
  // the same effective complex sample used by the individual and summary paths.
  if (correctionCache != nullptr && correctionCache->active) {
    sample *= correctionCache->amplitudeFactor[static_cast<std::size_t>(sampleIndex)];
  }

  return sample;
}

bool KFilterDoc::Sound( int a_intIndex )
{
  if (m_driverDriver[ a_intIndex ].PressureisActive )
  {
    m_driverDriver[ a_intIndex ].Schall();
    const ActiveFilterResponse& activeFilter = activeFilterResponse(a_intIndex);
    const SplCorrectionCache* correctionCache = ensureSplCorrectionCache(a_intIndex);
    for (int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex)
    {
      m_doubleXContainer[ a_intIndex ][ sampleIndex ] =
          DB(std::abs(effectivePressureSample(a_intIndex,
                                             sampleIndex,
                                             activeFilter,
                                             correctionCache)));
    }
  }
  return m_driverDriver[ a_intIndex ].PressureisActive;
}

bool KFilterDoc::Impedance( int a_intIndex )
{
	if (m_driverDriver[ a_intIndex ].ImpedanzisActive)
	{
		m_driverDriver[ a_intIndex ].Impedanz();
		int intJ = 0;
		for (int intI = 0; intI < 300; intI = intI + 2 )
		{
			m_doubleXContainer[ a_intIndex ][ intJ ] = std::sqrt( std::pow( \
				m_driverDriver[ a_intIndex ].ResultImpedanz[ intI ], 2.0 ) + \
				std::pow( m_driverDriver[ a_intIndex ].ResultImpedanz[ intI + 1 ], 2.0 ) );
			intJ++;
		}
	}
	return m_driverDriver[ a_intIndex ].ImpedanzisActive;
}

bool KFilterDoc::PressureSummary()
{
	std::array<std::complex<double>, PressureSampleCount> complexSum{};

	////////////////////////////// calculate vector summary for active drivers
	for( int intIndex = 0; intIndex < 4; intIndex++ )
	{
		if ( m_driverDriver[ intIndex ].SummaryisActive )
		{
			m_driverDriver[ intIndex ].Schall();
			const ActiveFilterResponse& activeFilter = activeFilterResponse(intIndex);
			const SplCorrectionCache* correctionCache = ensureSplCorrectionCache(intIndex);
			for ( int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex )
			{
				complexSum[static_cast<std::size_t>(sampleIndex)] +=
					effectivePressureSample(intIndex,
					                        sampleIndex,
					                        activeFilter,
					                        correctionCache);
			}
		}
	}

	////////////////////////////// vector summary becomes real summary
	for (int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex )
	{
		m_doubleXContainer[ 0 ][ sampleIndex ] =
			DB(std::abs(complexSum[static_cast<std::size_t>(sampleIndex)]));
	}
	///////////////////////////////
	return ( m_driverDriver[ 0 ].SummaryisActive || m_driverDriver[ 1 ].SummaryisActive || \
		m_driverDriver[ 2 ].SummaryisActive || m_driverDriver[ 3 ].SummaryisActive);
}

bool KFilterDoc::PressureScalarSummary()
{
	///////////////////init m_doubleXContainer
	for ( int intI = 0; intI < PressureSampleCount; intI++ )
	{
		m_doubleXContainer[ 0 ][ intI ] = 0;
	}

	for ( int intIndex = 0; intIndex < 4; intIndex++ )
	{
		if ( m_driverDriver[ intIndex ].ScalarSummaryisActive )
		{
			m_driverDriver[ intIndex ].Schall();
			const ActiveFilterResponse& activeFilter = activeFilterResponse(intIndex);
			const SplCorrectionCache* correctionCache = ensureSplCorrectionCache(intIndex);
			for ( int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex )
			{
				const std::complex<double> sample =
					effectivePressureSample(intIndex,
					                        sampleIndex,
					                        activeFilter,
					                        correctionCache);
				m_doubleXContainer[ 0 ][ sampleIndex ] += std::norm(sample);
			}
		}
	}
	for (int intI = 0; intI < PressureSampleCount; intI++ )
	{
		m_doubleXContainer[ 0 ][ intI ] = DB( std::sqrt( m_doubleXContainer[ 0 ][ intI ] ) );
	}
	///////////////////////////////
	return ( m_driverDriver[ 0 ].ScalarSummaryisActive || m_driverDriver[ 1 ].ScalarSummaryisActive || \
		m_driverDriver[ 2 ].ScalarSummaryisActive || m_driverDriver[ 3 ].ScalarSummaryisActive );
}

bool KFilterDoc::ImpedanceSummary()
{

	double doubleSum[ 300 ];
	for ( int intZ = 0; intZ < 300; intZ++ )
	{
		doubleSum[ intZ ] = 0;
	}
	////////////////////////////// calculate vector summary for active drivers
	for ( int intIndex = 0; intIndex < 4; intIndex++ )
	{
		if ( m_driverDriver[ intIndex ].ImpedanzSummaryisActive )
		{
			m_driverDriver[ intIndex ].Impedanz();
			m_driverDriver[ intIndex ].invertImpedanz();
			for ( int intI = 0; intI < 300; intI++ )
			{
				doubleSum[ intI ]= doubleSum[ intI ] + m_driverDriver[ intIndex ].ResultImpedanz[ intI ];
			}
			m_driverDriver[ intIndex ].invertImpedanz();
		}
	}
	////////////////////////////// vector summary becomes real summary
	int intZ = 0;
	for ( int intI = 0; intI < 300; intI = intI + 2 )
	{
		m_doubleXContainer[ 0 ][ intZ ] = 1.0 / ( std::sqrt( std::pow( doubleSum[ intI ], 2.0 ) +\
			std::pow( doubleSum[ intI + 1 ], 2.0 ) ) );
		intZ++;
	}
	///////////////////////////////
	return ( m_driverDriver[ 0 ].ImpedanzSummaryisActive || m_driverDriver[ 1 ].ImpedanzSummaryisActive || \
		m_driverDriver[ 2 ].ImpedanzSummaryisActive || m_driverDriver[ 3 ].ImpedanzSummaryisActive);
}

/** is called when open dialogs
need an update */
void KFilterDoc::slotUpdateAllDialogs()
{
  emit refreshDialog();
  emit forceviewrefresh();
}

void KFilterDoc::viewrefresh()
{
  emit forceviewrefresh();
}


