/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "kfilterview.h"

#include "kfilterdoc.h"

#include <QBrush>
#include <QFont>
#include <QFontMetrics>
#include <QLineF>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <QRectF>
#include <Qt>

#include <algorithm>
#include <cmath>

KFilterView::KFilterView(KFilterDoc *document, QWidget *parent)
    : QWidget(parent),
      m_document(document)
{
    setAutoFillBackground(true);
    setPlotColorSettings(defaultPlotColorSettings());

    setMinimumSize(640, 360);
    Start = 125.6637061;
    Faktor = 1.047128548;
    m_Schall1 = 0;
    m_Schall2 = 0;
    m_Schall3 = 0;
    initXvalue();
}

KFilterView::~KFilterView() = default;

KFilterDoc *KFilterView::getDocument() const
{
    return m_document;
}

void KFilterView::print(QPrinter *pPrinter)
{
    // Printing is intentionally left disabled during the first Qt6 view bring-up.
    Q_UNUSED(pPrinter);
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

KFilterView::CurveLabelAnchor KFilterView::findLastVisibleCurvePoint(const double values[200], int type) const
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
        points[i] = QPointF(XK(Xvalue[i]), YScale(values[i], type));
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

        CurveLabelAnchor anchor = findLastVisibleCurvePoint(mydoc->m_doubleXContainer[count], 0);
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
            drawEntry(pressureCurveColor(driverIndex),
                      Qt::SolidLine,
                      1,
                      tr("Driver %1 SPL").arg(driverIndex + 1));
        }

        if (mydoc->m_driverDriver[driverIndex].ImpedanzisActive) {
            drawEntry(impedanceCurveColor(driverIndex),
                      Qt::DotLine,
                      1,
                      tr("Driver %1 impedance").arg(driverIndex + 1));
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
                drawCurve(mypainter, mydoc->m_doubleXContainer[count], 0);
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
