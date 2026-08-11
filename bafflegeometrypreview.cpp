/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "bafflegeometrypreview.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace
{
constexpr qreal Margin = 4.0;
constexpr qreal TopDimensionReserve = 18.0;
constexpr qreal LeftDimensionReserve = 34.0;
constexpr qreal TrailingMargin = 4.0;
constexpr qreal ArrowSize = 6.0;

bool usablePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

double normalizedPosition(double value, double total)
{
    if (!std::isfinite(value) || value <= 0.0 || !usablePositive(total)) {
        return 0.5;
    }
    return std::clamp(value / total, 0.04, 0.96);
}

void drawArrowHead(QPainter& painter,
                   const QPointF& tip,
                   const QPointF& towards,
                   qreal size)
{
    QLineF direction(tip, towards);
    if (direction.length() <= 0.0) {
        return;
    }
    direction.setLength(size);

    QLineF left(direction);
    left.setAngle(direction.angle() + 28.0);
    QLineF right(direction);
    right.setAngle(direction.angle() - 28.0);

    painter.drawLine(tip, left.p2());
    painter.drawLine(tip, right.p2());
}

void drawDimensionLine(QPainter& painter, const QPointF& from, const QPointF& to)
{
    painter.drawLine(from, to);
    drawArrowHead(painter, from, to, ArrowSize);
    drawArrowHead(painter, to, from, ArrowSize);
}

QRectF fittedBaffleRect(const QRectF& available, double widthMm, double heightMm)
{
    double aspect = 0.58;
    if (usablePositive(widthMm) && usablePositive(heightMm)) {
        aspect = widthMm / heightMm;
    }

    // Keep pathological/incomplete values readable without pretending that the
    // preview is a CAD or validation surface.
    aspect = std::clamp(aspect, 0.18, 4.0);

    qreal drawWidth = available.width();
    qreal drawHeight = drawWidth / aspect;
    if (drawHeight > available.height()) {
        drawHeight = available.height();
        drawWidth = drawHeight * aspect;
    }

    return QRectF(available.center().x() - drawWidth / 2.0,
                  available.center().y() - drawHeight / 2.0,
                  drawWidth,
                  drawHeight);
}
}

BaffleGeometryPreview::BaffleGeometryPreview(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(minimumSizeHint());
    setToolTip(tr("Schematic view of the baffle geometry. Highlighting follows the focused geometry field; the driver circle is symbolic and not scaled from Dm."));
}

void BaffleGeometryPreview::setHighlight(Highlight highlight)
{
    if (m_highlight == highlight) {
        return;
    }
    m_highlight = highlight;
    update();
}

BaffleGeometryPreview::Highlight BaffleGeometryPreview::currentHighlight() const noexcept
{
    return m_highlight;
}

void BaffleGeometryPreview::setGeometryValues(double widthMm,
                                               double heightMm,
                                               double driverXmm,
                                               double driverYmm)
{
    if (m_widthMm == widthMm && m_heightMm == heightMm &&
        m_driverXmm == driverXmm && m_driverYmm == driverYmm) {
        return;
    }

    m_widthMm = widthMm;
    m_heightMm = heightMm;
    m_driverXmm = driverXmm;
    m_driverYmm = driverYmm;
    update();
}

double BaffleGeometryPreview::baffleWidthMm() const noexcept
{
    return m_widthMm;
}

double BaffleGeometryPreview::baffleHeightMm() const noexcept
{
    return m_heightMm;
}

double BaffleGeometryPreview::driverXmm() const noexcept
{
    return m_driverXmm;
}

double BaffleGeometryPreview::driverYmm() const noexcept
{
    return m_driverYmm;
}

QSize BaffleGeometryPreview::sizeHint() const
{
    return QSize(300, 260);
}

QSize BaffleGeometryPreview::minimumSizeHint() const
{
    return QSize(240, 220);
}

void BaffleGeometryPreview::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QColor normal = palette().color(QPalette::WindowText);
    const QColor muted = palette().color(QPalette::Mid);
    const QColor accent = palette().color(QPalette::Highlight);
    const QColor background = palette().color(QPalette::Base);

    QRectF drawingRect = QRectF(rect()).adjusted(Margin, Margin, -Margin, -Margin);
    if (drawingRect.width() <= LeftDimensionReserve + TrailingMargin ||
        drawingRect.height() <= TopDimensionReserve + TrailingMargin) {
        return;
    }

    // External dimensions are drawn only above (width) and to the left
    // (height). Reclaim the previously symmetric bottom/right reserves so the
    // explanatory geometry can use substantially more of the preview area.
    const QRectF baffleArea = drawingRect.adjusted(LeftDimensionReserve,
                                                    TopDimensionReserve,
                                                    -TrailingMargin,
                                                    -TrailingMargin);
    const QRectF baffle = fittedBaffleRect(baffleArea, m_widthMm, m_heightMm);

    painter.setPen(QPen(normal, 1.4));
    painter.setBrush(background);
    painter.drawRect(baffle);

    const double nx = normalizedPosition(m_driverXmm, m_widthMm);
    const double ny = normalizedPosition(m_driverYmm, m_heightMm);
    const QPointF centre(baffle.left() + nx * baffle.width(),
                         baffle.top() + ny * baffle.height());

    const qreal driverRadius = std::clamp(std::min(baffle.width(), baffle.height()) * 0.10,
                                          11.0,
                                          24.0);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(normal, 1.5));
    painter.drawEllipse(centre, driverRadius, driverRadius);

    painter.setPen(QPen(normal, 1.2));
    painter.drawLine(QPointF(centre.x() - 4.0, centre.y()),
                     QPointF(centre.x() + 4.0, centre.y()));
    painter.drawLine(QPointF(centre.x(), centre.y() - 4.0),
                     QPointF(centre.x(), centre.y() + 4.0));

    painter.setPen(muted);
    painter.drawText(QRectF(centre.x() + driverRadius + 5.0,
                            centre.y() - 10.0,
                            std::max<qreal>(0.0, drawingRect.right() - centre.x() - driverRadius - 5.0),
                            20.0),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("centre"));

    painter.setPen(QPen(accent, 2.0));
    painter.setBrush(Qt::NoBrush);

    switch (m_highlight) {
    case Highlight::BaffleWidth: {
        const qreal y = baffle.top() - 5.0;
        drawDimensionLine(painter, QPointF(baffle.left(), y), QPointF(baffle.right(), y));
        painter.drawLine(QPointF(baffle.left(), y + 4.0), QPointF(baffle.left(), baffle.top()));
        painter.drawLine(QPointF(baffle.right(), y + 4.0), QPointF(baffle.right(), baffle.top()));

        const qreal labelBottom = y - 2.0;
        const qreal labelTop = std::max<qreal>(0.0, labelBottom - 18.0);
        painter.drawText(QRectF(baffle.left(),
                                labelTop,
                                baffle.width(),
                                std::max<qreal>(0.0, labelBottom - labelTop)),
                         Qt::AlignCenter,
                         tr("Baffle width"));
        break;
    }
    case Highlight::BaffleHeight: {
        const qreal x = baffle.left() - 15.0;
        drawDimensionLine(painter, QPointF(x, baffle.top()), QPointF(x, baffle.bottom()));
        painter.drawLine(QPointF(x + 4.0, baffle.top()), QPointF(baffle.left(), baffle.top()));
        painter.drawLine(QPointF(x + 4.0, baffle.bottom()), QPointF(baffle.left(), baffle.bottom()));
        painter.save();
        painter.translate(x - 15.0, baffle.center().y());
        painter.rotate(-90.0);
        painter.drawText(QRectF(-baffle.height() / 2.0, -10.0, baffle.height(), 20.0),
                         Qt::AlignCenter,
                         tr("Baffle height"));
        painter.restore();
        break;
    }
    case Highlight::DriverX: {
        drawDimensionLine(painter, QPointF(baffle.left(), centre.y()), centre);
        painter.drawText(QRectF(baffle.left(),
                                centre.y() - 23.0,
                                std::max<qreal>(40.0, centre.x() - baffle.left()),
                                18.0),
                         Qt::AlignCenter,
                         tr("X to centre"));
        break;
    }
    case Highlight::DriverY: {
        drawDimensionLine(painter, QPointF(centre.x(), baffle.top()), centre);
        painter.save();
        painter.translate(centre.x() + 16.0, (baffle.top() + centre.y()) / 2.0);
        painter.rotate(-90.0);
        painter.drawText(QRectF(-(centre.y() - baffle.top()) / 2.0,
                                -10.0,
                                std::max<qreal>(40.0, centre.y() - baffle.top()),
                                20.0),
                         Qt::AlignCenter,
                         tr("Y to centre"));
        painter.restore();
        break;
    }
    case Highlight::None:
        break;
    }
}
