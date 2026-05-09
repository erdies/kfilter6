/***************************************************************************
                          kfilterview.cpp  -  description
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

#include "kfilterview.h"

#include "kfilterdoc.h"

#include <QBrush>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPoint>
#include <Qt>

#include <cmath>

KFilterView::KFilterView(KFilterDoc *document, QWidget *parent)
    : QWidget(parent),
      m_document(document),
      cgrid(QColor(70, 70, 70)),
      cpressure(Qt::red),
      cimpedance(QColor(205, 180, 0)),
      cpressureS(QColor(255, 210, 0)),
      cimpedanceS(QColor(0, 220, 255)),
      cscalarpressureS(Qt::magenta)
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);

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

void KFilterView::drawCurve(QPainter& painter, const double values[200], int type)
{
    QPoint lastPoint(XK(Xvalue[0]), YScale(values[0], type));
    for (int i = 1; i < 150; i++) {
        const QPoint nextPoint(XK(Xvalue[i]), YScale(values[i], type));
        painter.drawLine(lastPoint, nextPoint);
        lastPoint = nextPoint;
    }
}


QColor KFilterView::pressureCurveColor(int driverIndex) const
{
    switch (driverIndex) {
    case 0: return cpressure;
    case 1: return QColor(0, 210, 0);
    case 2: return QColor(255, 140, 0);
    case 3: return QColor(190, 120, 255);
    default: return cpressure;
    }
}

QColor KFilterView::impedanceCurveColor(int driverIndex) const
{
    switch (driverIndex) {
    case 0: return cimpedance;
    case 1: return QColor(185, 165, 0);
    case 2: return QColor(220, 195, 40);
    case 3: return QColor(170, 150, 0);
    default: return cimpedance;
    }
}

void KFilterView::drawLegend(QPainter& painter)
{
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
        painter.setPen(QPen(Qt::lightGray));
        painter.drawText(left + lineWidth + 8, y, label);
        y += rowHeight;
    };

    drawEntry(pressureCurveColor(0), Qt::SolidLine, 1, tr("Driver 1 SPL"));
    drawEntry(pressureCurveColor(1), Qt::SolidLine, 1, tr("Driver 2 SPL"));
    drawEntry(pressureCurveColor(2), Qt::SolidLine, 1, tr("Driver 3 SPL"));
    drawEntry(pressureCurveColor(3), Qt::SolidLine, 1, tr("Driver 4 SPL"));
    drawEntry(cpressureS, Qt::SolidLine, 3, tr("Vector SPL sum"));
    drawEntry(cscalarpressureS, Qt::SolidLine, 2, tr("Energetic SPL sum"));
    drawEntry(cimpedanceS, Qt::DotLine, 2, tr("Total impedance"));
}

void KFilterView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    KFilterDoc* mydoc = getDocument();
    QPainter mypainter(this);
    mypainter.fillRect(rect(), palette().window());

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
        if ((i != 5) | (i != 10) | (i != 15) | (i != 20) | (i != 25) | (i != 30)) {
            mypainter.drawLine(0, i * height() / 30, width(), i * height() / 30);
        }
    }

    pen.setColor(Qt::yellow);
    pen.setStyle(Qt::DotLine);
    mypainter.setPen(pen);
    for (i = 1; i <= 5; i++) {
        mypainter.drawLine(0, i * height() / 6, width(), i * height() / 6);
    }

    pen.setColor(Qt::black);
    pen.setStyle(Qt::SolidLine);
    mypainter.setPen(pen);

    pen.setColor(Qt::white);
    pen.setStyle(Qt::DotLine);
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

    pen.setColor(Qt::lightGray);
    pen.setStyle(Qt::SolidLine);
    pen.setWidth(1);
    QFont font(QStringLiteral("Times"), 10);
    mypainter.setPen(pen);
    mypainter.setFont(font);

    mypainter.drawText(XK(80 * 6.28), 50 * height() / 63, QStringLiteral("100 Hz"));
    mypainter.drawText(XK(800 * 6.28), 50 * height() / 63, QStringLiteral("1 kHz"));
    mypainter.drawText(XK(8000 * 6.28), 50 * height() / 63, QStringLiteral("10 kHz"));
    mypainter.drawText(5, 100 * height() / 625, QStringLiteral("0 dB"));
    mypainter.drawText(1, 200 * height() / 610, QStringLiteral("-10 dB"));
    mypainter.drawText(1, 300 * height() / 605, QStringLiteral("-20 dB"));
    mypainter.drawText(1, 400 * height() / 605, QStringLiteral("10 Ohm"));
    mypainter.drawText(1, 500 * height() / 605, QStringLiteral("0 Ohm"));

    if (width() >= 760 && height() >= 420) {
        drawLegend(mypainter);
    }
}

/** gives back the gridcolor */
QColor& KFilterView::gridColor()
{
    return cgrid;
}

/** sets the gridcolor */
void KFilterView::setGridColor(const QColor& color)
{
    cgrid = color;
}

/** gives back the pressurecolor */
QColor& KFilterView::pressureColor()
{
    return cpressure;
}

/** sets the pressurecolor */
void KFilterView::setPressureColor(const QColor& color)
{
    cpressure = color;
}

/** gives back the impedancecolor */
QColor& KFilterView::impedanceColor()
{
    return cimpedance;
}

/** sets the impedancecolor */
void KFilterView::setImpedanceColor(const QColor& color)
{
    cimpedance = color;
}

/** gives back the pressuresummarycolor */
QColor& KFilterView::pressureSummaryColor()
{
    return cpressureS;
}

/** sets the pressuresummarycolor */
void KFilterView::setPressureSummaryColor(const QColor& color)
{
    cpressureS = color;
}

/** gives back the impedancesummarycolor */
QColor& KFilterView::impedanceSummaryColor()
{
    return cimpedanceS;
}

/** sets the impedancesummarycolor */
void KFilterView::setImpedanceSummaryColor(const QColor& color)
{
    cimpedanceS = color;
}

/** gives back the scalarpressuresummarycolor */
QColor& KFilterView::scalarPressureSummaryColor()
{
    return cscalarpressureS;
}

/** sets the scalarpressuresummarycolor */
void KFilterView::setScalarPressureSummaryColor(const QColor& color)
{
    cscalarpressureS = color;
}
