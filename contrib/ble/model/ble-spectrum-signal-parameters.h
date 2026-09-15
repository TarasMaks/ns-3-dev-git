/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_SPECTRUM_SIGNAL_PARAMETERS_H
#define BLE_SPECTRUM_SIGNAL_PARAMETERS_H

#include "ble-constants.h"

#include "ns3/packet.h"
#include "ns3/spectrum-signal-parameters.h"

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * The parameters of a BLE signal travelling over a spectrum channel: the packet it carries,
 * the PHY mode it is modulated with and the channel it occupies.
 */
struct BleSpectrumSignalParameters : public SpectrumSignalParameters
{
    Ptr<SpectrumSignalParameters> Copy() const override;

    BleSpectrumSignalParameters();

    /**
     * Copy constructor.
     * @param p the parameters to copy
     */
    BleSpectrumSignalParameters(const BleSpectrumSignalParameters& p);

    Ptr<Packet> packet;                         //!< the packet being transmitted
    BlePhyMode mode{BlePhyMode::LE_1M};         //!< the PHY mode
    uint8_t channel{ADV_CHANNEL_37};            //!< the logical channel index
    uint32_t accessAddress{ADV_ACCESS_ADDRESS}; //!< the access address of the packet
};

} // namespace ble
} // namespace ns3

#endif /* BLE_SPECTRUM_SIGNAL_PARAMETERS_H */
