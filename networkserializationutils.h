/*
 * KFilter6
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2002-2026 Martin Erdtmann
 */

#ifndef NETWORKSERIALIZATIONUTILS_H
#define NETWORKSERIALIZATIONUTILS_H

#include "driver.h"

namespace NetworkSerializationUtils
{
constexpr int SectionCount = 8;
constexpr int ValuesPerSection = 6;
constexpr int ValueCount = SectionCount * ValuesPerSection;

struct Address
{
    int sectionIndex = 0;
    NetworkBranchType branch = NetworkBranchType::Series;
    NetworkComponent component = NetworkComponent::Resistance;
};

inline bool addressForIndex(int zeroBasedIndex, Address& address)
{
    if (zeroBasedIndex < 0 || zeroBasedIndex >= ValueCount) {
        return false;
    }

    const int row = zeroBasedIndex % ValuesPerSection;
    address.sectionIndex = zeroBasedIndex / ValuesPerSection;
    address.branch = row < 3 ? NetworkBranchType::Series : NetworkBranchType::Shunt;
    switch (row % 3) {
    case 0:
        address.component = NetworkComponent::Resistance;
        break;
    case 1:
        address.component = NetworkComponent::Capacitance;
        break;
    case 2:
        address.component = NetworkComponent::Inductance;
        break;
    }
    return true;
}

inline void setValue(driver& drv, int zeroBasedIndex, double value)
{
    Address address;
    if (!addressForIndex(zeroBasedIndex, address)) {
        return;
    }
    drv.setNetworkValue(address.sectionIndex, address.branch, address.component, value);
}

inline double value(const driver& drv, int zeroBasedIndex)
{
    Address address;
    if (!addressForIndex(zeroBasedIndex, address)) {
        return -1.0;
    }
    return drv.getNetworkValue(address.sectionIndex, address.branch, address.component);
}
}

#endif // NETWORKSERIALIZATIONUTILS_H
