/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-spectrum-signal-parameters.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleSpectrumSignalParameters");

namespace ble
{

BleSpectrumSignalParameters::BleSpectrumSignalParameters()
{
    NS_LOG_FUNCTION(this);
}

BleSpectrumSignalParameters::BleSpectrumSignalParameters(const BleSpectrumSignalParameters& p)
    : SpectrumSignalParameters(p),
      mode(p.mode),
      channel(p.channel),
      accessAddress(p.accessAddress)
{
    NS_LOG_FUNCTION(this << &p);
    packet = p.packet ? p.packet->Copy() : nullptr;
}

Ptr<SpectrumSignalParameters>
BleSpectrumSignalParameters::Copy() const
{
    NS_LOG_FUNCTION(this);
    return Create<BleSpectrumSignalParameters>(*this);
}

} // namespace ble
} // namespace ns3
