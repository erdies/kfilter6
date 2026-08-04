/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterdoc.h"

#include "kfilterprojectio.h"

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

constexpr int PressureSampleCount = 150;
constexpr double MinimumPressureFrequencyHz = 20.0;
constexpr double PressureFrequencyStep = 1.047128548;

const std::array<double, PressureSampleCount>& pressureSampleFrequenciesHz()
{
    static const std::array<double, PressureSampleCount> frequencies = []() {
        std::array<double, PressureSampleCount> values{};
        values[0] = MinimumPressureFrequencyHz;
        for (int sampleIndex = 1; sampleIndex < PressureSampleCount; ++sampleIndex) {
            values[static_cast<std::size_t>(sampleIndex)] =
                values[static_cast<std::size_t>(sampleIndex - 1)] * PressureFrequencyStep;
        }
        return values;
    }();
    return frequencies;
}
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

  KFilterMeasurementCurve& curve =
      m_splCorrectionCurves[static_cast<std::size_t>(driverIndex)];
  const bool curveChanged = !curve.isEmpty();
  curve.clear();

  const bool mergeChanged = m_measurementMergeEnabled && !hasMergeableMeasurementCurves();
  if (mergeChanged) {
    m_measurementMergeEnabled = false;
  }

  return curveChanged || mergeChanged;
}

bool KFilterDoc::clearMeasurementCurves()
{
  const bool changed = hasMeasurementCurves() || m_measurementMergeEnabled;
  for (KFilterMeasurementCurve& curve : m_splCorrectionCurves) {
    curve.clear();
  }
  m_measurementMergeEnabled = false;
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
  return true;
}

double KFilterDoc::splCorrectionDb(int driverIndex, int sampleIndex) const
{
  if (!m_measurementMergeEnabled ||
      driverIndex < 0 || driverIndex >= static_cast<int>(m_splCorrectionCurves.size()) ||
      sampleIndex < 0 || sampleIndex >= PressureSampleCount) {
    return 0.0;
  }

  const KFilterMeasurementCurve& curve = splCorrectionCurve(driverIndex);
  if (curve.size() < 2) {
    return 0.0;
  }

  double correctionDb = 0.0;
  const double frequencyHz =
      pressureSampleFrequenciesHz()[static_cast<std::size_t>(sampleIndex)];
  if (!curve.interpolatedValueAt(frequencyHz, correctionDb) ||
      !std::isfinite(correctionDb)) {
    return 0.0;
  }

  return correctionDb;
}

double KFilterDoc::splCorrectionAmplitudeFactor(int driverIndex, int sampleIndex) const
{
  const double amplitudeFactor =
      std::pow(10.0, splCorrectionDb(driverIndex, sampleIndex) / 20.0);
  return std::isfinite(amplitudeFactor) && amplitudeFactor > 0.0
             ? amplitudeFactor
             : 1.0;
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
  modified = false;
}

void KFilterDoc::markLoadedContentsReady()
{
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

bool KFilterDoc::Sound( int a_intIndex )
{
  if (m_driverDriver[ a_intIndex ].PressureisActive )
  {
    m_driverDriver[ a_intIndex ].Schall();
    int intJ = 0;
    for (int intI = 0; intI < 300; intI = intI + 2 )
    {
      m_doubleXContainer[ a_intIndex ][ intJ ] = DB( std::sqrt( std::pow( \
        m_driverDriver[ a_intIndex ].ResultSchall[ intI ], 2.0 ) + \
        std::pow( m_driverDriver[ a_intIndex ].ResultSchall[ intI + 1 ], 2.0 ) ) );
      intJ++;
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
	/////////////////////////////init temp variable
	double doubleSum[ 300 ];
	for( int intZ = 0; intZ < 300; intZ++ )
	{
		doubleSum[ intZ ] = 0.0;
	}
	////////////////////////////// calculate vector summary for active drivers
	for( int intIndex = 0; intIndex < 4; intIndex++ )
	{
		if ( m_driverDriver[ intIndex ].SummaryisActive )
		{
			m_driverDriver[ intIndex ].Schall();
			for ( int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex )
			{
				const int resultIndex = sampleIndex * 2;
				const double amplitudeFactor =
					splCorrectionAmplitudeFactor(intIndex, sampleIndex);
				doubleSum[ resultIndex ] +=
					m_driverDriver[ intIndex ].ResultSchall[ resultIndex ] * amplitudeFactor;
				doubleSum[ resultIndex + 1 ] +=
					m_driverDriver[ intIndex ].ResultSchall[ resultIndex + 1 ] * amplitudeFactor;
			}
		}
	}
	////////////////////////////// vector summary becomes real summary
	int intZ = 0;
	for (int intI = 0; intI < 300; intI = intI + 2 )
	{
		m_doubleXContainer[ 0 ][ intZ ] = DB( std::sqrt( std::pow( doubleSum[ intI ], 2.0 ) + \
			std::pow( doubleSum[ intI + 1 ], 2.0 ) ) );
		intZ++;
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
			for ( int sampleIndex = 0; sampleIndex < PressureSampleCount; ++sampleIndex )
			{
				//		turbo pascal version			dB0-round(	ln(std::sqrt(sqr(qx1)+sqr(qy1)+sqr(qx2)+sqr(qy2)+sqr(qx3)+sqr(qy3))	)
				//  	old C version 				m_doubleXContainer[0][j] = m_doubleXContainer[0][j] + std::sqrt(std::pow(m_driverDriver[index].ResultSchall[i],2.0)+std::pow(m_driverDriver[index].ResultSchall[i+1],2.0));
				const int resultIndex = sampleIndex * 2;
				const double amplitudeFactor =
					splCorrectionAmplitudeFactor(intIndex, sampleIndex);
				const double correctedReal =
					m_driverDriver[ intIndex ].ResultSchall[ resultIndex ] * amplitudeFactor;
				const double correctedImaginary =
					m_driverDriver[ intIndex ].ResultSchall[ resultIndex + 1 ] * amplitudeFactor;
				m_doubleXContainer[ 0 ][ sampleIndex ] +=
					std::pow( correctedReal, 2.0 ) + std::pow( correctedImaginary, 2.0 );
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


