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

QPoint findHoverPoint(CircuitOut& preview,
                      const QRect& searchRect,
                      int expectedDriverIndex,
                      int& lastHoverDriverIndex)
{
    for (int y = searchRect.top(); y <= searchRect.bottom(); y += 2) {
        for (int x = searchRect.left(); x <= searchRect.right(); x += 2) {
            sendMouseMove(preview, QPoint(x, y));
            if (lastHoverDriverIndex == expectedDriverIndex) {
                return QPoint(x, y);
            }
        }
    }
    return QPoint(-1, -1);
}

QPoint findBaffleHoverAnywhere(CircuitOut& preview,
                              int expectedDriverIndex,
                              int& lastHoverDriverIndex)
{
    return findHoverPoint(preview,
                          QRect(0, 0, preview.width() - 1, preview.height() - 1),
                          expectedDriverIndex,
                          lastHoverDriverIndex);
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

    int baffleClickCount = 0;
    int lastBaffleClickDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::baffleParametersClicked,
                     [&baffleClickCount, &lastBaffleClickDriverIndex](int driverIndex) {
        ++baffleClickCount;
        lastBaffleClickDriverIndex = driverIndex;
    });

    int baffleHoverCount = 0;
    int lastBaffleHoverDriverIndex = -1;
    QObject::connect(&preview, &CircuitOut::baffleParametersHovered,
                     [&baffleHoverCount, &lastBaffleHoverDriverIndex](int driverIndex) {
        ++baffleHoverCount;
        lastBaffleHoverDriverIndex = driverIndex;
    });

    int driverClickCount = 0;
    QObject::connect(&preview, &CircuitOut::driverClicked,
                     [&driverClickCount](int) {
        ++driverClickCount;
    });

    int driverHoverCount = 0;
    int lastDriverHoverIndex = -1;
    QObject::connect(&preview, &CircuitOut::driverHovered,
                     [&driverHoverCount, &lastDriverHoverIndex](int driverIndex) {
        ++driverHoverCount;
        lastDriverHoverIndex = driverIndex;
    });

    // Find the existing loudspeaker radiation-wave symbol at the far right of
    // the free-air driver graphic. Searching the compact expected region keeps
    // this test robust against small geometry changes while still verifying
    // that the hit zone remains attached to the wave symbol itself.
    const QPoint wavePoint = findHoverPoint(preview,
                                            QRect(preview.width() - 90, 105, 85, 105),
                                            0,
                                            lastBaffleHoverDriverIndex);
    if (wavePoint.x() < 0 || baffleHoverCount < 1 ||
        preview.cursor().shape() != Qt::PointingHandCursor) {
        std::cerr << "Could not locate Driver 1 Baffle / Diffraction wave hit zone\n";
        return 1;
    }

    sendLeftPress(preview, wavePoint);
    if (baffleClickCount != 1 || lastBaffleClickDriverIndex != 0) {
        std::cerr << "Wave-symbol click did not select Baffle / Diffraction for Driver 1\n";
        return 1;
    }
    if (driverClickCount != 0) {
        std::cerr << "Wave-symbol click leaked into the Driver Parameters hit zone\n";
        return 1;
    }

    // The loudspeaker/equivalent-driver area immediately left of the radiation
    // symbol must retain its Driver Parameters semantics.
    lastDriverHoverIndex = -1;
    const QPoint driverPoint = findHoverPoint(preview,
                                              QRect(preview.width() - 190, 90, 105, 135),
                                              0,
                                              lastDriverHoverIndex);
    if (driverPoint.x() < 0 || driverHoverCount < 1) {
        std::cerr << "Could not locate the remaining Driver Parameters hit zone\n";
        return 1;
    }

    sendLeftPress(preview, driverPoint);
    if (driverClickCount != 1 || baffleClickCount != 1) {
        std::cerr << "Driver Parameters and Baffle / Diffraction hit zones are not cleanly separated\n";
        return 1;
    }

    // The wave hit zone is derived from the actual speaker-symbol geometry.
    // Exercise every enclosure layout because sealed/vented speakers sit at the
    // cabinet edge while the bandpass speaker moves to the internal partition.
    for (int boxType = 0; boxType <= 3; ++boxType) {
        driver enclosureDriver;
        enclosureDriver.SetTitle(QStringLiteral("Enclosure %1").arg(boxType));
        enclosureDriver.Vb = boxType == 0 ? 0.0 : 20.0;
        enclosureDriver.GTypProposal = boxType;
        if (boxType == 3) {
            enclosureDriver.V2 = 8.0;
        }

        preview.setDriver(enclosureDriver, 1);
        preview.resize(1140, 330);
        QApplication::processEvents();

        lastBaffleHoverDriverIndex = -1;
        const QPoint enclosureWavePoint = findBaffleHoverAnywhere(
            preview, 0, lastBaffleHoverDriverIndex);
        if (enclosureWavePoint.x() < 0 || lastBaffleHoverDriverIndex != 0) {
            std::cerr << "Could not locate Baffle / Diffraction wave hit zone for box type "
                      << boxType << '\n';
            return 1;
        }

        const int previousBaffleClicks = baffleClickCount;
        const int previousDriverClicks = driverClickCount;
        sendLeftPress(preview, enclosureWavePoint);
        if (baffleClickCount != previousBaffleClicks + 1 ||
            lastBaffleClickDriverIndex != 0 ||
            driverClickCount != previousDriverClicks) {
            std::cerr << "Baffle / Driver hit separation failed for box type "
                      << boxType << '\n';
            return 1;
        }
    }

    std::array<driver, 4> drivers;
    for (int index = 0; index < static_cast<int>(drivers.size()); ++index) {
        drivers[index].SetTitle(QStringLiteral("Driver %1").arg(index + 1));
    }

    preview.setDrivers(drivers.data(), static_cast<int>(drivers.size()));
    preview.resize(preview.sizeHint());
    QApplication::processEvents();

    const int driverCount = static_cast<int>(drivers.size());
    for (int driverIndex = 0; driverIndex < driverCount; ++driverIndex) {
        const int panelTop = allDriversPanelTop(preview, driverCount, driverIndex);
        lastBaffleHoverDriverIndex = -1;
        const QPoint allDriversWavePoint = findHoverPoint(
            preview,
            QRect(preview.width() - 90, panelTop + 95, 85, 115),
            driverIndex,
            lastBaffleHoverDriverIndex);

        if (allDriversWavePoint.x() < 0 || lastBaffleHoverDriverIndex != driverIndex) {
            std::cerr << "All-drivers wave hover selected wrong driver: expected "
                      << driverIndex << ", got " << lastBaffleHoverDriverIndex << '\n';
            return 1;
        }

        sendLeftPress(preview, allDriversWavePoint);
        if (lastBaffleClickDriverIndex != driverIndex) {
            std::cerr << "All-drivers wave click selected wrong driver: expected "
                      << driverIndex << ", got " << lastBaffleClickDriverIndex << '\n';
            return 1;
        }
    }

    std::cout << "circuitout baffle-parameters hit smoke test passed\n";
    return 0;
}
