/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "bafflegeometrypreview.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
constexpr qreal Margin = 4.0;
constexpr qreal TopDimensionReserve = 18.0;
constexpr qreal LeftDimensionReserve = 34.0;
constexpr qreal TrailingMargin = 4.0;
constexpr qreal ArrowSize = 6.0;
constexpr double DragResolutionMm = 0.1;

bool usablePositive(double value)
{
    return std::isfinite(value) && value > 0.0;
}

double normalizedPosition(double value, double total)
{
    if (!std::isfinite(value) || value <= 0.0 || !usablePositive(total)) {
        return 0.5;
    }
    return std::clamp(value / total, 0.0, 1.0);
}

double quantizeDragCoordinate(double value)
{
    return std::round(value / DragResolutionMm) * DragResolutionMm;
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
    setMouseTracking(true);
    setToolTip(tr("Schematic view of the baffle geometry. Highlighting follows the focused geometry field. In Rectangular Edge Diffraction mode, drag the symbolic driver circle to change X/Y; the acoustic response is recalculated when the mouse button is released. The driver circle is not scaled from Dm."));
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
                                               double driverYmm,
                                               double leftChamferSetbackMm,
                                               double rightChamferSetbackMm)
{
    if (m_widthMm == widthMm && m_heightMm == heightMm &&
        m_driverXmm == driverXmm && m_driverYmm == driverYmm &&
        m_leftChamferSetbackMm == leftChamferSetbackMm &&
        m_rightChamferSetbackMm == rightChamferSetbackMm) {
        return;
    }

    m_widthMm = widthMm;
    m_heightMm = heightMm;
    m_driverXmm = driverXmm;
    m_driverYmm = driverYmm;
    m_leftChamferSetbackMm = leftChamferSetbackMm;
    m_rightChamferSetbackMm = rightChamferSetbackMm;
    update();
}

void BaffleGeometryPreview::setDriverPositionCallbacks(DriverPositionCallback previewCallback,
                                                        DriverPositionCallback commitCallback)
{
    m_driverPositionPreviewCallback = std::move(previewCallback);
    m_driverPositionCommitCallback = std::move(commitCallback);
}

void BaffleGeometryPreview::setDriverDragEnabled(bool enabled)
{
    if (m_driverDragEnabled == enabled) {
        return;
    }

    m_driverDragEnabled = enabled;
    if (!enabled) {
        if (m_driverDragging) {
            m_driverDragging = false;
            m_driverDragChanged = false;
            m_driverXmm = m_driverDragStartXmm;
            m_driverYmm = m_driverDragStartYmm;
            if (m_driverPositionPreviewCallback) {
                m_driverPositionPreviewCallback(m_driverXmm, m_driverYmm);
            }
            update();
        }
        unsetCursor();
    }
}

bool BaffleGeometryPreview::driverDragEnabled() const noexcept
{
    return m_driverDragEnabled;
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

double BaffleGeometryPreview::leftChamferSetbackMm() const noexcept
{
    return m_leftChamferSetbackMm;
}

double BaffleGeometryPreview::rightChamferSetbackMm() const noexcept
{
    return m_rightChamferSetbackMm;
}

QSize BaffleGeometryPreview::sizeHint() const
{
    return QSize(300, 260);
}

QSize BaffleGeometryPreview::minimumSizeHint() const
{
    return QSize(240, 220);
}

QRectF BaffleGeometryPreview::currentBaffleRect() const
{
    QRectF drawingRect = QRectF(rect()).adjusted(Margin, Margin, -Margin, -Margin);
    if (drawingRect.width() <= LeftDimensionReserve + TrailingMargin ||
        drawingRect.height() <= TopDimensionReserve + TrailingMargin) {
        return {};
    }

    const QRectF baffleArea = drawingRect.adjusted(LeftDimensionReserve,
                                                    TopDimensionReserve,
                                                    -TrailingMargin,
                                                    -TrailingMargin);
    return fittedBaffleRect(baffleArea, m_widthMm, m_heightMm);
}

QPointF BaffleGeometryPreview::currentDriverCentre(const QRectF& baffle) const
{
    const double nx = normalizedPosition(m_driverXmm, m_widthMm);
    const double ny = normalizedPosition(m_driverYmm, m_heightMm);
    return QPointF(baffle.left() + nx * baffle.width(),
                   baffle.top() + ny * baffle.height());
}

qreal BaffleGeometryPreview::currentDriverRadius(const QRectF& baffle) const
{
    return std::clamp(std::min(baffle.width(), baffle.height()) * 0.10,
                      11.0,
                      24.0);
}

bool BaffleGeometryPreview::driverHitTest(const QPointF& position) const
{
    if (!m_driverDragEnabled || !usablePositive(m_widthMm) || !usablePositive(m_heightMm)) {
        return false;
    }

    const QRectF baffle = currentBaffleRect();
    if (baffle.isEmpty()) {
        return false;
    }

    const QPointF centre = currentDriverCentre(baffle);
    const qreal hitRadius = currentDriverRadius(baffle) + 4.0;
    const QPointF delta = position - centre;
    return delta.x() * delta.x() + delta.y() * delta.y() <= hitRadius * hitRadius;
}

bool BaffleGeometryPreview::updateDraggedDriverPosition(const QPointF& pointerPosition)
{
    if (!m_driverDragging || !usablePositive(m_widthMm) || !usablePositive(m_heightMm)) {
        return false;
    }

    const QRectF baffle = currentBaffleRect();
    if (baffle.isEmpty() || baffle.width() <= 0.0 || baffle.height() <= 0.0) {
        return false;
    }

    const double minX = std::max(0.0, m_leftChamferSetbackMm) + DragResolutionMm;
    const double maxX = m_widthMm - std::max(0.0, m_rightChamferSetbackMm) - DragResolutionMm;
    const double minY = DragResolutionMm;
    const double maxY = m_heightMm - DragResolutionMm;
    if (!(minX <= maxX) || !(minY <= maxY)) {
        return false;
    }

    const QPointF desiredCentre = pointerPosition - m_driverDragOffset;
    double xMm = m_widthMm * (desiredCentre.x() - baffle.left()) / baffle.width();
    double yMm = m_heightMm * (desiredCentre.y() - baffle.top()) / baffle.height();
    xMm = std::clamp(quantizeDragCoordinate(xMm), minX, maxX);
    yMm = std::clamp(quantizeDragCoordinate(yMm), minY, maxY);

    if (std::abs(xMm - m_driverXmm) < 1.0e-9 &&
        std::abs(yMm - m_driverYmm) < 1.0e-9) {
        return false;
    }

    m_driverXmm = xMm;
    m_driverYmm = yMm;
    m_driverDragChanged =
        std::abs(m_driverXmm - m_driverDragStartXmm) >= 1.0e-9 ||
        std::abs(m_driverYmm - m_driverDragStartYmm) >= 1.0e-9;
    if (m_driverPositionPreviewCallback) {
        m_driverPositionPreviewCallback(m_driverXmm, m_driverYmm);
    }
    update();
    return true;
}

void BaffleGeometryPreview::updateHoverCursor(const QPointF& pointerPosition)
{
    if (m_driverDragging) {
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (driverHitTest(pointerPosition)) {
        setCursor(Qt::OpenHandCursor);
    } else {
        unsetCursor();
    }
}

void BaffleGeometryPreview::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && driverHitTest(event->position())) {
        const QRectF baffle = currentBaffleRect();
        m_driverDragging = true;
        m_driverDragChanged = false;
        m_driverDragStartXmm = m_driverXmm;
        m_driverDragStartYmm = m_driverYmm;
        m_driverDragOffset = event->position() - currentDriverCentre(baffle);
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void BaffleGeometryPreview::mouseMoveEvent(QMouseEvent *event)
{
    if (m_driverDragging && (event->buttons() & Qt::LeftButton)) {
        updateDraggedDriverPosition(event->position());
        event->accept();
        return;
    }

    updateHoverCursor(event->position());
    QWidget::mouseMoveEvent(event);
}

void BaffleGeometryPreview::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_driverDragging) {
        updateDraggedDriverPosition(event->position());
        m_driverDragging = false;
        const bool changed = m_driverDragChanged;
        m_driverDragChanged = false;

        if (changed && m_driverPositionCommitCallback) {
            m_driverPositionCommitCallback(m_driverXmm, m_driverYmm);
        }

        updateHoverCursor(event->position());
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void BaffleGeometryPreview::leaveEvent(QEvent *event)
{
    if (!m_driverDragging) {
        unsetCursor();
    }
    QWidget::leaveEvent(event);
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
    const QRectF baffle = currentBaffleRect();
    if (baffle.isEmpty()) {
        return;
    }

    painter.setPen(QPen(normal, 1.4));
    painter.setBrush(background);
    painter.drawRect(baffle);

    const auto chamferPixels = [&](double setbackMm) -> qreal {
        if (!usablePositive(m_widthMm) || !usablePositive(setbackMm)) {
            return 0.0;
        }
        return std::clamp<qreal>(baffle.width() * setbackMm / m_widthMm,
                                 0.0, baffle.width() * 0.45);
    };
    const qreal leftChamferPx = chamferPixels(m_leftChamferSetbackMm);
    const qreal rightChamferPx = chamferPixels(m_rightChamferSetbackMm);

    // Front view of the two full-height 45-degree side bevels. The outer
    // rectangle remains the cabinet silhouette; the inset vertical lines mark
    // where the flat front surface begins. A few diagonals make the sloping
    // surfaces visually unambiguous without turning the preview into CAD.
    painter.save();
    painter.setPen(QPen(muted, 1.0));
    if (leftChamferPx > 0.0) {
        const qreal innerX = baffle.left() + leftChamferPx;
        painter.drawLine(QPointF(innerX, baffle.top()), QPointF(innerX, baffle.bottom()));
        for (int i = 1; i <= 3; ++i) {
            const qreal y = baffle.top() + i * baffle.height() / 4.0;
            painter.drawLine(QPointF(baffle.left(), y + 5.0),
                             QPointF(innerX, y - 5.0));
        }
    }
    if (rightChamferPx > 0.0) {
        const qreal innerX = baffle.right() - rightChamferPx;
        painter.drawLine(QPointF(innerX, baffle.top()), QPointF(innerX, baffle.bottom()));
        for (int i = 1; i <= 3; ++i) {
            const qreal y = baffle.top() + i * baffle.height() / 4.0;
            painter.drawLine(QPointF(innerX, y - 5.0),
                             QPointF(baffle.right(), y + 5.0));
        }
    }
    painter.restore();

    const QPointF centre = currentDriverCentre(baffle);
    const qreal driverRadius = currentDriverRadius(baffle);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(m_driverDragging ? accent : normal, m_driverDragging ? 2.0 : 1.5));
    painter.drawEllipse(centre, driverRadius, driverRadius);

    painter.setPen(QPen(m_driverDragging ? accent : normal, 1.2));
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
    case Highlight::LeftChamfer: {
        if (leftChamferPx > 0.0) {
            const qreal y = baffle.bottom() - 12.0;
            drawDimensionLine(painter,
                              QPointF(baffle.left(), y),
                              QPointF(baffle.left() + leftChamferPx, y));
            painter.drawText(QRectF(baffle.left(), y - 24.0,
                                    std::max<qreal>(55.0, leftChamferPx), 18.0),
                             Qt::AlignLeft | Qt::AlignVCenter, tr("Left chamfer"));
        }
        break;
    }
    case Highlight::RightChamfer: {
        if (rightChamferPx > 0.0) {
            const qreal y = baffle.bottom() - 12.0;
            drawDimensionLine(painter,
                              QPointF(baffle.right() - rightChamferPx, y),
                              QPointF(baffle.right(), y));
            painter.drawText(QRectF(baffle.right() - std::max<qreal>(90.0, rightChamferPx),
                                    y - 24.0, std::max<qreal>(90.0, rightChamferPx), 18.0),
                             Qt::AlignRight | Qt::AlignVCenter, tr("Right chamfer"));
        }
        break;
    }
    case Highlight::None:
        break;
    }
}
