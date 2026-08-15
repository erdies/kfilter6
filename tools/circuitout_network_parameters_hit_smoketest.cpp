/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#include "circuitout.h"
#include "driver.h"

#include <QApplication>
#include <QMouseEvent>

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
    singleDriver.SetTitle(QStringLiteral("Preview driver"));
    preview.setDriver(singleDriver, 1);
    preview.show();
    QApplication::processEvents();

    int clickCount = 0;
    int lastClickDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::networkParametersClicked,
                     [&clickCount, &lastClickDriverIndex](int driverIndex) {
        ++clickCount;
        lastClickDriverIndex = driverIndex;
    });

    int hoverCount = 0;
    int lastHoverDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::networkParametersHovered,
                     [&hoverCount, &lastHoverDriverIndex](int driverIndex) {
        ++hoverCount;
        lastHoverDriverIndex = driverIndex;
    });

    // The AC source is centered near x=58/y=150 in the normal 1140x330 preview.
    // The hit area intentionally covers the source symbol plus a small click margin.
    const QPoint singleSourceCenter(58, 150);
    sendMouseMove(preview, singleSourceCenter);
    if (hoverCount != 1 || lastHoverDriverIndex != 0 ||
        preview.cursor().shape() != Qt::PointingHandCursor) {
        std::cerr << "AC-source hover did not select Driver 1\n";
        return 1;
    }

    sendLeftPress(preview, singleSourceCenter);
    if (clickCount != 1 || lastClickDriverIndex != 0) {
        std::cerr << "AC-source click did not select Driver 1\n";
        return 1;
    }

    // A point well above the AC source must not be swallowed by its hit area.
    sendLeftPress(preview, QPoint(singleSourceCenter.x(), 100));
    if (clickCount != 1) {
        std::cerr << "AC-source hit area is unexpectedly tall\n";
        return 1;
    }

    std::array<driver, 4> drivers;
    for (int index = 0; index < static_cast<int>(drivers.size()); ++index) {
        drivers[index].SetTitle(QStringLiteral("Driver %1").arg(index + 1));
    }

    preview.setDrivers(drivers.data(), static_cast<int>(drivers.size()));
    preview.resize(preview.sizeHint());
    QApplication::processEvents();

    // Derive each panel position from the actual all-drivers widget height so
    // cosmetic row-height tuning does not leave this hit test stale.
    const int driverCount = static_cast<int>(drivers.size());
    for (int driverIndex = 0; driverIndex < driverCount; ++driverIndex) {
        const int panelTop = allDriversPanelTop(preview, driverCount, driverIndex);
        const QPoint sourceCenter(64, panelTop + 148);

        sendMouseMove(preview, sourceCenter);
        if (lastHoverDriverIndex != driverIndex) {
            std::cerr << "All-drivers AC-source hover selected wrong driver: expected "
                      << driverIndex << ", got " << lastHoverDriverIndex << '\n';
            return 1;
        }

        sendLeftPress(preview, sourceCenter);
        if (lastClickDriverIndex != driverIndex) {
            std::cerr << "All-drivers AC-source click selected wrong driver: expected "
                      << driverIndex << ", got " << lastClickDriverIndex << '\n';
            return 1;
        }
    }

    std::cout << "circuitout network-parameters hit smoke test passed\n";
    return 0;
}
