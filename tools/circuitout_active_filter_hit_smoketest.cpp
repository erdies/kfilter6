/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "activefiltermodel.h"
#include "activefilterresponse.h"
#include "circuitout.h"
#include "driver.h"

#include <QApplication>
#include <QMouseEvent>
#include <QString>

#include <array>
#include <iostream>

namespace
{
void sendLeftPress(CircuitOut& preview, const QPoint& position)
{
    const QPointF localPosition(position);
    const QPointF globalPosition(preview.mapToGlobal(position));

    QMouseEvent event(QEvent::MouseButtonPress,
                      localPosition,
                      globalPosition,
                      Qt::LeftButton,
                      Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&preview, &event);
}

void sendMouseMove(CircuitOut& preview, const QPoint& position)
{
    const QPointF localPosition(position);
    const QPointF globalPosition(preview.mapToGlobal(position));

    QMouseEvent event(QEvent::MouseMove,
                      localPosition,
                      globalPosition,
                      Qt::NoButton,
                      Qt::NoButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&preview, &event);
}

int allDriversPanelTop(const CircuitOut& preview, int driverCount, int driverIndex)
{
    constexpr int outerMargin = 8;
    constexpr int rowGap = 10;

    const int panelHeight =
        (preview.height() - 2 * outerMargin - (driverCount - 1) * rowGap) / driverCount;
    return outerMargin + driverIndex * (panelHeight + rowGap);
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    CircuitOut preview;
    preview.resize(1140, 330);

    driver singleDriver;
    singleDriver.setTitle(QStringLiteral("Preview driver"));
    preview.setDriver(singleDriver, 1);
    preview.show();
    QApplication::processEvents();

    int clickCount = 0;
    int lastDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::activeFilterClicked,
                     [&clickCount, &lastDriverIndex](int driverIndex) {
        ++clickCount;
        lastDriverIndex = driverIndex;
    });

    int hoverCount = 0;
    int lastHoverDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::activeFilterHovered,
                     [&hoverCount, &lastHoverDriverIndex](int driverIndex) {
        ++hoverCount;
        lastHoverDriverIndex = driverIndex;
    });

    // Patch 233: hovering the same header strip must expose the owning driver
    // so the main window can show the Active Filter status-bar hint.
    sendMouseMove(preview, QPoint(preview.width() - 80, 20));
    if (hoverCount != 1 || lastHoverDriverIndex != 0) {
        std::cerr << "Empty active-filter header hover did not select Driver 1\n";
        return 1;
    }

    // Patch 231: the free header strip right of the driver name must already
    // open Active Filter Parameters even when no active filter is defined.
    sendLeftPress(preview, QPoint(preview.width() - 80, 20));
    if (clickCount != 1 || lastDriverIndex != 0) {
        std::cerr << "Empty active-filter header area did not select Driver 1\n";
        return 1;
    }

    // The driver title itself must keep its existing semantics and must not be
    // swallowed by the new active-filter hit area.
    sendLeftPress(preview, QPoint(90, 20));
    if (clickCount != 1) {
        std::cerr << "Active-filter hit area overlaps the driver title\n";
        return 1;
    }

    ActiveFilterChain chain;
    chain.setEnabled(true);
    chain.addSection(ActiveFilterType::Gain);
    preview.setActiveFilterState(0, chain, ActiveFilterResponseStatus::Valid);
    QApplication::processEvents();

    sendLeftPress(preview, QPoint(preview.width() - 80, 20));
    if (clickCount != 2 || lastDriverIndex != 0) {
        std::cerr << "Visible active-filter summary did not select Driver 1\n";
        return 1;
    }

    std::array<driver, 4> drivers;
    for (int index = 0; index < static_cast<int>(drivers.size()); ++index) {
        drivers[index].setTitle(QStringLiteral("Driver %1").arg(index + 1));
    }

    preview.setDrivers(drivers.data(), static_cast<int>(drivers.size()));
    preview.resize(preview.sizeHint());
    QApplication::processEvents();

    const int driverCount = static_cast<int>(drivers.size());
    for (int driverIndex = 0; driverIndex < driverCount; ++driverIndex) {
        const int headerY = allDriversPanelTop(preview, driverCount, driverIndex) + 18;
        sendMouseMove(preview, QPoint(preview.width() - 80, headerY));
        if (lastHoverDriverIndex != driverIndex) {
            std::cerr << "All-drivers active-filter hover selected wrong driver: expected "
                      << driverIndex << ", got " << lastHoverDriverIndex << '\n';
            return 1;
        }

        sendLeftPress(preview, QPoint(preview.width() - 80, headerY));
        if (lastDriverIndex != driverIndex) {
            std::cerr << "All-drivers active-filter hit selected wrong driver: expected "
                      << driverIndex << ", got " << lastDriverIndex << '\n';
            return 1;
        }
    }

    std::cout << "circuitout active-filter hit smoke test passed\n";
    return 0;
}
