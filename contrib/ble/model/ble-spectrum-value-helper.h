/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_SPECTRUM_VALUE_HELPER_H
#define BLE_SPECTRUM_VALUE_HELPER_H

#include "ble-constants.h"

#include "ns3/ptr.h"
#include "ns3/spectrum-value.h"

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * Build the power spectral densities of the BLE model.
 *
 * The spectrum model covers the 2.4 GHz ISM band with 1 MHz resolution, so that BLE signals
 * interfere correctly with the other 2.4 GHz technologies of ns-3 that use the spectrum
 * framework. The transmit density approximates the GFSK spectrum of the LE PHYs: the occupied
 * bandwidth is one symbol rate wide, so 1 MHz for LE 1M and the LE Coded PHYs and 2 MHz for
 * LE 2M.
 */
class BleSpectrumValueHelper
{
  public:
    /// @return the spectrum model shared by all BLE devices
    static Ptr<const SpectrumModel> GetSpectrumModel();

    /**
     * Build the transmit power spectral density of a BLE signal.
     *
     * @param txPowerDbm the total transmit power, in dBm
     * @param channel the logical channel index (0-39)
     * @param mode the PHY mode, which sets the occupied bandwidth
     * @return the transmit power spectral density, in W/Hz
     */
    static Ptr<SpectrumValue> CreateTxPowerSpectralDensity(double txPowerDbm,
                                                           uint8_t channel,
                                                           BlePhyMode mode);

    /**
     * Build the noise power spectral density of a receiver.
     *
     * @param noiseFigureDb the receiver noise figure, in dB
     * @return the noise power spectral density, in W/Hz
     */
    static Ptr<SpectrumValue> CreateNoisePowerSpectralDensity(double noiseFigureDb);

    /**
     * Build the filter that selects the band a receiver listens to, one for every 1 MHz bin
     * of the occupied bandwidth of the channel.
     *
     * @param channel the logical channel index (0-39)
     * @param mode the PHY mode, which sets the receiver bandwidth
     * @return a spectrum value that is one inside the band and zero outside
     */
    static Ptr<SpectrumValue> CreateRfFilter(uint8_t channel, BlePhyMode mode);

    /**
     * Integrate a power spectral density over the band a receiver listens to.
     *
     * @param psd the power spectral density, in W/Hz
     * @param channel the logical channel index (0-39)
     * @param mode the PHY mode, which sets the receiver bandwidth
     * @return the power in the band, in W
     */
    static double GetBandPower(Ptr<const SpectrumValue> psd, uint8_t channel, BlePhyMode mode);

    /**
     * @param mode the PHY mode
     * @return the noise bandwidth of a receiver, in Hz (one symbol rate)
     */
    static double GetNoiseBandwidthHz(BlePhyMode mode);
};

} // namespace ble
} // namespace ns3

#endif /* BLE_SPECTRUM_VALUE_HELPER_H */
