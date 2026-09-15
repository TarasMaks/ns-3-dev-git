/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_SPECTRUM_VALUE_HELPER_H
#define UWB_SPECTRUM_VALUE_HELPER_H

#include "uwb-constants.h"

#include "ns3/ptr.h"
#include "ns3/spectrum-value.h"

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * Build the power spectral densities of the UWB model.
 *
 * The spectrum model covers 3.1 GHz to 10.7 GHz, the band regulators opened to ultra-wideband
 * devices, with a resolution of 99.84 MHz. That figure is a fifth of the 499.2 MHz channel
 * bandwidth and every UWB centre frequency is an integer multiple of 499.2 MHz, so every
 * channel is an exact whole number of bins wide and lands exactly on the grid; no channel edge
 * ever has to be approximated.
 *
 * The transmit density is flat across the channel, which is how regulators define the limit a
 * UWB emission has to respect, with a skirt on each side for the out of band emission that a
 * real pulse shape leaves behind. Because a UWB signal is half a gigahertz wide, the density is
 * tiny: the -41.3 dBm/MHz limit puts a total of -14.3 dBm into a 499.2 MHz channel, some 300
 * times less power than a Wi-Fi transmitter radiates. Spreading it over a real band, rather
 * than treating the signal as a point in frequency, is what lets this module share a
 * MultiModelSpectrumChannel with the 5 GHz and 6 GHz Wi-Fi models and interfere with them
 * correctly.
 */
class UwbSpectrumValueHelper
{
  public:
    /// @return the spectrum model shared by all UWB devices
    static Ptr<const SpectrumModel> GetSpectrumModel();

    /**
     * Build the transmit power spectral density of a UWB signal.
     *
     * @param txPowerDbm the total transmit power, in dBm
     * @param channel the channel index (1 to 15)
     * @return the transmit power spectral density, in W/Hz
     */
    static Ptr<SpectrumValue> CreateTxPowerSpectralDensity(double txPowerDbm, uint8_t channel);

    /**
     * Build the noise power spectral density of a receiver.
     *
     * @param noiseFigureDb the receiver noise figure, in dB
     * @return the noise power spectral density, in W/Hz
     */
    static Ptr<SpectrumValue> CreateNoisePowerSpectralDensity(double noiseFigureDb);

    /**
     * Build the filter that selects the band a receiver listens to.
     *
     * @param channel the channel index (1 to 15)
     * @return a spectrum value that is one inside the channel and zero outside
     */
    static Ptr<SpectrumValue> CreateRfFilter(uint8_t channel);

    /**
     * Integrate a power spectral density over the band a receiver listens to.
     *
     * @param psd the power spectral density, in W/Hz
     * @param channel the channel index (1 to 15)
     * @return the power in the band, in W
     */
    static double GetBandPower(Ptr<const SpectrumValue> psd, uint8_t channel);

    /**
     * The noise bandwidth of a receiver, which is the whole channel: a UWB receiver takes in
     * the entire 499.2 MHz and recovers the signal to noise ratio afterwards, in the
     * correlator, rather than by filtering.
     *
     * @param channel the channel index (1 to 15)
     * @return the bandwidth, in Hz
     */
    static double GetNoiseBandwidthHz(uint8_t channel);

    /**
     * The mean power density of a transmission, which is the figure the regulatory limit
     * applies to.
     *
     * @param txPowerDbm the total transmit power, in dBm
     * @param channel the channel index (1 to 15)
     * @return the density, in dBm per megahertz
     */
    static double GetPowerDensityDbmPerMhz(double txPowerDbm, uint8_t channel);

    /// @return the width of one bin of the spectrum model, in Hz
    static double GetBinWidthHz();

    /// @return the number of bins of the spectrum model
    static uint32_t GetNBins();
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_SPECTRUM_VALUE_HELPER_H */
