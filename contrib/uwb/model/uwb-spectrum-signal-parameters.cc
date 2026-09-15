/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-spectrum-signal-parameters.h"

#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbSpectrumSignalParameters");

namespace uwb
{

UwbSpectrumSignalParameters::UwbSpectrumSignalParameters()
    : preambleCode(9),
      shrDuration(Time(0)),
      ranging(false)
{
    NS_LOG_FUNCTION(this);
}

UwbSpectrumSignalParameters::UwbSpectrumSignalParameters(const UwbSpectrumSignalParameters& p)
    : SpectrumSignalParameters(p),
      config(p.config),
      preambleCode(p.preambleCode),
      shrDuration(p.shrDuration),
      ranging(p.ranging)
{
    NS_LOG_FUNCTION(this << &p);
    packet = p.packet ? p.packet->Copy() : nullptr;
}

Ptr<SpectrumSignalParameters>
UwbSpectrumSignalParameters::Copy() const
{
    return Create<UwbSpectrumSignalParameters>(*this);
}

} // namespace uwb
} // namespace ns3
