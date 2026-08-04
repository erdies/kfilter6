/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterview.h"

#include "kfilterdoc.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineF>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRectF>
#include <QResizeEvent>
#include <QStringList>
#include <Qt>

#include <algorithm>
#include <cmath>
#include <limits>

KFilterView::KFilterView(KFilterDoc *document, QWidget *parent)
    : QWidget(parent),
      m_document(document)
{
    setAutoFillBackground(true);
    setPlotColorSettings(defaultPlotColorSettings());

    setMinimumSize(640, 360);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    Start = 125.6637061;
    Faktor = 1.047128548;
    initXvalue();
}

KFilterView::~KFilterView() = default;

KFilterDoc *KFilterView::getDocument() const
{
    return m_document;
}

bool KFilterView::beginSplCorrectionDrawing(int driverIndex)
{
    if (m_document == nullptr || driverIndex < 0 || driverIndex >= 4) {
        return false;
    }

    if (m_measurementDrawingActive) {
        cancelSplCorrectionDrawing();
    }

    m_activeMeasurementDriverIndex = driverIndex;
    KFilterMeasurementCurve& curve = m_document->splCorrectionCurve(driverIndex);
    m_measurementCurveSnapshot = curve;
    curve.clear();
    m_measurementDrawingActive = true;
    m_measurementCursorValid = false;

    setCursor(Qt::CrossCursor);
    setFocus(Qt::OtherFocusReason);
    update();

    emit measurementDrawingStateChanged(true, driverIndex);
    emit measurementStatusMessage(
        tr("Drawing SPL correction for %1: left-click waypoints from left to right; Enter finishes, Escape cancels.")
            .arg(measurementDriverLabel(driverIndex)));
    return true;
}

bool KFilterView::finishSplCorrectionDrawing()
{
    if (!m_measurementDrawingActive) {
        return false;
    }

    const int driverIndex = m_activeMeasurementDriverIndex;
    const qsizetype pointCount = m_document->splCorrectionCurve(driverIndex).size();
    if (pointCount == 0) {
        m_document->splCorrectionCurve(driverIndex) = m_measurementCurveSnapshot;
        endMeasurementDrawing();
        emit measurementStatusMessage(
            tr("No waypoint was set for %1; the previous correction curve was restored.")
                .arg(measurementDriverLabel(driverIndex)));
        return true;
    }

    m_measurementCurveSnapshot.clear();
    endMeasurementDrawing();
    emit measurementStatusMessage(
        tr("SPL correction curve for %1 finished with %2 waypoint(s).")
            .arg(measurementDriverLabel(driverIndex))
            .arg(static_cast<qlonglong>(pointCount)));
    emit measurementProjectStateChanged();
    return true;
}

bool KFilterView::cancelSplCorrectionDrawing()
{
    if (!m_measurementDrawingActive) {
        return false;
    }

    const int driverIndex = m_activeMeasurementDriverIndex;
    m_document->splCorrectionCurve(driverIndex) = m_measurementCurveSnapshot;
    m_measurementCurveSnapshot.clear();
    endMeasurementDrawing();
    emit measurementStatusMessage(
        tr("SPL correction drawing for %1 cancelled; the previous correction curve was restored.")
            .arg(measurementDriverLabel(driverIndex)));
    return true;
}

bool KFilterView::undoSplCorrectionWaypoint()
{
    if (!m_measurementDrawingActive) {
        return false;
    }

    KFilterMeasurementCurve& curve = m_document->splCorrectionCurve(m_activeMeasurementDriverIndex);
    if (!curve.removeLastPoint()) {
        emit measurementStatusMessage(tr("There is no measurement waypoint to undo."));
        return false;
    }

    update();
    emit measurementStatusMessage(
        tr("Last correction waypoint removed from %1; %2 waypoint(s) remain.")
            .arg(measurementDriverLabel(m_activeMeasurementDriverIndex))
            .arg(static_cast<qlonglong>(curve.size())));
    return true;
}

bool KFilterView::clearMeasurementCurve(int driverIndex)
{
    if (m_measurementDrawingActive) {
        cancelSplCorrectionDrawing();
    }

    if (m_document == nullptr || !m_document->clearMeasurementCurve(driverIndex)) {
        return false;
    }

    emit measurementProjectStateChanged();
    update();
    return true;
}

void KFilterView::clearMeasurementCurves()
{
    if (m_measurementDrawingActive) {
        cancelSplCorrectionDrawing();
    }

    if (m_document != nullptr && m_document->clearMeasurementCurves()) {
        emit measurementProjectStateChanged();
    }
    update();
}

bool KFilterView::hasMeasurementCurves() const
{
    return m_document != nullptr && m_document->hasMeasurementCurves();
}

bool KFilterView::hasMergeableMeasurementCurves() const
{
    return m_document != nullptr && m_document->hasMergeableMeasurementCurves();
}

bool KFilterView::measurementDrawingActive() const
{
    return m_measurementDrawingActive;
}

int KFilterView::activeMeasurementDriverIndex() const
{
    return m_activeMeasurementDriverIndex;
}

bool KFilterView::mergeMeasurementsEnabled() const
{
    return m_document != nullptr && m_document->measurementMergeEnabled();
}

void KFilterView::setMergeMeasurementsEnabled(bool enabled)
{
    if (m_document == nullptr || !m_document->setMeasurementMergeEnabled(enabled)) {
        return;
    }

    update();
    emit measurementProjectStateChanged();
}

double KFilterView::xToFrequencyHz(double x) const
{
    if (!std::isfinite(x) || width() <= 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    constexpr double MinimumFrequencyHz = 20.0;
    constexpr double FrequencyRatio = 1000.0;
    return MinimumFrequencyHz * std::exp((x / static_cast<double>(width())) * std::log(FrequencyRatio));
}

double KFilterView::frequencyHzToX(double frequencyHz) const
{
    if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0 || width() <= 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    constexpr double MinimumFrequencyHz = 20.0;
    constexpr double FrequencyRatio = 1000.0;
    return static_cast<double>(width()) *
           std::log(frequencyHz / MinimumFrequencyHz) / std::log(FrequencyRatio);
}

double KFilterView::yToPressureDb(double y) const
{
    if (!std::isfinite(y) || height() <= 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return 10.0 - (60.0 * y / static_cast<double>(height()));
}

double KFilterView::pressureDbToY(double valueDb) const
{
    if (!std::isfinite(valueDb) || height() <= 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return static_cast<double>(height()) / 6.0 -
           valueDb * static_cast<double>(height()) / 60.0;
}

bool KFilterView::printRendering() const
{
    return m_printRendering;
}

void KFilterView::setPrintRendering(bool enabled)
{
    m_printRendering = enabled;
}

void KFilterView::endMeasurementDrawing()
{
    m_measurementDrawingActive = false;
    m_measurementCursorValid = false;
    m_measurementCurveSnapshot.clear();
    m_activeMeasurementDriverIndex = -1;
    unsetCursor();
    update();
    emit measurementDrawingStateChanged(false, -1);
}

QString KFilterView::measurementDriverLabel(int driverIndex) const
{
    const QString fallback = tr("Driver %1").arg(driverIndex + 1);
    if (m_document == nullptr || driverIndex < 0 || driverIndex >= 4) {
        return fallback;
    }

    const QString title = m_document->m_driverDriver[driverIndex].GetTitle().trimmed();
    if (title.isEmpty() || title == QStringLiteral("This is a default driver")) {
        return fallback;
    }

    return tr("Driver %1 (%2)").arg(driverIndex + 1).arg(title);
}

QString KFilterView::measurementPointerText(const QPointF& position) const
{
    const double frequencyHz = xToFrequencyHz(position.x());
    const double pressureDb = yToPressureDb(position.y());
    if (!std::isfinite(frequencyHz) || !std::isfinite(pressureDb)) {
        return QString();
    }

    const QString frequencyText = frequencyHz >= 1000.0
                                      ? tr("%1 kHz").arg(frequencyHz / 1000.0, 0, 'f', 2)
                                      : tr("%1 Hz").arg(frequencyHz, 0, 'f', 1);
    return tr("%1: %2, %3 dB — left-click to set a waypoint.")
        .arg(measurementDriverLabel(m_activeMeasurementDriverIndex),
             frequencyText,
             QString::number(pressureDb, 'f', 1));
}

bool KFilterView::measurementMergeAppliedForDriver(int driverIndex) const
{
    return m_document != nullptr && m_document->measurementMergeEnabled() &&
           driverIndex >= 0 && driverIndex < 4 &&
           m_document->splCorrectionCurve(driverIndex).size() >= 2;
}

double KFilterView::effectivePressureDb(int driverIndex, int sampleIndex, double simulatedDb) const
{
    if (m_document == nullptr || !std::isfinite(simulatedDb)) {
        return simulatedDb;
    }

    return simulatedDb + m_document->splCorrectionDb(driverIndex, sampleIndex);
}

void KFilterView::initXvalue()
{
    Xvalue[0] = Start;
    for (int i = 1; i < 150; i++) {
        Xvalue[i] = Xvalue[i - 1] * Faktor;
    }
}

int KFilterView::XK(double x) const
{
    if (x <= 0.0 || width() <= 0) {
        return 0;
    }

    const double w = width();
    return static_cast<int>(w * 0.144764827 * std::log(x * 0.007957747155));
    // f*1/20 -> f*1/(20*2*pi). The calculation uses omega instead of frequency.
    // "width * ..." means width * 1/ln(20000/20).
}

int KFilterView::YScale(double value, int flag) const
{
    if (!std::isfinite(value)) {
        return height();
    }

    const double h = height();
    if (flag == 0) {
        return static_cast<int>(h / 6.0 - value * h / 60.0);
    }
    if (flag == 1) {
        return static_cast<int>(5.0 * h / 6.0 - value * h / 60.0);
    }
    return static_cast<int>(h / 6.0 - value * h / 60.0);
}

KFilterView::CurveLabelAnchor KFilterView::findLastVisibleCurvePoint(const double values[200], int type, int driverIndex) const
{
    const QRectF visibleRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
    if (!visibleRect.isValid()) {
        return {};
    }

    QPointF points[150];
    bool valid[150] = {};
    for (int i = 0; i < 150; i++) {
        if (!std::isfinite(values[i])) {
            continue;
        }
        double value = values[i];
        if (type == 0 && driverIndex >= 0) {
            value = effectivePressureDb(driverIndex, i, value);
        }
        points[i] = QPointF(XK(Xvalue[i]), YScale(value, type));
        valid[i] = true;
    }

    auto bestRightmostIntersection = [&](const QPointF& first, const QPointF& second) -> CurveLabelAnchor {
        const QLineF segment(first, second);
        const QLineF edges[] = {
            QLineF(visibleRect.topLeft(), visibleRect.topRight()),
            QLineF(visibleRect.topRight(), visibleRect.bottomRight()),
            QLineF(visibleRect.bottomRight(), visibleRect.bottomLeft()),
            QLineF(visibleRect.bottomLeft(), visibleRect.topLeft())
        };

        CurveLabelAnchor best;
        for (const QLineF& edge : edges) {
            QPointF intersection;
            if (segment.intersects(edge, &intersection) == QLineF::BoundedIntersection) {
                if (!best.valid || intersection.x() > best.point.x()) {
                    best.point = intersection;
                    best.valid = true;
                }
            }
        }
        return best;
    };

    for (int i = 148; i >= 0; i--) {
        if (!valid[i] || !valid[i + 1]) {
            continue;
        }

        const QPointF first = points[i];
        const QPointF second = points[i + 1];
        if (visibleRect.contains(second)) {
            return {second, true};
        }

        CurveLabelAnchor intersection = bestRightmostIntersection(first, second);
        if (intersection.valid) {
            return intersection;
        }

        if (visibleRect.contains(first)) {
            return {first, true};
        }
    }

    if (valid[0] && visibleRect.contains(points[0])) {
        return {points[0], true};
    }

    return {};
}

void KFilterView::drawCurve(QPainter& painter, const double values[200], int type)
{
    QPoint lastPoint(XK(Xvalue[0]), YScale(values[0], type));
    for (int i = 1; i < 150; i++) {
        const QPoint nextPoint(XK(Xvalue[i]), YScale(values[i], type));
        painter.drawLine(lastPoint, nextPoint);
        lastPoint = nextPoint;
    }
}

void KFilterView::drawDriverPressureCurve(QPainter& painter, const double values[200], int driverIndex)
{
    QPoint lastPoint(XK(Xvalue[0]), YScale(effectivePressureDb(driverIndex, 0, values[0]), 0));
    for (int i = 1; i < 150; i++) {
        const QPoint nextPoint(XK(Xvalue[i]),
                               YScale(effectivePressureDb(driverIndex, i, values[i]), 0));
        painter.drawLine(lastPoint, nextPoint);
        lastPoint = nextPoint;
    }
}

void KFilterView::drawCurveLabel(QPainter& painter, const QPointF& point, const QString& label) const
{
    const QString trimmedLabel = label.trimmed();
    if (trimmedLabel.isEmpty()) {
        return;
    }

    QFont labelFont(QStringLiteral("Sans Serif"), 9);
    labelFont.setBold(true);
    painter.setFont(labelFont);

    const QFontMetrics metrics(labelFont);
    const int textWidth = metrics.horizontalAdvance(trimmedLabel);
    const int margin = 4;
    const int spacing = 6;

    int x = static_cast<int>(std::round(point.x())) + spacing;
    if (x + textWidth + margin > width()) {
        x = static_cast<int>(std::round(point.x())) - textWidth - spacing;
    }
    x = std::max(margin, std::min(x, width() - textWidth - margin));

    int baseline = static_cast<int>(std::round(point.y())) + metrics.ascent() / 2;
    baseline = std::max(margin + metrics.ascent(), std::min(baseline, height() - margin - metrics.descent()));

    painter.setPen(QPen(shadowTextColor()));
    painter.drawText(x + 1, baseline + 1, trimmedLabel);
    painter.setPen(QPen(foregroundTextColor()));
    painter.drawText(x, baseline, trimmedLabel);
}

void KFilterView::drawDriverCurveLabels(QPainter& painter)
{
    KFilterDoc* mydoc = getDocument();
    if (mydoc == nullptr) {
        return;
    }

    for (int count = 0; count < 4; count++) {
        if (!mydoc->Sound(count)) {
            continue;
        }

        CurveLabelAnchor anchor = findLastVisibleCurvePoint(mydoc->m_doubleXContainer[count], 0, count);
        if (!anchor.valid) {
            continue;
        }

        QString label = mydoc->m_driverDriver[count].GetTitle();
        if (label.trimmed().isEmpty()) {
            label = tr("Driver %1").arg(count + 1);
        }
        drawCurveLabel(painter, anchor.point, label);
    }
}

QColor KFilterView::pressureCurveColor(int driverIndex) const
{
    if (driverIndex >= 0 && driverIndex < static_cast<int>(m_pressureCurveColors.size())) {
        return m_pressureCurveColors[driverIndex];
    }
    return cpressure;
}

QColor KFilterView::impedanceCurveColor(int driverIndex) const
{
    if (driverIndex >= 0 && driverIndex < static_cast<int>(m_impedanceCurveColors.size())) {
        return m_impedanceCurveColors[driverIndex];
    }
    return cimpedance;
}

QColor KFilterView::foregroundTextColor() const
{
    return m_backgroundColor.lightness() < 128 ? QColor(Qt::white) : QColor(Qt::black);
}

QColor KFilterView::shadowTextColor() const
{
    return m_backgroundColor.lightness() < 128 ? QColor(Qt::black) : QColor(Qt::white);
}

void KFilterView::drawMeasurementCurves(QPainter& painter)
{
    if (mergeMeasurementsEnabled() && !m_measurementDrawingActive) {
        return;
    }

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool showOnlyActiveCurve = mergeMeasurementsEnabled() && m_measurementDrawingActive;
    for (int driverIndex = 0; driverIndex < 4; ++driverIndex) {
        if (showOnlyActiveCurve && driverIndex != m_activeMeasurementDriverIndex) {
            continue;
        }

        const KFilterMeasurementCurve& curve = m_document->splCorrectionCurve(driverIndex);
        if (curve.isEmpty()) {
            continue;
        }

        QPen curvePen(pressureCurveColor(driverIndex));
        curvePen.setStyle(Qt::DashLine);
        curvePen.setWidth(2);
        painter.setPen(curvePen);
        painter.setBrush(Qt::NoBrush);

        QPointF previousPoint;
        bool havePreviousPoint = false;
        for (const KFilterMeasurementPoint& point : curve.points) {
            const QPointF plotPoint(frequencyHzToX(point.frequencyHz), pressureDbToY(point.value));
            if (!std::isfinite(plotPoint.x()) || !std::isfinite(plotPoint.y())) {
                continue;
            }

            if (havePreviousPoint) {
                painter.drawLine(previousPoint, plotPoint);
            }
            painter.drawEllipse(plotPoint, 3.5, 3.5);
            previousPoint = plotPoint;
            havePreviousPoint = true;
        }
    }

    if (m_measurementDrawingActive && m_measurementCursorValid &&
        m_activeMeasurementDriverIndex >= 0 && m_activeMeasurementDriverIndex < 4) {
        const KFilterMeasurementCurve& activeCurve = m_document->splCorrectionCurve(m_activeMeasurementDriverIndex);
        const double cursorFrequencyHz = xToFrequencyHz(m_measurementCursorPosition.x());

        QPen previewPen(pressureCurveColor(m_activeMeasurementDriverIndex));
        previewPen.setStyle(Qt::DashLine);
        previewPen.setWidth(1);
        painter.setPen(previewPen);
        painter.setBrush(Qt::NoBrush);

        if (!activeCurve.isEmpty() && std::isfinite(cursorFrequencyHz) &&
            cursorFrequencyHz > activeCurve.points.constLast().frequencyHz) {
            const KFilterMeasurementPoint& last = activeCurve.points.constLast();
            painter.drawLine(QPointF(frequencyHzToX(last.frequencyHz), pressureDbToY(last.value)),
                             m_measurementCursorPosition);
        }
        painter.drawEllipse(m_measurementCursorPosition, 4.0, 4.0);
    }

    painter.restore();
}

void KFilterView::drawLegend(QPainter& painter)
{
    KFilterDoc* mydoc = getDocument();
    if (mydoc == nullptr) {
        return;
    }

    QFont font(QStringLiteral("Sans Serif"), 8);
    painter.setFont(font);

    const int left = 10;
    int y = 18;
    const int lineWidth = 34;
    const int rowHeight = 16;

    auto drawEntry = [&](const QColor& color, Qt::PenStyle style, int width, const QString& label) {
        QPen pen(color);
        pen.setStyle(style);
        pen.setWidth(width);
        painter.setPen(pen);
        painter.drawLine(left, y - 4, left + lineWidth, y - 4);
        painter.setPen(QPen(foregroundTextColor()));
        painter.drawText(left + lineWidth + 8, y, label);
        y += rowHeight;
    };

    for (int driverIndex = 0; driverIndex < 4; ++driverIndex) {
        if (mydoc->m_driverDriver[driverIndex].PressureisActive) {
            const QString pressureLabel = measurementMergeAppliedForDriver(driverIndex)
                                              ? tr("Driver %1 SPL (merged)").arg(driverIndex + 1)
                                              : tr("Driver %1 SPL").arg(driverIndex + 1);
            drawEntry(pressureCurveColor(driverIndex),
                      Qt::SolidLine,
                      1,
                      pressureLabel);
        }

        if (mydoc->m_driverDriver[driverIndex].ImpedanzisActive) {
            drawEntry(impedanceCurveColor(driverIndex),
                      Qt::DotLine,
                      1,
                      tr("Driver %1 impedance").arg(driverIndex + 1));
        }
    }

    for (int driverIndex = 0; driverIndex < 4; ++driverIndex) {
        const bool curveVisible = !mergeMeasurementsEnabled() ||
                                  (m_measurementDrawingActive &&
                                   driverIndex == m_activeMeasurementDriverIndex);
        if (curveVisible && !m_document->splCorrectionCurve(driverIndex).isEmpty()) {
            drawEntry(pressureCurveColor(driverIndex),
                      Qt::DashLine,
                      2,
                      tr("Driver %1 SPL correction").arg(driverIndex + 1));
        }
    }

    if (mydoc->m_driverDriver[0].SummaryisActive ||
        mydoc->m_driverDriver[1].SummaryisActive ||
        mydoc->m_driverDriver[2].SummaryisActive ||
        mydoc->m_driverDriver[3].SummaryisActive) {
        drawEntry(cpressureS, Qt::SolidLine, 3, tr("Vector SPL sum"));
    }

    if (mydoc->m_driverDriver[0].ScalarSummaryisActive ||
        mydoc->m_driverDriver[1].ScalarSummaryisActive ||
        mydoc->m_driverDriver[2].ScalarSummaryisActive ||
        mydoc->m_driverDriver[3].ScalarSummaryisActive) {
        drawEntry(cscalarpressureS, Qt::SolidLine, 2, tr("Energetic SPL sum"));
    }

    if (mydoc->m_driverDriver[0].ImpedanzSummaryisActive ||
        mydoc->m_driverDriver[1].ImpedanzSummaryisActive ||
        mydoc->m_driverDriver[2].ImpedanzSummaryisActive ||
        mydoc->m_driverDriver[3].ImpedanzSummaryisActive) {
        drawEntry(cimpedanceS, Qt::DotLine, 2, tr("Total impedance"));
    }
}

void KFilterView::drawPrintMeasurementStatus(QPainter& painter)
{
    if (!m_printRendering || m_document == nullptr || !m_document->hasMeasurementCurves()) {
        return;
    }

    const bool mergeEnabled = m_document->measurementMergeEnabled();
    QStringList driverNumbers;
    for (int driverIndex = 0; driverIndex < 4; ++driverIndex) {
        const KFilterMeasurementCurve& curve = m_document->splCorrectionCurve(driverIndex);
        if (curve.isEmpty() || (mergeEnabled && curve.size() < 2)) {
            continue;
        }
        driverNumbers.append(QString::number(driverIndex + 1));
    }

    QString statusText;
    if (mergeEnabled) {
        statusText = tr("Measurements: merge enabled for driver(s) %1")
                         .arg(driverNumbers.join(QStringLiteral(", ")));
    } else {
        statusText = tr("Measurements: merge disabled; corrections shown for driver(s) %1")
                         .arg(driverNumbers.join(QStringLiteral(", ")));
    }

    painter.save();
    const QFont statusFont(QStringLiteral("Sans Serif"), 8);
    painter.setFont(statusFont);
    const QFontMetrics metrics(statusFont);
    const int horizontalMargin = 8;
    const int verticalMargin = 5;
    const int maximumTextWidth = std::max(80, width() - 36);
    statusText = metrics.elidedText(statusText, Qt::ElideRight, maximumTextWidth);

    QRect statusRect = metrics.boundingRect(statusText);
    statusRect.adjust(-horizontalMargin, -verticalMargin,
                      horizontalMargin, verticalMargin);
    statusRect.moveTop(8);
    statusRect.moveRight(width() - 8);

    painter.fillRect(statusRect, m_backgroundColor);
    QPen borderPen(cthresholdGrid);
    borderPen.setWidth(1);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(statusRect);

    painter.setPen(foregroundTextColor());
    painter.drawText(statusRect.adjusted(horizontalMargin, verticalMargin,
                                         -horizontalMargin, -verticalMargin),
                     Qt::AlignLeft | Qt::AlignVCenter, statusText);
    painter.restore();
}

void KFilterView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    KFilterDoc* mydoc = getDocument();
    QPainter mypainter(this);
    mypainter.fillRect(rect(), m_backgroundColor);

    QPen pen;
    pen.setColor(cgrid);
    pen.setWidth(1);
    mypainter.setPen(pen);

    int i;
    int j;
    for (i = 30; i <= 100; i = i + 10) {
        j = XK(i * 6.28318);
        mypainter.drawLine(j, 0, j, height());
    }
    for (i = 200; i <= 1000; i = i + 100) {
        j = XK(i * 6.28318);
        mypainter.drawLine(j, 0, j, height());
    }
    for (i = 2000; i <= 10000; i = i + 1000) {
        j = XK(i * 6.28318);
        mypainter.drawLine(j, 0, j, height());
    }

    for (i = 1; i <= 30; i++) {
        if ((i != 5) && (i != 10) && (i != 15) && (i != 20) && (i != 25) && (i != 30)) {
            mypainter.drawLine(0, i * height() / 30, width(), i * height() / 30);
        }
    }

    pen.setColor(cthresholdGrid);
    pen.setStyle(Qt::DotLine);
    pen.setWidth(1);
    mypainter.setPen(pen);
    for (i = 1; i <= 5; i++) {
        mypainter.drawLine(0, i * height() / 6, width(), i * height() / 6);
    }

    pen.setColor(foregroundTextColor());
    pen.setStyle(Qt::DotLine);
    pen.setWidth(1);
    mypainter.setPen(pen);
    mypainter.drawLine(0, height() / 6, width(), height() / 6);

    if (mydoc != nullptr) {
        for (int count = 0; count < 4; count++) {
            if (mydoc->Sound(count)) {
                pen.setColor(pressureCurveColor(count));
                pen.setStyle(Qt::SolidLine);
                pen.setWidth(1);
                mypainter.setPen(pen);
                drawDriverPressureCurve(mypainter, mydoc->m_doubleXContainer[count], count);
            }
            if (mydoc->Impedance(count)) {
                pen.setColor(impedanceCurveColor(count));
                pen.setStyle(Qt::DotLine);
                pen.setWidth(1);
                mypainter.setPen(pen);
                drawCurve(mypainter, mydoc->m_doubleXContainer[count], 1);
            }
        }

        if (mydoc->PressureSummary()) {
            pen.setColor(cpressureS);
            pen.setStyle(Qt::SolidLine);
            pen.setWidth(3);
            mypainter.setPen(pen);
            drawCurve(mypainter, mydoc->m_doubleXContainer[0], 0);
        }

        if (mydoc->PressureScalarSummary()) {
            pen.setColor(cscalarpressureS);
            pen.setStyle(Qt::SolidLine);
            pen.setWidth(2);
            mypainter.setPen(pen);
            drawCurve(mypainter, mydoc->m_doubleXContainer[0], 0);
        }

        if (mydoc->ImpedanceSummary()) {
            pen.setColor(cimpedanceS);
            pen.setStyle(Qt::DotLine);
            pen.setWidth(2);
            mypainter.setPen(pen);
            drawCurve(mypainter, mydoc->m_doubleXContainer[0], 1);
        }
    }

    drawMeasurementCurves(mypainter);

    pen.setColor(foregroundTextColor());
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);
    QFont font(QStringLiteral("Times"), 10);
    mypainter.setPen(pen);
    mypainter.setFont(font);

    mypainter.drawText(XK(100 * 6.28), 50 * height() / 63, QStringLiteral("100 Hz"));
    mypainter.drawText(XK(1000 * 6.28), 50 * height() / 63, QStringLiteral("1 kHz"));
    mypainter.drawText(XK(10000 * 6.28), 50 * height() / 63, QStringLiteral("10 kHz"));
    mypainter.drawText(5, 100 * height() / 625, QStringLiteral("0 dB"));
    mypainter.drawText(1, 200 * height() / 610, QStringLiteral("-10 dB"));
    mypainter.drawText(1, 300 * height() / 605, QStringLiteral("-20 dB"));
    mypainter.drawText(1, 400 * height() / 605, QStringLiteral("10 Ohm"));
    mypainter.drawText(1, 500 * height() / 605, QStringLiteral("0 Ohm"));

    drawDriverCurveLabels(mypainter);

    if (width() >= 760 && height() >= 420) {
        drawLegend(mypainter);
    }
    drawPrintMeasurementStatus(mypainter);
}


void KFilterView::mousePressEvent(QMouseEvent *event)
{
    if (!m_measurementDrawingActive || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPointF position = event->position();
    const double frequencyHz = xToFrequencyHz(position.x());
    const double pressureDb = yToPressureDb(position.y());
    KFilterMeasurementCurve& curve = m_document->splCorrectionCurve(m_activeMeasurementDriverIndex);

    if (!curve.appendPoint(frequencyHz, pressureDb)) {
        emit measurementStatusMessage(
            tr("Waypoint ignored: measurement frequencies must be finite, positive and strictly increase from left to right."));
        event->accept();
        return;
    }

    m_measurementCursorPosition = position;
    m_measurementCursorValid = true;
    update();
    emit measurementStatusMessage(
        tr("Waypoint %1 set for %2 at %3 Hz, %4 dB.")
            .arg(static_cast<qlonglong>(curve.size()))
            .arg(measurementDriverLabel(m_activeMeasurementDriverIndex))
            .arg(frequencyHz, 0, 'f', 1)
            .arg(pressureDb, 0, 'f', 1));
    event->accept();
}

void KFilterView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_measurementDrawingActive) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    m_measurementCursorPosition = event->position();
    m_measurementCursorValid = rect().contains(m_measurementCursorPosition.toPoint());
    update();

    const QString statusText = measurementPointerText(m_measurementCursorPosition);
    if (!statusText.isEmpty()) {
        emit measurementStatusMessage(statusText);
    }
    event->accept();
}

void KFilterView::leaveEvent(QEvent *event)
{
    if (m_measurementDrawingActive) {
        m_measurementCursorValid = false;
        update();
        emit measurementStatusMessage(
            tr("Drawing SPL correction for %1: move into the plot and left-click waypoints from left to right.")
                .arg(measurementDriverLabel(m_activeMeasurementDriverIndex)));
    }
    QWidget::leaveEvent(event);
}

void KFilterView::keyPressEvent(QKeyEvent *event)
{
    if (!m_measurementDrawingActive) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->matches(QKeySequence::Undo) || event->key() == Qt::Key_Backspace) {
        undoSplCorrectionWaypoint();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        finishSplCorrectionDrawing();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        cancelSplCorrectionDrawing();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void KFilterView::resizeEvent(QResizeEvent *event)
{
    if (m_measurementDrawingActive) {
        m_measurementCursorValid = false;
    }
    QWidget::resizeEvent(event);
}

KFilterView::PlotColorSettings KFilterView::defaultPlotColorSettings()
{
    PlotColorSettings settings;
    settings.background = QColor(Qt::black);
    settings.grid = QColor(70, 70, 70);
    settings.thresholdGrid = QColor(Qt::yellow);
    settings.pressureCurves = {QColor(Qt::red),
                               QColor(0, 210, 0),
                               QColor(255, 140, 0),
                               QColor(190, 120, 255)};
    settings.impedanceCurves = {QColor(205, 180, 0),
                                QColor(185, 165, 0),
                                QColor(220, 195, 40),
                                QColor(170, 150, 0)};
    settings.pressureSummary = QColor(255, 210, 0);
    settings.impedanceSummary = QColor(0, 220, 255);
    settings.scalarPressureSummary = QColor(Qt::magenta);
    return settings;
}

KFilterView::PlotColorSettings KFilterView::plotColorSettings() const
{
    PlotColorSettings settings;
    settings.background = m_backgroundColor;
    settings.grid = cgrid;
    settings.thresholdGrid = cthresholdGrid;
    settings.pressureCurves = m_pressureCurveColors;
    settings.impedanceCurves = m_impedanceCurveColors;
    settings.pressureSummary = cpressureS;
    settings.impedanceSummary = cimpedanceS;
    settings.scalarPressureSummary = cscalarpressureS;
    return settings;
}

void KFilterView::setPlotColorSettings(const PlotColorSettings& settings)
{
    m_backgroundColor = settings.background.isValid() ? settings.background : QColor(Qt::black);
    cgrid = settings.grid.isValid() ? settings.grid : QColor(70, 70, 70);
    cthresholdGrid = settings.thresholdGrid.isValid() ? settings.thresholdGrid : QColor(Qt::yellow);
    m_pressureCurveColors = settings.pressureCurves;
    m_impedanceCurveColors = settings.impedanceCurves;
    cpressure = m_pressureCurveColors[0].isValid() ? m_pressureCurveColors[0] : QColor(Qt::red);
    cimpedance = m_impedanceCurveColors[0].isValid() ? m_impedanceCurveColors[0] : QColor(205, 180, 0);
    cpressureS = settings.pressureSummary.isValid() ? settings.pressureSummary : QColor(255, 210, 0);
    cimpedanceS = settings.impedanceSummary.isValid() ? settings.impedanceSummary : QColor(0, 220, 255);
    cscalarpressureS = settings.scalarPressureSummary.isValid() ? settings.scalarPressureSummary : QColor(Qt::magenta);

    const PlotColorSettings defaults = defaultPlotColorSettings();
    for (int index = 0; index < static_cast<int>(m_pressureCurveColors.size()); ++index) {
        if (!m_pressureCurveColors[index].isValid()) {
            m_pressureCurveColors[index] = defaults.pressureCurves[index];
        }
        if (!m_impedanceCurveColors[index].isValid()) {
            m_impedanceCurveColors[index] = defaults.impedanceCurves[index];
        }
    }

    cpressure = m_pressureCurveColors[0];
    cimpedance = m_impedanceCurveColors[0];

    QPalette pal = palette();
    pal.setColor(QPalette::Window, m_backgroundColor);
    setPalette(pal);
    update();
}

void KFilterView::resetPlotColorSettings()
{
    setPlotColorSettings(defaultPlotColorSettings());
}

QColor KFilterView::backgroundColor() const
{
    return m_backgroundColor;
}

void KFilterView::setBackgroundColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }

    PlotColorSettings settings = plotColorSettings();
    settings.background = color;
    setPlotColorSettings(settings);
}

/** gives back the gridcolor */
QColor& KFilterView::gridColor()
{
    return cgrid;
}

/** sets the gridcolor */
void KFilterView::setGridColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cgrid = color;
    update();
}

/** gives back the pressurecolor */
QColor& KFilterView::pressureColor()
{
    return cpressure;
}

/** sets the pressurecolor */
void KFilterView::setPressureColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cpressure = color;
    m_pressureCurveColors[0] = color;
    update();
}

/** gives back the impedancecolor */
QColor& KFilterView::impedanceColor()
{
    return cimpedance;
}

/** sets the impedancecolor */
void KFilterView::setImpedanceColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cimpedance = color;
    m_impedanceCurveColors[0] = color;
    update();
}

/** gives back the pressuresummarycolor */
QColor& KFilterView::pressureSummaryColor()
{
    return cpressureS;
}

/** sets the pressuresummarycolor */
void KFilterView::setPressureSummaryColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cpressureS = color;
    update();
}

/** gives back the impedancesummarycolor */
QColor& KFilterView::impedanceSummaryColor()
{
    return cimpedanceS;
}

/** sets the impedancesummarycolor */
void KFilterView::setImpedanceSummaryColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cimpedanceS = color;
    update();
}

/** gives back the scalarpressuresummarycolor */
QColor& KFilterView::scalarPressureSummaryColor()
{
    return cscalarpressureS;
}

/** sets the scalarpressuresummarycolor */
void KFilterView::setScalarPressureSummaryColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }
    cscalarpressureS = color;
    update();
}
