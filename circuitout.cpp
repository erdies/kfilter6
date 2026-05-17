/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "circuitout.h"

#include "driver.h"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPen>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double Epsilon = 1.0e-18;

bool isActive(double value)
{
    return std::abs(value) > Epsilon;
}

QString compactNumber(double value, int precision = 4)
{
    return QString::number(value, 'g', precision);
}

int perceivedBrightness(const QColor& color)
{
    return (299 * color.red() + 587 * color.green() + 114 * color.blue()) / 1000;
}

bool driverHasActiveCurveOrTotalFlag(const driver& drv)
{
    return drv.PressureisActive ||
           drv.ImpedanzisActive ||
           drv.SummaryisActive ||
           drv.ScalarSummaryisActive ||
           drv.ImpedanzSummaryisActive;
}

int allDriversPreviewHeight(int driverCount)
{
    const int visibleRowCount = std::max(1, driverCount);
    return 16 + visibleRowCount * 320 + (visibleRowCount - 1) * 10;
}

struct EquivalentDriverGeometry
{
    qreal topY = 0.0;
    qreal bottomY = 0.0;
    QRectF rSeriesRect;
    QRectF lSeriesRect;
    qreal busLeftX = 0.0;
    qreal busRightX = 0.0;
    std::array<qreal, 3> branchX{};
};

EquivalentDriverGeometry buildEquivalentDriverGeometry(const QRectF& rect, int signalY, int returnY,
                                                       qreal shuntStartX, qreal shuntBranchShiftX = 0.0)
{
    EquivalentDriverGeometry geometry;
    geometry.topY = std::max(rect.top() + 8.0, static_cast<qreal>(signalY));
    geometry.bottomY = static_cast<qreal>(returnY);

    // Keep the series RL branch a bit further to the left so the shunt elements
    // can be spread out more clearly in narrow box-symbol layouts.
    const qreal rSeriesWidth = 20;
    const qreal lSeriesWidth = 26;
    const qreal gap = 4;

    geometry.rSeriesRect = QRectF(rect.left() + 1, geometry.topY - 7, rSeriesWidth, 14);
    geometry.lSeriesRect = QRectF(geometry.rSeriesRect.right() + gap, geometry.topY - 8, lSeriesWidth, 16);

    geometry.busLeftX = geometry.lSeriesRect.right() + 4;
    geometry.busRightX = rect.right() - 2;

    if (shuntStartX > 0.0) {
        const qreal clampedShuntStartX = std::clamp(shuntStartX, geometry.busLeftX, geometry.busRightX - 22.0);
        const qreal shuntWidth = std::max<qreal>(22.0, geometry.busRightX - clampedShuntStartX);
        geometry.branchX = {
            clampedShuntStartX + shuntWidth * 0.08,
            clampedShuntStartX + shuntWidth * 0.52,
            clampedShuntStartX + shuntWidth * 0.92
        };
    } else {
        const qreal usableWidth = std::max<qreal>(30.0, geometry.busRightX - geometry.busLeftX);
        geometry.branchX = {
            geometry.busLeftX + usableWidth * 0.03,
            geometry.busLeftX + usableWidth * 0.50,
            geometry.busLeftX + usableWidth * 0.97
        };
    }

    if (std::abs(shuntBranchShiftX) > 0.0) {
        // Shift only the parallel R/C/L branch group while keeping it inside
        // the existing upper and lower bus connections.
        const qreal maxLeftShift = geometry.busLeftX - geometry.branchX.front();
        const qreal maxRightShift = geometry.busRightX - geometry.branchX.back();
        const qreal effectiveShift = std::clamp(shuntBranchShiftX, maxLeftShift, maxRightShift);
        for (qreal& branchX : geometry.branchX) {
            branchX += effectiveShift;
        }
    }

    return geometry;
}
}

CircuitOut::CircuitOut(QWidget *parent)
    : QWidget(parent)
{
    setAutoFillBackground(true);
    setMouseTracking(true);
    setToolTip(tr("Click a driver activity lamp to toggle plot visibility for that driver."));
    setMinimumHeight(300);
}

CircuitOut::DriverSnapshot CircuitOut::snapshotFromDriver(driver& drv, int driverNumber)
{
    DriverSnapshot snapshot;
    snapshot.driverNumber = driverNumber;
    snapshot.title = drv.GetTitle();
    snapshot.boxTypeProposal = drv.GTypProposal;
    snapshot.vb = drv.Vb;
    snapshot.fb = drv.Fb;
    snapshot.v2 = drv.V2;
    if (drv.Vb == 0.0 || snapshot.boxTypeProposal < 0) {
        snapshot.boxTypeProposal = 0;
    }
    for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
        snapshot.network[unitIndex] = drv.getUnit(unitIndex);
    }
    snapshot.curveOrTotalFlagActive = driverHasActiveCurveOrTotalFlag(drv);
    snapshot.valid = true;
    return snapshot;
}

void CircuitOut::applySnapshot(const DriverSnapshot& snapshot)
{
    m_network = snapshot.network;
    m_driverTitle = snapshot.title;
    m_driverNumber = snapshot.driverNumber;
    m_boxTypeProposal = snapshot.boxTypeProposal;
    m_vb = snapshot.vb;
    m_fb = snapshot.fb;
    m_v2 = snapshot.v2;
    m_curveOrTotalFlagActive = snapshot.curveOrTotalFlagActive;
}

void CircuitOut::applyPreviewGeometry()
{
    setMinimumHeight(minimumSizeHint().height());
    updateGeometry();
    update();
}

void CircuitOut::setDriver(driver& drv, int driverNumber)
{
    m_showAllDrivers = false;
    m_driverSnapshotCount = 1;
    m_driverSnapshots[0] = snapshotFromDriver(drv, driverNumber);
    applySnapshot(m_driverSnapshots[0]);
    applyPreviewGeometry();
}

void CircuitOut::setDrivers(driver drivers[], int driverCount)
{
    m_showAllDrivers = true;
    m_driverSnapshotCount = 0;

    const int availableDriverCount = std::clamp(driverCount, 0, static_cast<int>(m_driverSnapshots.size()));
    for (int index = 0; index < availableDriverCount; ++index) {
        m_driverSnapshots[m_driverSnapshotCount] = snapshotFromDriver(drivers[index], index + 1);
        ++m_driverSnapshotCount;
    }
    for (int index = m_driverSnapshotCount; index < static_cast<int>(m_driverSnapshots.size()); ++index) {
        m_driverSnapshots[index] = DriverSnapshot{};
    }

    if (m_driverSnapshotCount > 0) {
        applySnapshot(m_driverSnapshots[0]);
    }
    applyPreviewGeometry();
}

void CircuitOut::setvalues(double network[])
{
    m_showAllDrivers = false;
    m_driverSnapshotCount = 0;
    for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
        m_network[unitIndex] = network[unitIndex];
    }
    applyPreviewGeometry();
}

void CircuitOut::setShowValues(bool showValues)
{
    if (m_showValues == showValues) {
        return;
    }
    m_showValues = showValues;
    update();
}

QColor CircuitOut::defaultBackgroundColor()
{
    return QColor(148, 148, 148);
}

QColor CircuitOut::backgroundColor() const
{
    return m_backgroundColor;
}

void CircuitOut::setBackgroundColor(const QColor& color)
{
    if (!color.isValid() || m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    update();
}

bool CircuitOut::printRenderStyle() const
{
    return m_renderStyle == RenderStyle::Print;
}

QColor CircuitOut::backgroundFillColor() const
{
    return printRenderStyle() ? QColor(Qt::white) : m_backgroundColor;
}

bool CircuitOut::useLightInk() const
{
    if (printRenderStyle()) {
        return false;
    }

    return perceivedBrightness(m_backgroundColor) < 128;
}

QColor CircuitOut::panelBorderColor() const
{
    if (printRenderStyle()) {
        return QColor(150, 150, 150);
    }

    return useLightInk() ? QColor(205, 205, 205)
                         : QColor(88, 88, 88);
}

QColor CircuitOut::primaryInkColor() const
{
    if (printRenderStyle()) {
        return QColor(0, 0, 0);
    }

    return useLightInk() ? QColor(245, 245, 245)
                         : QColor(0, 0, 0);
}

QColor CircuitOut::secondaryInkColor() const
{
    if (printRenderStyle()) {
        return QColor(50, 50, 50);
    }

    return useLightInk() ? QColor(220, 220, 220)
                         : QColor(45, 45, 45);
}

QColor CircuitOut::guideInkColor() const
{
    if (printRenderStyle()) {
        return QColor(115, 115, 115);
    }

    return useLightInk() ? QColor(185, 185, 185)
                         : QColor(105, 105, 105);
}

QColor CircuitOut::placeholderInkColor() const
{
    if (printRenderStyle()) {
        return QColor(165, 165, 165);
    }

    return useLightInk() ? QColor(150, 150, 150)
                         : QColor(175, 175, 175);
}

QColor CircuitOut::enclosureOutlineColor() const
{
    if (printRenderStyle()) {
        return QColor(65, 65, 65);
    }

    return useLightInk() ? QColor(245, 205, 160)
                         : QColor(120, 85, 50);
}

QSize CircuitOut::minimumSizeHint() const
{
    if (m_showAllDrivers) {
        return QSize(860, allDriversPreviewHeight(m_driverSnapshotCount));
    }
    return QSize(860, 330);
}

QSize CircuitOut::sizeHint() const
{
    if (m_showAllDrivers) {
        return QSize(1140, allDriversPreviewHeight(m_driverSnapshotCount));
    }
    return QSize(1140, 330);
}

QSize CircuitOut::sourceSizeForRender(RenderStyle style) const
{
    if (m_showAllDrivers && style == RenderStyle::Print) {
        return QSize(1140, allDriversPreviewHeight(static_cast<int>(driverSnapshotIndexesForRender(style).size())));
    }
    return sizeHint();
}

QVector<int> CircuitOut::driverSnapshotIndexesForRender(RenderStyle style) const
{
    QVector<int> indexes;
    indexes.reserve(std::max(0, m_driverSnapshotCount));

    for (int index = 0; index < m_driverSnapshotCount; ++index) {
        const DriverSnapshot& snapshot = m_driverSnapshots[index];
        if (!snapshot.valid) {
            continue;
        }

        if (style == RenderStyle::Print && !driverSnapshotPrintableInAllDriversPdf(snapshot)) {
            continue;
        }

        indexes.append(index);
    }

    return indexes;
}

bool CircuitOut::driverSnapshotHasAnyNetworkElement(const DriverSnapshot& snapshot)
{
    for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
        if (isActive(snapshot.network[unitIndex])) {
            return true;
        }
    }
    return false;
}

bool CircuitOut::driverSnapshotPrintableInAllDriversPdf(const DriverSnapshot& snapshot)
{
    return snapshot.valid &&
           (driverSnapshotHasAnyNetworkElement(snapshot) ||
            snapshot.curveOrTotalFlagActive);
}

void CircuitOut::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);
    m_sectionHits.clear();
    m_driverHits.clear();
    m_driverActivityLampHits.clear();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawPreview(painter, rect(), RenderStyle::Screen);
}

void CircuitOut::renderForPrint(QPainter& painter, const QRectF& targetRect)
{
    if (!targetRect.isValid() || targetRect.isEmpty()) {
        return;
    }

    QSize sourceSize = sourceSizeForRender(RenderStyle::Print);
    if (!sourceSize.isValid() || sourceSize.isEmpty()) {
        sourceSize = QSize(1140, 330);
    }

    const qreal leftInset = targetRect.width() * 0.00;
    const qreal rightInset = targetRect.width() * 0.08;
    const QRectF contentRect = targetRect.adjusted(leftInset, 0.0, -rightInset, 0.0);
    if (!contentRect.isValid() || contentRect.isEmpty()) {
        return;
    }

    const qreal xScale = contentRect.width() / static_cast<qreal>(sourceSize.width());
    const qreal yScale = contentRect.height() / static_cast<qreal>(sourceSize.height());
    const qreal scale = std::min(xScale, yScale);
    if (scale <= 0.0) {
        return;
    }

    const QSizeF scaledSize(sourceSize.width() * scale, sourceSize.height() * scale);
    const QPointF topLeft(contentRect.left() + (contentRect.width() - scaledSize.width()) / 2.0,
                          contentRect.top());

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(targetRect, QColor(Qt::white));
    painter.translate(topLeft);
    painter.scale(scale, scale);
    drawPreview(painter, QRect(QPoint(0, 0), sourceSize), RenderStyle::Print);
    painter.restore();
}

void CircuitOut::drawPreview(QPainter& painter, const QRect& previewRect, RenderStyle style)
{
    const RenderStyle previousRenderStyle = m_renderStyle;
    m_renderStyle = style;

    painter.fillRect(previewRect, backgroundFillColor());
    painter.setPen(QPen(panelBorderColor(), 1.0));
    painter.drawRect(previewRect.adjusted(0, 0, -1, -1));

    if (!m_showAllDrivers) {
        drawCurrentDriverPreview(painter, previewRect.adjusted(12, 10, -12, -10));
        m_renderStyle = previousRenderStyle;
        return;
    }

    const QVector<int> renderIndexes = driverSnapshotIndexesForRender(style);
    if (renderIndexes.isEmpty()) {
        if (m_driverSnapshotCount <= 0) {
            drawNoDriversMessage(painter, previewRect.adjusted(12, 10, -12, -10));
        } else {
            drawNoPrintableDriversMessage(painter, previewRect.adjusted(12, 10, -12, -10));
        }
        m_renderStyle = previousRenderStyle;
        return;
    }

    DriverSnapshot previousSnapshot;
    previousSnapshot.network = m_network;
    previousSnapshot.title = m_driverTitle;
    previousSnapshot.driverNumber = m_driverNumber;
    previousSnapshot.boxTypeProposal = m_boxTypeProposal;
    previousSnapshot.vb = m_vb;
    previousSnapshot.fb = m_fb;
    previousSnapshot.v2 = m_v2;
    previousSnapshot.curveOrTotalFlagActive = m_curveOrTotalFlagActive;
    previousSnapshot.valid = true;

    const QRect outerRect = previewRect.adjusted(8, 8, -8, -8);
    const int rowGap = 10;
    const int renderDriverCount = static_cast<int>(renderIndexes.size());
    const int previewHeight = std::max(320, (outerRect.height() - (renderDriverCount - 1) * rowGap) / renderDriverCount);

    int top = outerRect.top();
    for (const int index : renderIndexes) {
        const QRect previewPanel(outerRect.left(), top, outerRect.width(), previewHeight);
        painter.fillRect(previewPanel, backgroundFillColor());
        painter.setPen(QPen(panelBorderColor(), 1.0));
        painter.drawRect(previewPanel.adjusted(0, 0, -1, -1));

        applySnapshot(m_driverSnapshots[index]);
        drawCurrentDriverPreview(painter, previewPanel.adjusted(10, 8, -10, -8));
        top += previewHeight + rowGap;
    }

    applySnapshot(previousSnapshot);
    m_renderStyle = previousRenderStyle;
}

void CircuitOut::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        DriverActivityLampHit lampHit;
        if (findDriverActivityLampHit(event->pos(), lampHit)) {
            emit driverActivityLampClicked(lampHit.driverIndex);
            event->accept();
            return;
        }

        NetworkSectionHit hit;
        if (findSectionHit(event->pos(), hit)) {
            emit networkSectionClicked(hit.driverIndex, hit.sectionIndex, hit.group);
            event->accept();
            return;
        }

        DriverHit driverHit;
        if (findDriverHit(event->pos(), driverHit)) {
            emit driverClicked(driverHit.driverIndex);
            event->accept();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void CircuitOut::contextMenuEvent(QContextMenuEvent *event)
{
    NetworkSectionHit hit;
    if (findSectionHit(event->pos(), hit)) {
        emit networkSectionContextMenuRequested(hit.driverIndex,
                                                hit.sectionIndex,
                                                hit.group,
                                                event->globalPos());
        event->accept();
        return;
    }

    QWidget::contextMenuEvent(event);
}

void CircuitOut::mouseMoveEvent(QMouseEvent *event)
{
    updateHoverHit(event->pos());
    QWidget::mouseMoveEvent(event);
}

void CircuitOut::leaveEvent(QEvent *event)
{
    clearHoverHit();
    QWidget::leaveEvent(event);
}

void CircuitOut::registerSectionHit(int section, NetworkHitGroup group, const QRectF& bounds) const
{
    if (printRenderStyle() || !bounds.isValid()) {
        return;
    }

    NetworkSectionHit hit;
    hit.bounds = bounds;
    hit.driverIndex = std::clamp(m_driverNumber - 1, 0, 3);
    hit.sectionIndex = std::clamp(section, 0, SectionCount - 1);
    hit.group = group;
    m_sectionHits.append(hit);
}

void CircuitOut::registerDriverHit(const QRectF& bounds) const
{
    if (printRenderStyle() || !bounds.isValid()) {
        return;
    }

    DriverHit hit;
    hit.bounds = bounds;
    hit.driverIndex = std::clamp(m_driverNumber - 1, 0, 3);
    m_driverHits.append(hit);
}

void CircuitOut::registerDriverActivityLampHit(const QRectF& bounds) const
{
    if (printRenderStyle() || !bounds.isValid()) {
        return;
    }

    DriverActivityLampHit hit;
    hit.bounds = bounds;
    hit.driverIndex = std::clamp(m_driverNumber - 1, 0, 3);
    m_driverActivityLampHits.append(hit);
}

bool CircuitOut::findSectionHit(const QPoint& position, NetworkSectionHit& hit) const
{
    for (auto it = m_sectionHits.crbegin(); it != m_sectionHits.crend(); ++it) {
        if (it->bounds.contains(position)) {
            hit = *it;
            return true;
        }
    }
    return false;
}

bool CircuitOut::findDriverHit(const QPoint& position, DriverHit& hit) const
{
    for (auto it = m_driverHits.crbegin(); it != m_driverHits.crend(); ++it) {
        if (it->bounds.contains(position)) {
            hit = *it;
            return true;
        }
    }
    return false;
}

bool CircuitOut::findDriverActivityLampHit(const QPoint& position, DriverActivityLampHit& hit) const
{
    for (auto it = m_driverActivityLampHits.crbegin(); it != m_driverActivityLampHits.crend(); ++it) {
        if (it->bounds.contains(position)) {
            hit = *it;
            return true;
        }
    }
    return false;
}

bool CircuitOut::sameSectionHit(const NetworkSectionHit& lhs, const NetworkSectionHit& rhs) const
{
    return lhs.driverIndex == rhs.driverIndex &&
           lhs.sectionIndex == rhs.sectionIndex &&
           lhs.group == rhs.group;
}

bool CircuitOut::sameDriverHit(const DriverHit& lhs, const DriverHit& rhs) const
{
    return lhs.driverIndex == rhs.driverIndex;
}

bool CircuitOut::sameDriverActivityLampHit(const DriverActivityLampHit& lhs, const DriverActivityLampHit& rhs) const
{
    return lhs.driverIndex == rhs.driverIndex;
}

void CircuitOut::updateHoverHit(const QPoint& position)
{
    DriverActivityLampHit lampHit;
    if (findDriverActivityLampHit(position, lampHit)) {
        setCursor(Qt::PointingHandCursor);
        if (m_hoverHitKind != HoverHitKind::DriverActivityLamp ||
            !sameDriverActivityLampHit(m_hoverDriverActivityLampHit, lampHit)) {
            m_hoverDriverActivityLampHit = lampHit;
            m_hoverHitKind = HoverHitKind::DriverActivityLamp;
            emit networkSectionHoverLeft();
        }
        return;
    }

    NetworkSectionHit sectionHit;
    if (findSectionHit(position, sectionHit)) {
        setCursor(Qt::PointingHandCursor);
        if (m_hoverHitKind != HoverHitKind::NetworkSection ||
            !sameSectionHit(m_hoverSectionHit, sectionHit)) {
            m_hoverSectionHit = sectionHit;
            m_hoverHitKind = HoverHitKind::NetworkSection;
            emit networkSectionHovered(sectionHit.driverIndex, sectionHit.sectionIndex, sectionHit.group);
        }
        return;
    }

    DriverHit driverHit;
    if (findDriverHit(position, driverHit)) {
        setCursor(Qt::PointingHandCursor);
        if (m_hoverHitKind != HoverHitKind::Driver ||
            !sameDriverHit(m_hoverDriverHit, driverHit)) {
            m_hoverDriverHit = driverHit;
            m_hoverHitKind = HoverHitKind::Driver;
            emit driverHovered(driverHit.driverIndex);
        }
        return;
    }

    clearHoverHit();
}

void CircuitOut::clearHoverHit()
{
    unsetCursor();
    if (m_hoverHitKind == HoverHitKind::None) {
        return;
    }

    m_hoverHitKind = HoverHitKind::None;
    emit networkSectionHoverLeft();
}

void CircuitOut::drawNoDriversMessage(QPainter& painter, const QRect& messageRect) const
{
    const QFont oldFont = painter.font();
    QFont titleFont = oldFont;
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);

    painter.setFont(titleFont);
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.drawText(messageRect.left(), messageRect.top(), messageRect.width(), 24,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("No drivers available."));

    painter.setFont(oldFont);
    painter.setPen(QPen(secondaryInkColor(), 1.0));
    painter.drawText(QRectF(messageRect.left(), messageRect.top() + 34,
                            messageRect.width(), 42),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     tr("No driver slots are available for the network preview."));
}

void CircuitOut::drawNoPrintableDriversMessage(QPainter& painter, const QRect& messageRect) const
{
    const QFont oldFont = painter.font();
    QFont titleFont = oldFont;
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);

    painter.setFont(titleFont);
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.drawText(messageRect.left(), messageRect.top(), messageRect.width(), 24,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     tr("No printable drivers available."));

    painter.setFont(oldFont);
    painter.setPen(QPen(secondaryInkColor(), 1.0));
    painter.drawText(QRectF(messageRect.left(), messageRect.top() + 34,
                            messageRect.width(), 54),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                     tr("All drivers are empty and have no active plot or summary flag."));
}

void CircuitOut::drawCurrentDriverPreview(QPainter& painter, const QRect& previewRect) const
{
    painter.setPen(QPen(primaryInkColor(), 1.2));

    const QRect drawingRect = previewRect;
    const QString title = tr("Driver %1%2")
                              .arg(m_driverNumber)
                              .arg(m_driverTitle.isEmpty() ? QString() : QStringLiteral(" - %1").arg(m_driverTitle));

    const QFont oldFont = painter.font();
    QFont titleFont = oldFont;
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    painter.setFont(titleFont);
    const qreal lampSize = 14.0;
    const qreal lampGap = 8.0;
    const QRectF lampRect(drawingRect.left() + 2.0,
                          drawingRect.top() + (22.0 - lampSize) / 2.0,
                          lampSize,
                          lampSize);
    drawDriverActivityLamp(painter, lampRect, m_curveOrTotalFlagActive);
    registerDriverActivityLampHit(lampRect.adjusted(-5.0, -5.0, 5.0, 5.0));

    const int titleLeft = static_cast<int>(lampRect.right() + lampGap);
    painter.drawText(titleLeft, drawingRect.top(), drawingRect.right() - titleLeft, 22,
                     Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setPen(QPen(panelBorderColor(), 1.0));
    painter.drawLine(QPointF(drawingRect.left(), drawingRect.top() + 24),
                     QPointF(drawingRect.right(), drawingRect.top() + 24));
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.setFont(oldFont);

    const int leftLabelWidth = 74;
    const int driverWidth = 150;
    const int driverGap = 18;
    const int circuitLeft = drawingRect.left() + leftLabelWidth;
    const int circuitRight = drawingRect.right() - driverWidth - driverGap;
    const int circuitTop = drawingRect.top() + 42;
    const int signalY = circuitTop + 34;
    const int returnY = signalY + 128;
    const int footerTop = returnY + 18;

    if (!hasAnyNetworkElement()) {
        painter.setPen(QPen(secondaryInkColor(), 1));
        painter.drawText(QRectF(drawingRect.left(), footerTop, drawingRect.width(), 18),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         tr("No network elements set for this driver."));
    }

    const int sectionWidth = std::max(64, (circuitRight - circuitLeft) / SectionCount);
    const int networkRight = circuitLeft + sectionWidth * SectionCount;

    painter.setPen(QPen(primaryInkColor(), 1.4));
    const qreal sourceX = circuitLeft - 28;
    painter.drawLine(QPointF(sourceX, signalY), QPointF(circuitLeft, signalY));
    painter.drawLine(QPointF(sourceX, returnY), QPointF(networkRight + 22, returnY));
    drawAcSource(painter, sourceX, signalY, returnY);
    drawGround(painter, QPointF(sourceX, returnY));
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.drawText(QRectF(circuitLeft - 88, signalY - 16, 56, 18), Qt::AlignRight | Qt::AlignVCenter, tr("Input"));

    for (int section = 0; section < SectionCount; ++section) {
        const int x0 = circuitLeft + section * sectionWidth;
        const int x1 = (section == SectionCount - 1) ? networkRight : circuitLeft + (section + 1) * sectionWidth;
        drawSection(painter, section, x0, x1, signalY, returnY);
    }

    const QRectF driverRect(networkRight + 12, signalY - 22, driverWidth, returnY - signalY + 12);
    registerDriverHit(driverRect.adjusted(-8.0, -10.0, 10.0, 10.0));
    painter.setPen(QPen(primaryInkColor(), 1.4));
    painter.drawLine(QPointF(networkRight, signalY), QPointF(driverRect.left(), signalY));
    drawDriver(painter, driverRect, signalY, returnY);
}

double CircuitOut::unit(int section, NetworkRow row) const
{
    const int unitIndex = section * RowsPerSection + static_cast<int>(row) + 1;
    if (unitIndex < 1 || unitIndex > NetworkUnitCount) {
        return 0.0;
    }
    return m_network[unitIndex];
}

bool CircuitOut::sectionHasSeriesElement(int section) const
{
    return isActive(unit(section, SeriesR)) ||
           isActive(unit(section, SeriesC)) ||
           isActive(unit(section, SeriesL));
}

bool CircuitOut::sectionHasShuntElement(int section) const
{
    return isActive(unit(section, ShuntR)) ||
           isActive(unit(section, ShuntC)) ||
           isActive(unit(section, ShuntL));
}

bool CircuitOut::hasAnyNetworkElement() const
{
    for (int unitIndex = 1; unitIndex <= NetworkUnitCount; ++unitIndex) {
        if (isActive(m_network[unitIndex])) {
            return true;
        }
    }
    return false;
}

QString CircuitOut::valueText(double value, NetworkRow row) const
{
    switch (row) {
    case SeriesR:
    case ShuntR:
        return QStringLiteral("%1 Ω").arg(compactNumber(value));
    case SeriesC:
    case ShuntC:
        return QStringLiteral("%1 µF").arg(compactNumber(value * 1000000.0));
    case SeriesL:
    case ShuntL:
        return QStringLiteral("%1 mH").arg(compactNumber(value * 1000.0));
    default:
        return compactNumber(value);
    }
}

void CircuitOut::drawComponentValue(QPainter& painter, const QRectF& rect, const QString& text) const
{
    if (!m_showValues || text.isEmpty()) {
        return;
    }

    QFont valueFont = painter.font();
    valueFont.setPointSize(std::max(7, valueFont.pointSize() - 1));
    const QFont oldFont = painter.font();
    painter.setFont(valueFont);
    painter.setPen(QPen(secondaryInkColor(), 1));

    const QRectF textRect(rect.left() - 22,
                          rect.bottom() + 3,
                          rect.width() + 44,
                          14);
    painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, text);
    painter.setFont(oldFont);
}




void CircuitOut::drawSection(QPainter& painter, int section, int x0, int x1, int signalY, int returnY) const
{
    painter.setPen(QPen(guideInkColor(), 1, Qt::DashLine));
    painter.drawLine(QPointF(x0, signalY - 42), QPointF(x0, returnY + 4));

    const int sectionWidth = x1 - x0;
    const int seriesRight = x0 + static_cast<int>(sectionWidth * 0.62);
    const int shuntX = x0 + static_cast<int>(sectionWidth * 0.80);

    const QRectF seriesHitRect(x0,
                               signalY - 52,
                               std::max(1, seriesRight - x0),
                               112);
    registerSectionHit(section, NetworkHitGroup::SeriesSection, seriesHitRect);

    const QRectF shuntHitRect(seriesRight,
                              signalY + 8,
                              std::max(1, x1 - seriesRight),
                              std::max(1, returnY - signalY));
    registerSectionHit(section, NetworkHitGroup::ShuntSection, shuntHitRect);

    // Historical layout: the series element is always placed on the left,
    // the shunt (parallel-to-line) trap branch is always to its right.
    drawSeriesPath(painter, section, x0, seriesRight, signalY);

    painter.setPen(QPen(primaryInkColor(), 1.3));
    painter.drawLine(QPointF(seriesRight, signalY), QPointF(x1, signalY));

    if (sectionHasShuntElement(section)) {
        drawShuntTrapBranch(painter, section, shuntX, signalY, returnY);
    }

    painter.setPen(QPen(secondaryInkColor(), 1));
    painter.drawText(QRectF(x0, signalY - 54, x1 - x0, 14), Qt::AlignCenter, tr("%1").arg(section + 1));
}

void CircuitOut::drawSeriesPath(QPainter& painter, int section, int x0, int x1, int signalY) const
{
    if (!sectionHasSeriesElement(section)) {
        painter.setPen(QPen(primaryInkColor(), 1.3));
        painter.drawLine(QPointF(x0, signalY), QPointF(x1, signalY));
        return;
    }

    // Historical semantics:
    // - series C != 0  => series element is a Saugkreis (parallel R/C/L network)
    // - series C == 0  => series element is an RL series element, with R as coil resistance
    if (isActive(unit(section, SeriesC))) {
        drawSeriesSuckCircuit(painter, section, x0, x1, signalY);
        return;
    }

    drawSeriesRlElement(painter, section, x0, x1, signalY);
}

void CircuitOut::drawSeriesRlElement(QPainter& painter, int section, int x0, int x1, int signalY) const
{
    const double seriesR = unit(section, SeriesR);
    const double seriesL = unit(section, SeriesL);

    QFont markerFont = painter.font();
    markerFont.setPointSize(std::max(6, markerFont.pointSize() - 2));
    const QFont oldFont = painter.font();
    painter.setFont(markerFont);
    painter.setPen(QPen(secondaryInkColor(), 1));
    painter.drawText(QRectF(x0, signalY - 32, x1 - x0, 11), Qt::AlignCenter, tr("RL"));
    painter.setFont(oldFont);

    struct Element
    {
        NetworkRow row;
        double value;
    };

    QVector<Element> elements;
    if (isActive(seriesR)) {
        elements.append(Element{SeriesR, seriesR});
    }
    if (isActive(seriesL)) {
        elements.append(Element{SeriesL, seriesL});
    }

    if (elements.isEmpty()) {
        painter.setPen(QPen(primaryInkColor(), 1.3));
        painter.drawLine(QPointF(x0, signalY), QPointF(x1, signalY));
        return;
    }

    const int available = std::max(42, x1 - x0 - 12);
    const int elementCount = static_cast<int>(elements.size());
    const int slotWidth = std::max(34, available / elementCount);
    const int groupLeft = (x0 + x1) / 2 - (slotWidth * elementCount) / 2;

    int previousX = x0;
    for (int i = 0; i < elementCount; ++i) {
        const int centerX = groupLeft + slotWidth * i + slotWidth / 2;
        const QRectF elementRect(centerX - 17, signalY - 14, 34, 28);
        painter.setPen(QPen(primaryInkColor(), 1.3));
        painter.drawLine(QPointF(previousX, signalY), QPointF(elementRect.left(), signalY));
        if (elements.at(i).row == SeriesR) {
            drawResistor(painter, elementRect, Qt::Horizontal);
        } else {
            drawInductor(painter, elementRect, Qt::Horizontal);
        }
        drawComponentValue(painter, elementRect, valueText(elements.at(i).value, elements.at(i).row));
        previousX = static_cast<int>(elementRect.right());
    }

    painter.setPen(QPen(primaryInkColor(), 1.3));
    painter.drawLine(QPointF(previousX, signalY), QPointF(x1, signalY));
}

void CircuitOut::drawSeriesSuckCircuit(QPainter& painter, int section, int x0, int x1, int signalY) const
{
    /*
     * Saugkreis semantics for the *series* element:
     *
     * Series C != 0 means that the series element is a two-terminal
     * PARALLEL R/C/L network inserted into the main signal path.  R, C and L
     * are therefore drawn as three separate branches between the same two
     * vertical rails.  They must never be drawn as a horizontal R-C-L chain.
     */
    const double seriesR = unit(section, SeriesR);
    const double seriesL = unit(section, SeriesL);
    const bool hasSeriesR = isActive(seriesR);
    const bool hasSeriesL = isActive(seriesL);

    const int elementLeft = x0 + 3;
    const int elementRight = x1 - 3;
    const int railLeft = elementLeft + 12;
    const int railRight = elementRight - 12;

    const int rY = signalY - 27;
    const int cY = signalY;
    const int lY = signalY + 27;

    const int railTop = hasSeriesR ? rY : cY;
    const int railBottom = hasSeriesL ? lY : cY;
    const qreal valueLabelYOffset = -6.0;

    painter.setPen(QPen(primaryInkColor(), 1.3));

    // Main signal path enters and leaves the two-terminal parallel R/C/L element.
    painter.drawLine(QPointF(x0, signalY), QPointF(railLeft, signalY));
    painter.drawLine(QPointF(railRight, signalY), QPointF(x1, signalY));

    // Draw the vertical rails only as far as actual branches exist so there are
    // no dangling line segments above/below the capacitor branch.
    painter.drawLine(QPointF(railLeft, railTop), QPointF(railLeft, railBottom));
    painter.drawLine(QPointF(railRight, railTop), QPointF(railRight, railBottom));

    painter.setBrush(primaryInkColor());
    painter.drawEllipse(QPointF(railLeft, signalY), 2.0, 2.0);
    painter.drawEllipse(QPointF(railRight, signalY), 2.0, 2.0);
    painter.setBrush(Qt::NoBrush);

    auto drawParallelBranch = [&](NetworkRow row, int y, bool active) {
        if (!active) {
            return;
        }

        const int branchLeft = railLeft + 5;
        const int branchRight = railRight - 5;
        const QRectF componentRect(branchLeft, y - 9,
                                   std::max<qreal>(14.0, branchRight - branchLeft),
                                   18.0);

        painter.setPen(QPen(primaryInkColor(), 1.3));
        painter.drawLine(QPointF(railLeft, y), QPointF(componentRect.left(), y));

        switch (row) {
        case SeriesR:
            drawResistor(painter, componentRect, Qt::Horizontal);
            break;
        case SeriesC:
            drawCapacitor(painter, componentRect, Qt::Horizontal);
            break;
        case SeriesL:
            drawInductor(painter, componentRect, Qt::Horizontal);
            break;
        default:
            break;
        }
        const QRectF valueRect = componentRect.translated(0.0, valueLabelYOffset);
        drawComponentValue(painter, valueRect, valueText(unit(section, row), row));

        painter.drawLine(QPointF(componentRect.right(), y), QPointF(railRight, y));
    };

    // Draw in vertical order: R branch above C branch above L branch.
    // The C branch is always active here because this function is called only
    // for SeriesC != 0. Optional R/L branches are shown only when they exist.
    drawParallelBranch(SeriesR, rY, hasSeriesR);
    drawParallelBranch(SeriesC, cY, true);
    drawParallelBranch(SeriesL, lY, hasSeriesL);
}

void CircuitOut::drawShuntTrapBranch(QPainter& painter, int section, int branchX, int signalY, int returnY) const
{
    painter.setPen(QPen(primaryInkColor(), 1.3));
    painter.setBrush(primaryInkColor());
    painter.drawEllipse(QPointF(branchX, signalY), 2.0, 2.0);
    painter.setBrush(Qt::NoBrush);

    const NetworkRow rows[3] = { ShuntR, ShuntC, ShuntL };
    const int firstTop = signalY + 10;
    const int usableHeight = std::max(54, returnY - firstTop - 2);
    const int slotHeight = usableHeight / 3;

    qreal previousY = signalY;
    for (int index = 0; index < 3; ++index) {
        const NetworkRow row = rows[index];
        const int slotTop = firstTop + index * slotHeight;
        const int slotBottom = (index == 2) ? returnY - 2 : firstTop + (index + 1) * slotHeight;
        const int centerY = (slotTop + slotBottom) / 2;
        const QRectF rect(branchX - 13, centerY - 12, 26, 24);

        painter.setPen(QPen(primaryInkColor(), 1.3));
        painter.drawLine(QPointF(branchX, previousY), QPointF(branchX, rect.top()));
        if (isActive(unit(section, row))) {
            switch (row) {
            case ShuntR:
                drawResistor(painter, rect, Qt::Vertical);
                break;
            case ShuntC:
                drawCapacitor(painter, rect, Qt::Vertical);
                break;
            case ShuntL:
                drawInductor(painter, rect, Qt::Vertical);
                break;
            default:
                break;
            }
            drawComponentValue(painter, rect, valueText(unit(section, row), row));
        } else {
            painter.drawLine(QPointF(branchX, rect.top()), QPointF(branchX, rect.bottom()));
        }
        previousY = rect.bottom();
    }

    painter.setPen(QPen(primaryInkColor(), 1.3));
    painter.drawLine(QPointF(branchX, previousY), QPointF(branchX, returnY));
    painter.setBrush(primaryInkColor());
    painter.drawEllipse(QPointF(branchX, returnY), 2.0, 2.0);
    painter.setBrush(Qt::NoBrush);
}


void CircuitOut::drawResistor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const
{
    painter.setPen(QPen(primaryInkColor(), 1.6));
    if (orientation == Qt::Horizontal) {
        const qreal cy = rect.center().y();
        painter.drawLine(QPointF(rect.left(), cy), QPointF(rect.left() + 5, cy));
        QPainterPath path(QPointF(rect.left() + 5, cy));
        const qreal step = (rect.width() - 10) / 6.0;
        for (int i = 0; i < 6; ++i) {
            path.lineTo(rect.left() + 5 + step * (i + 0.5), cy + (i % 2 == 0 ? -7 : 7));
            path.lineTo(rect.left() + 5 + step * (i + 1.0), cy);
        }
        painter.drawPath(path);
        painter.drawLine(QPointF(rect.right() - 5, cy), QPointF(rect.right(), cy));
    } else {
        const qreal cx = rect.center().x();
        painter.drawLine(QPointF(cx, rect.top()), QPointF(cx, rect.top() + 4));
        QPainterPath path(QPointF(cx, rect.top() + 4));
        const qreal step = (rect.height() - 8) / 6.0;
        for (int i = 0; i < 6; ++i) {
            path.lineTo(cx + (i % 2 == 0 ? -7 : 7), rect.top() + 4 + step * (i + 0.5));
            path.lineTo(cx, rect.top() + 4 + step * (i + 1.0));
        }
        painter.drawPath(path);
        painter.drawLine(QPointF(cx, rect.bottom() - 4), QPointF(cx, rect.bottom()));
    }
}

void CircuitOut::drawCapacitor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const
{
    painter.setPen(QPen(primaryInkColor(), 1.6));
    if (orientation == Qt::Horizontal) {
        const qreal cy = rect.center().y();
        const qreal cx = rect.center().x();
        painter.drawLine(QPointF(rect.left(), cy), QPointF(cx - 5, cy));
        painter.drawLine(QPointF(cx + 5, cy), QPointF(rect.right(), cy));
        painter.drawLine(QPointF(cx - 5, rect.top() + 4), QPointF(cx - 5, rect.bottom() - 4));
        painter.drawLine(QPointF(cx + 5, rect.top() + 4), QPointF(cx + 5, rect.bottom() - 4));
    } else {
        const qreal cx = rect.center().x();
        const qreal cy = rect.center().y();
        painter.drawLine(QPointF(cx, rect.top()), QPointF(cx, cy - 5));
        painter.drawLine(QPointF(cx, cy + 5), QPointF(cx, rect.bottom()));
        painter.drawLine(QPointF(rect.left() + 4, cy - 5), QPointF(rect.right() - 4, cy - 5));
        painter.drawLine(QPointF(rect.left() + 4, cy + 5), QPointF(rect.right() - 4, cy + 5));
    }
}

void CircuitOut::drawInductor(QPainter& painter, const QRectF& rect, Qt::Orientation orientation) const
{
    painter.setPen(QPen(primaryInkColor(), 1.6));
    if (orientation == Qt::Horizontal) {
        const qreal cy = rect.center().y();
        painter.drawLine(QPointF(rect.left(), cy), QPointF(rect.left() + 4, cy));
        const qreal radius = (rect.width() - 8) / 8.0;
        for (int i = 0; i < 4; ++i) {
            const QRectF arcRect(rect.left() + 4 + i * 2 * radius, cy - radius, 2 * radius, 2 * radius);
            painter.drawArc(arcRect, 0, 180 * 16);
        }
        painter.drawLine(QPointF(rect.right() - 4, cy), QPointF(rect.right(), cy));
    } else {
        const qreal cx = rect.center().x();
        painter.drawLine(QPointF(cx, rect.top()), QPointF(cx, rect.top() + 4));
        const qreal radius = (rect.height() - 8) / 8.0;
        for (int i = 0; i < 4; ++i) {
            const QRectF arcRect(cx - radius, rect.top() + 4 + i * 2 * radius, 2 * radius, 2 * radius);
            painter.drawArc(arcRect, 90 * 16, 180 * 16);
        }
        painter.drawLine(QPointF(cx, rect.bottom() - 4), QPointF(cx, rect.bottom()));
    }
}

void CircuitOut::drawEquivalentDriverCircuit(QPainter& painter, const QRectF& rect, int signalY, int returnY,
                                             qreal shuntStartX, bool drawUpperShuntBus,
                                             qreal shuntBranchShiftX) const
{
    const EquivalentDriverGeometry geometry = buildEquivalentDriverGeometry(rect, signalY, returnY,
                                                                           shuntStartX, shuntBranchShiftX);

    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.drawLine(QPointF(rect.left(), geometry.topY), QPointF(geometry.rSeriesRect.left(), geometry.topY));
    drawResistor(painter, geometry.rSeriesRect, Qt::Horizontal);
    painter.drawLine(QPointF(geometry.rSeriesRect.right(), geometry.topY), QPointF(geometry.lSeriesRect.left(), geometry.topY));
    drawInductor(painter, geometry.lSeriesRect, Qt::Horizontal);
    painter.drawLine(QPointF(geometry.lSeriesRect.right(), geometry.topY), QPointF(geometry.busRightX, geometry.topY));

    const qreal componentTop = geometry.topY + 10;
    const qreal componentHeight = std::max<qreal>(24.0, geometry.bottomY - componentTop - 10);

    if (drawUpperShuntBus) {
        painter.drawLine(QPointF(geometry.branchX.front(), componentTop),
                         QPointF(geometry.branchX.back(), componentTop));
    }

    for (int i = 0; i < 3; ++i) {
        const qreal x = geometry.branchX[static_cast<std::size_t>(i)];
        // Keep the shunt R/C/L symbols visually compact and use explicit
        // connector lines above and below so the branch looks less bulky.
        qreal compactHeight = componentHeight;
        qreal componentWidth = 10.0;
        switch (i) {
        case 0:
            compactHeight = std::min<qreal>(24.0, componentHeight);
            componentWidth = 10.0;
            break;
        case 1:
            compactHeight = std::min<qreal>(18.0, componentHeight);
            componentWidth = 12.0;
            break;
        default:
            compactHeight = std::min<qreal>(26.0, componentHeight);
            componentWidth = 14.0;
            break;
        }

        const qreal compactTop = componentTop + (componentHeight - compactHeight) / 2.0;
        const QRectF componentRect(x - componentWidth / 2.0, compactTop, componentWidth, compactHeight);

        painter.drawLine(QPointF(x, geometry.topY), QPointF(x, componentRect.top()));
        switch (i) {
        case 0:
            drawResistor(painter, componentRect, Qt::Vertical);
            break;
        case 1:
            drawCapacitor(painter, componentRect, Qt::Vertical);
            break;
        default:
            drawInductor(painter, componentRect, Qt::Vertical);
            break;
        }
        painter.drawLine(QPointF(x, componentRect.bottom()), QPointF(x, geometry.bottomY));
    }

    painter.drawLine(QPointF(geometry.branchX.front() - 8, geometry.bottomY),
                     QPointF(geometry.branchX.back(), geometry.bottomY));
}

void CircuitOut::drawDriverActivityLamp(QPainter& painter, const QRectF& rect, bool active) const
{
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor rimColor;
    QColor shadowColor;
    QColor fillColor;
    QColor coreColor;
    QColor highlightColor;

    if (printRenderStyle()) {
        rimColor = QColor(35, 35, 35);
        shadowColor = QColor(205, 205, 205);
        fillColor = QColor(238, 238, 238);
        coreColor = active ? QColor(45, 45, 45) : QColor(175, 175, 175);
        highlightColor = QColor(255, 255, 255);
    } else {
        rimColor = useLightInk() ? QColor(225, 225, 225)
                                 : QColor(35, 35, 35);
        shadowColor = useLightInk() ? QColor(45, 45, 45)
                                    : QColor(115, 115, 115);
        fillColor = active ? QColor(80, 205, 95)
                           : (useLightInk() ? QColor(82, 82, 82)
                                            : QColor(118, 118, 118));
        coreColor = active ? QColor(40, 150, 55)
                           : (useLightInk() ? QColor(58, 58, 58)
                                            : QColor(88, 88, 88));
        highlightColor = QColor(210, 255, 215);
    }

    painter.setPen(QPen(shadowColor, 1.0));
    painter.setBrush(QBrush(shadowColor));
    painter.drawEllipse(rect.adjusted(1.2, 1.2, 1.2, 1.2));

    painter.setPen(QPen(rimColor, 1.1));
    painter.setBrush(QBrush(fillColor));
    painter.drawEllipse(rect);

    const QRectF coreRect = rect.adjusted(rect.width() * 0.23,
                                         rect.height() * 0.23,
                                         -rect.width() * 0.23,
                                         -rect.height() * 0.23);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QBrush(coreColor));
    painter.drawEllipse(coreRect);

    if (active) {
        const QRectF highlightRect(rect.left() + rect.width() * 0.24,
                                   rect.top() + rect.height() * 0.18,
                                   rect.width() * 0.27,
                                   rect.height() * 0.27);
        painter.setBrush(QBrush(highlightColor));
        painter.drawEllipse(highlightRect);
    }

    painter.restore();
}

void CircuitOut::drawPort(QPainter& painter, const QRectF& rect, bool leftFacing) const
{
    const QPen portPen(primaryInkColor(), 1.2);
    painter.setPen(portPen);
    painter.setBrush(Qt::NoBrush);

    const qreal tubeHeight = std::max<qreal>(10.0, rect.height() * 0.74);
    const qreal tubeY = rect.center().y() - tubeHeight / 2.0;
    const qreal mouthWidth = std::max<qreal>(6.5, rect.height() * 0.42);
    const qreal mouthHeight = std::max<qreal>(tubeHeight + 3.0, rect.height() * 0.92);

    const auto drawRadiation = [&](const QRectF& mouthRect, bool radiatesLeft) {
        const qreal centerY = mouthRect.center().y();
        const qreal baseArcWidth = std::max<qreal>(5.0, rect.height() * 0.28);
        const qreal baseArcHeight = std::max<qreal>(10.0, rect.height() * 0.62);

        for (int i = 0; i < 3; ++i) {
            const qreal arcWidth = baseArcWidth + i * 3.0;
            const qreal arcHeight = baseArcHeight + i * 5.0;
            const qreal offset = 3.0 + i * 2.0;

            if (radiatesLeft) {
                const QRectF arcRect(mouthRect.left() - offset - arcWidth,
                                     centerY - arcHeight / 2.0,
                                     arcWidth,
                                     arcHeight);
                painter.drawArc(arcRect, 125 * 16, 110 * 16);
            } else {
                const QRectF arcRect(mouthRect.right() + offset,
                                     centerY - arcHeight / 2.0,
                                     arcWidth,
                                     arcHeight);
                painter.drawArc(arcRect, -55 * 16, 110 * 16);
            }
        }
    };

    const auto drawMouth = [&](const QRectF& mouthRect) {
        // Fill the mouth with the panel background so the tube side wall stays hidden
        // behind the opening while preserving a simple outlined port symbol.
        painter.save();
        painter.setPen(portPen);
        painter.setBrush(backgroundFillColor());
        painter.drawEllipse(mouthRect);
        painter.restore();
    };

    if (leftFacing) {
        const QRectF tubeRect(rect.left() + mouthWidth * 0.18, tubeY,
                              rect.width() - mouthWidth * 0.18, tubeHeight);
        painter.drawRoundedRect(tubeRect, 1.8, 1.8);
        const QRectF mouthRect(tubeRect.left() - mouthWidth * 0.60,
                               rect.center().y() - mouthHeight / 2.0,
                               mouthWidth,
                               mouthHeight);
        drawMouth(mouthRect);
        painter.drawLine(QPointF(tubeRect.right(), tubeRect.center().y()),
                         QPointF(tubeRect.right() + 3.5, tubeRect.center().y()));
        drawRadiation(mouthRect, true);
    } else {
        const QRectF tubeRect(rect.left(), tubeY,
                              rect.width() - mouthWidth * 0.18, tubeHeight);
        painter.drawRoundedRect(tubeRect, 1.8, 1.8);
        const QRectF mouthRect(tubeRect.right() - mouthWidth * 0.40,
                               rect.center().y() - mouthHeight / 2.0,
                               mouthWidth,
                               mouthHeight);
        drawMouth(mouthRect);
        painter.drawLine(QPointF(tubeRect.left() - 3.5, tubeRect.center().y()),
                         QPointF(tubeRect.left(), tubeRect.center().y()));
        drawRadiation(mouthRect, false);
    }
}

void CircuitOut::drawSpeakerSymbol(QPainter& painter, const QRectF& rect) const
{
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.setBrush(Qt::NoBrush);

    const qreal cy = rect.center().y();
    const qreal blockW = rect.width() * 0.18;
    const qreal coneLeft = rect.left() + blockW + 3;
    const qreal coneRight = rect.left() + rect.width() * 0.56;

    const QRectF blockRect(rect.left(), cy - rect.height() * 0.18, blockW, rect.height() * 0.36);
    painter.drawRoundedRect(blockRect, 2.0, 2.0);

    QPolygonF cone;
    cone << QPointF(coneLeft, cy - rect.height() * 0.28)
         << QPointF(coneRight, cy - rect.height() * 0.42)
         << QPointF(coneRight, cy + rect.height() * 0.42)
         << QPointF(coneLeft, cy + rect.height() * 0.28);
    painter.drawPolygon(cone);

    const qreal arcX = coneRight + 3;
    const qreal arcTop = cy - rect.height() * 0.36;
    const qreal arcH = rect.height() * 0.72;
    for (int i = 0; i < 3; ++i) {
        const qreal w = rect.width() * (0.18 + i * 0.12);
        const QRectF arcRect(arcX + i * 2.0, arcTop - i * 3.0, w, arcH + i * 6.0);
        painter.drawArc(arcRect, -55 * 16, 110 * 16);
    }
}

void CircuitOut::drawEnclosureHatching(QPainter& painter, const QRectF& rect) const
{
    if (rect.width() < 8.0 || rect.height() < 8.0) {
        return;
    }

    painter.save();

    // Keep the hatching subtle so the box still reads as visually transparent
    // and the parameter text / equivalent circuit remain easy to read.
    QColor hatchColor = enclosureOutlineColor();
    hatchColor.setAlpha(useLightInk() ? 42 : 58);
    painter.setPen(QPen(hatchColor, 0.8));

    const QRectF hatchRect = rect.adjusted(1.5, 1.5, -1.5, -1.5);
    painter.setClipRect(hatchRect);

    const qreal spacing = 10.0;
    for (qreal x = hatchRect.left() - hatchRect.height(); x < hatchRect.right(); x += spacing) {
        painter.drawLine(QPointF(x, hatchRect.bottom()),
                         QPointF(x + hatchRect.height(), hatchRect.top()));
    }

    painter.restore();
}

void CircuitOut::drawBoxParameterText(QPainter& painter, const QRectF& boxRect, int boxType) const
{
    if (boxType <= 0) {
        return;
    }

    QStringList lines;
    lines << QStringLiteral("Vb=%1L").arg(compactNumber(m_vb, 4));
    if (boxType >= 2) {
        lines << QStringLiteral("Fb=%1Hz").arg(compactNumber(m_fb, 4));
    }
    if (boxType >= 3) {
        lines << QStringLiteral("V2=%1L").arg(compactNumber(m_v2, 4));
    }

    QFont textFont = painter.font();
    textFont.setPointSize(std::max(7, textFont.pointSize() - 1));
    const QFont oldFont = painter.font();
    painter.setFont(textFont);
    painter.setPen(QPen(secondaryInkColor(), 1));

    const QFontMetrics fm(textFont);
    const int lineHeight = fm.height();
    const qreal totalHeight = lineHeight * lines.size();

    QRectF textRect(boxRect.left() + 4,
                    boxRect.center().y() - totalHeight / 2.0,
                    boxRect.width() - 8,
                    totalHeight + 2);
    Qt::Alignment textAlignment = Qt::AlignCenter;

    if (boxType == 1) {
        // In the sealed-enclosure view keep the parameter block left of the
        // equivalent circuit so the text does not collide with the shunt R symbol.
        textRect = QRectF(boxRect.left() + 6,
                          boxRect.center().y() - totalHeight / 2.0,
                          boxRect.width() * 0.42,
                          totalHeight + 2);
        textAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    } else if (boxType == 2) {
        // In the vented-enclosure view, move the parameter text a little further
        // left while keeping the existing centered layout otherwise unchanged.
        textRect.translate(-12.0, 0.0);
    } else if (boxType >= 3) {
        // In the bandpass view keep the parameter block clearly inside the
        // left chamber so it stays readable next to the partition-mounted
        // loudspeaker symbol.
        textRect = QRectF(boxRect.left() + 4,
                          boxRect.center().y() - totalHeight / 2.0,
                          boxRect.width() * 0.33,
                          totalHeight + 2);
        textAlignment = Qt::AlignLeft | Qt::AlignVCenter;
    }

    painter.drawText(textRect, textAlignment, lines.join(QStringLiteral("\n")));
    painter.setFont(oldFont);
}

void CircuitOut::drawAcSource(QPainter& painter, qreal centerX, qreal signalY, qreal returnY) const
{
    painter.setPen(QPen(primaryInkColor(), 1.4));
    painter.setBrush(Qt::NoBrush);

    const qreal availableHeight = std::max<qreal>(24.0, returnY - signalY);
    const qreal radius = std::min<qreal>(18.0, availableHeight * 0.22);
    const qreal centerY = (signalY + returnY) / 2.0;

    painter.drawLine(QPointF(centerX, signalY), QPointF(centerX, centerY - radius));
    painter.drawLine(QPointF(centerX, centerY + radius), QPointF(centerX, returnY));
    painter.drawEllipse(QPointF(centerX, centerY), radius, radius);

    QPainterPath wave;
    const qreal waveLeft = centerX - radius * 0.58;
    const qreal waveRight = centerX + radius * 0.58;
    const qreal waveAmplitude = radius * 0.34;
    wave.moveTo(waveLeft, centerY);
    wave.cubicTo(centerX - radius * 0.42, centerY - waveAmplitude,
                 centerX - radius * 0.18, centerY - waveAmplitude,
                 centerX, centerY);
    wave.cubicTo(centerX + radius * 0.18, centerY + waveAmplitude,
                 centerX + radius * 0.42, centerY + waveAmplitude,
                 waveRight, centerY);
    painter.drawPath(wave);
}

void CircuitOut::drawDriver(QPainter& painter, const QRectF& rect, int signalY, int returnY) const
{
    Q_UNUSED(signalY)

    const int boxType = std::clamp(m_boxTypeProposal, 0, 3);
    const qreal boxTop = rect.top() + 2.0;
    const qreal topGap = std::max<qreal>(6.0, static_cast<qreal>(signalY) - boxTop);
    const qreal boxBottom = static_cast<qreal>(returnY) + topGap;
    painter.setPen(QPen(primaryInkColor(), 1.2));
    painter.setBrush(Qt::NoBrush);

    if (boxType == 0) {
        // Free air: equivalent circuit plus visible loudspeaker symbol.
        const QRectF eqRect(rect.left() + 4, rect.top() + 8, rect.width() - 44, rect.height() - 28);
        drawEquivalentDriverCircuit(painter, eqRect, signalY, returnY);
        painter.drawLine(QPointF(eqRect.left() - 2, returnY), QPointF(eqRect.right() + 2, returnY));

        const qreal speakerCenterY = (static_cast<qreal>(signalY) + static_cast<qreal>(returnY)) / 2.0;
        drawSpeakerSymbol(painter, QRectF(rect.right() - 28, speakerCenterY - 18, 30, 36));
        return;
    }

    if (boxType == 1) {
        const QRectF boxRect(rect.left() + 6, boxTop, rect.width() - 28, boxBottom - boxTop);
        painter.setPen(QPen(enclosureOutlineColor(), 1.3));
        painter.drawRect(boxRect);
        drawEnclosureHatching(painter, boxRect);
        painter.setPen(QPen(primaryInkColor(), 1.2));
        drawBoxParameterText(painter, boxRect, boxType);
        painter.setPen(QPen(primaryInkColor(), 1.2));
        const qreal enclosureCircuitShift = 8.0;
        const QRectF eqRect(boxRect.left() + 10 - enclosureCircuitShift,
                            rect.top() + 8,
                            boxRect.width() - 26,
                            returnY - rect.top() - 16);
        const qreal sealedShuntBranchShiftX = 12.0;
        const EquivalentDriverGeometry eqGeometry = buildEquivalentDriverGeometry(eqRect, signalY, returnY,
                                                                                 -1.0, sealedShuntBranchShiftX);
        painter.drawLine(QPointF(boxRect.left(), signalY), QPointF(eqRect.left(), signalY));
        drawEquivalentDriverCircuit(painter, eqRect, signalY, returnY,
                                    -1.0, false, sealedShuntBranchShiftX);
        painter.drawLine(QPointF(boxRect.left(), returnY), QPointF(eqGeometry.branchX.back(), returnY));
        drawSpeakerSymbol(painter, QRectF(boxRect.right() - 2, boxRect.center().y() - 18, 30, 36));
        return;
    }

    if (boxType == 2) {
        const QRectF boxRect(rect.left() + 6, boxTop, rect.width() - 28, boxBottom - boxTop);
        painter.setPen(QPen(enclosureOutlineColor(), 1.3));
        painter.drawRect(boxRect);
        drawEnclosureHatching(painter, boxRect);
        painter.setPen(QPen(primaryInkColor(), 1.2));
        drawBoxParameterText(painter, boxRect, boxType);
        painter.setPen(QPen(primaryInkColor(), 1.2));
        const QRectF portRect(boxRect.left() + 2, returnY - 19, 40, 18);
        drawPort(painter, portRect, true);
        const qreal enclosureCircuitShift = 8.0;
        const QRectF eqRect(boxRect.left() + 38 - enclosureCircuitShift,
                            rect.top() + 8,
                            boxRect.width() - 62,
                            returnY - rect.top() - 16);
        const EquivalentDriverGeometry eqGeometry = buildEquivalentDriverGeometry(eqRect, signalY, returnY, -1.0);
        painter.drawLine(QPointF(boxRect.left(), signalY), QPointF(eqRect.left(), signalY));
        drawEquivalentDriverCircuit(painter, eqRect, signalY, returnY);
        // In the vented-enclosure view, keep the upper signal connection to the
        // shunt branch explicit up to the rightmost parallel L connection.
        painter.drawLine(QPointF(eqGeometry.lSeriesRect.right(), signalY),
                         QPointF(eqGeometry.branchX.back(), signalY));
        painter.drawLine(QPointF(boxRect.left() - 4, returnY), QPointF(eqGeometry.branchX.back(), returnY));
        drawSpeakerSymbol(painter, QRectF(boxRect.right() - 2, boxRect.center().y() - 18, 30, 36));
        return;
    }

    // boxType == 3 -> vented box with V2 proposal: two chambers, driver in the partition.
    const QRectF outerRect(rect.left() + 4, boxTop, rect.width() - 12, boxBottom - boxTop);
    painter.setPen(QPen(enclosureOutlineColor(), 1.3));
    painter.drawRect(outerRect);
    drawEnclosureHatching(painter, outerRect);
    drawBoxParameterText(painter, outerRect, boxType);
    const qreal partitionX = outerRect.left() + outerRect.width() * 0.48;
    painter.setPen(QPen(enclosureOutlineColor(), 1.3));
    painter.drawLine(QPointF(partitionX, outerRect.top()), QPointF(partitionX, outerRect.bottom()));
    painter.setPen(QPen(primaryInkColor(), 1.2));
    const QRectF portRect(outerRect.left() + 2, returnY - 19, 40, 18);
    drawPort(painter, portRect, true);

    // Give the bandpass equivalent circuit more room in the right chamber and
    // keep the shunt R/C/L branch to the right of the partition-mounted speaker.
    const QRectF speakerRect(partitionX - 6, outerRect.center().y() - 18, 30, 36);
    const qreal eqLeft = partitionX - 50;
    const QRectF eqRect(eqLeft,
                        rect.top() + 8,
                        outerRect.right() - eqLeft - 8,
                        returnY - rect.top() - 16);
    painter.drawLine(QPointF(outerRect.left(), signalY), QPointF(eqRect.left(), signalY));
    const EquivalentDriverGeometry eqGeometry = buildEquivalentDriverGeometry(eqRect, signalY, returnY, speakerRect.right() + 6.0);
    drawEquivalentDriverCircuit(painter, eqRect, signalY, returnY, speakerRect.right() + 6.0);
    painter.drawLine(QPointF(outerRect.left(), returnY), QPointF(eqGeometry.branchX.back(), returnY));

    // In the bandpass view, show the driver directly at the partition instead
    // of keeping a separate symbol at the far right.
    drawSpeakerSymbol(painter, speakerRect);
}

void CircuitOut::drawGround(QPainter& painter, QPointF center) const
{
    painter.setPen(QPen(guideInkColor(), 1.1));
    painter.drawLine(center, QPointF(center.x(), center.y() + 5));
    painter.drawLine(QPointF(center.x() - 12, center.y() + 5), QPointF(center.x() + 12, center.y() + 5));
    painter.drawLine(QPointF(center.x() - 8, center.y() + 9), QPointF(center.x() + 8, center.y() + 9));
    painter.drawLine(QPointF(center.x() - 4, center.y() + 13), QPointF(center.x() + 4, center.y() + 13));
}
