/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_SPECTRUM_SIGNAL_PARAMETERS_H
#define UWB_SPECTRUM_SIGNAL_PARAMETERS_H

#include "uwb-utils.h"

#include "ns3/packet.h"
#include "ns3/spectrum-signal-parameters.h"

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * The parameters of a UWB signal travelling over a spectrum channel.
 *
 * Besides the packet and the link configuration, the parameters carry the preamble code, which
 * decides how well two overlapping UWB signals reject one another, and the duration of the
 * synchronisation header, which is what separates the start of the signal from the marker that
 * ranging timestamps refer to.
 */
struct UwbSpectrumSignalParameters : public SpectrumSignalParameters
{
    Ptr<SpectrumSignalParameters> Copy() const override;

    UwbSpectrumSignalParameters();

    /**
     * Copy constructor.
     * @param p the parameters to copy
     */
    UwbSpectrumSignalParameters(const UwbSpectrumSignalParameters& p);

    Ptr<Packet> packet;   //!< the packet being transmitted
    UwbPhyConfig config;  //!< the channel, data rate, pulse repetition frequency and preamble
    uint8_t preambleCode; //!< the preamble code index (1 to 24)
    Time shrDuration;     //!< the duration of the preamble and start of frame delimiter
    bool ranging;         //!< true if the receiver should timestamp the arrival
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_SPECTRUM_SIGNAL_PARAMETERS_H */
