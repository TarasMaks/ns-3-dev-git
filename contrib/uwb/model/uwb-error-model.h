/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_ERROR_MODEL_H
#define UWB_ERROR_MODEL_H

#include "uwb-utils.h"

#include "ns3/object.h"

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * The bit error rate of the HRP UWB physical layer, and the preamble acquisition that has to
 * succeed before any of it matters.
 *
 * A UWB receiver starts from a signal that is far below the noise floor: at the sensitivity of
 * a common transceiver the signal to noise ratio measured across the 499.2 MHz channel is about
 * -12 dB at 6.81 Mb/s and about -24 dB at 0.11 Mb/s. What makes the link work is processing
 * gain. The channel is thousands of times wider than the information rate, so the energy per
 * information bit over the noise density is
 *
 *     Eb/N0 = SINR * B / R,
 *
 * which for 6.81 Mb/s in a 499.2 MHz channel is 18.65 dB of gain. The model then uses the bit
 * error rate of coherent binary phase shift keying, which is the polarity of a burst position
 * modulation symbol,
 *
 *     BER = 0.5 erfc(sqrt(Eb/N0)),
 *
 * applied to an effective Eb/N0 that also carries a per rate term. That term is the net of two
 * real effects the model does not simulate individually: the gain of the concatenated
 * convolutional and Reed-Solomon code of Clause 15, and the implementation loss of a real
 * receiver.
 *
 * The per rate term is not guessed. It is solved for, at run time, from two published receiver
 * sensitivities (Qorvo DW1000: -105 dBm at 0.11 Mb/s and -93 dBm at 6.81 Mb/s, both at 1 %
 * packet error rate on a 127 octet frame) so that GetSensitivityDbm reproduces them exactly at
 * the reference noise figure. The two remaining rates are placed on the straight line those two
 * anchors define against the logarithm of the data rate, which yields -99 dBm at 0.85 Mb/s and
 * -89 dBm at 27.24 Mb/s.
 *
 * It is worth saying why the anchors slope the way they do. Dropping from 6.81 Mb/s to
 * 0.11 Mb/s is 18.1 dB of data rate but buys only 12 dB of sensitivity, so the long range mode
 * is about 6 dB less efficient per bit than the fast one. That is a property of real hardware,
 * not of the code: a 127 octet frame at 0.11 Mb/s lasts about ten milliseconds, over which
 * crystal drift and the receiver tracking loops cost more than the extra coding returns. A
 * model that assumed the long range mode was ideal would overstate UWB range by a factor of
 * two, so the loss is kept.
 *
 * Before any of this applies, the receiver has to find the signal. Acquisition correlates
 * against the preamble code, which accumulates the code length multiplied by the number of
 * preamble symbols, and the model requires the result to clear a threshold. At the preamble
 * lengths the datasheet quotes, 1024 symbols at 0.11 Mb/s and 128 at 6.81 Mb/s, acquisition has
 * several dB of margin and the payload decides the sensitivity, which is the regime real
 * products are designed for. Shorten the preamble and acquisition becomes the limit instead,
 * which is exactly why long range configurations use long preambles.
 */
class UwbErrorModel : public Object
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();

    UwbErrorModel();
    ~UwbErrorModel() override;

    /**
     * The energy per information bit over the noise density.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio measured over
     *             the whole channel bandwidth
     * @param config the link configuration
     * @return Eb/N0 in dB
     */
    double GetEbNoDb(double sinr, const UwbPhyConfig& config) const;

    /**
     * The bit error rate of the payload.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio measured over
     *             the whole channel bandwidth
     * @param config the link configuration
     * @return the bit error rate, between 0 and 0.5
     */
    double GetBitErrorRate(double sinr, const UwbPhyConfig& config) const;

    /**
     * The probability that a run of bits is received without any error.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio
     * @param config the link configuration
     * @param nbits the number of information bits
     * @return the probability that all the bits are correct
     */
    double GetChunkSuccessRate(double sinr, const UwbPhyConfig& config, uint64_t nbits) const;

    /**
     * The gain the preamble correlator accumulates, which is the code length multiplied by the
     * number of preamble symbols.
     *
     * @param config the link configuration
     * @return the gain, in dB
     */
    double GetAcquisitionGainDb(const UwbPhyConfig& config) const;

    /**
     * Whether the receiver can lock onto the preamble of a signal.
     *
     * @param sinr the signal to interference and noise ratio during the preamble, as a linear
     *             ratio measured over the whole channel bandwidth
     * @param config the link configuration
     * @return true if the correlator output clears the acquisition threshold
     */
    bool IsPreambleAcquired(double sinr, const UwbPhyConfig& config) const;

    /**
     * The received power at which a frame is decoded with the quoted packet error rate.
     *
     * @param config the link configuration
     * @param psduOctets the payload length, in octets
     * @param per the packet error rate the sensitivity is quoted at
     * @return the sensitivity, in dBm
     */
    double GetSensitivityDbm(const UwbPhyConfig& config,
                             uint32_t psduOctets = SENSITIVITY_PAYLOAD_OCTETS,
                             double per = SENSITIVITY_PER) const;

    /**
     * The received power at which the receiver can still acquire the preamble.
     *
     * @param config the link configuration
     * @return the power, in dBm
     */
    double GetAcquisitionThresholdDbm(const UwbPhyConfig& config) const;

    /**
     * The net of the coding gain of the physical layer and the implementation loss of the
     * receiver, solved from the published sensitivities.
     *
     * @param rate the data rate
     * @return the net gain, in dB, positive when the code wins
     */
    static double GetNetCodingGainDb(UwbDataRate rate);

    /**
     * The effective energy per bit over noise density a frame needs to be decoded with a given
     * packet error rate.
     *
     * @param nbits the number of information bits
     * @param per the packet error rate
     * @return the required Eb/N0, in dB, before the per rate term is applied
     */
    static double GetRequiredEffectiveEbNoDb(uint64_t nbits, double per);

    /**
     * The thermal noise power in a bandwidth.
     *
     * @param bandwidthHz the bandwidth, in Hz
     * @param noiseFigureDb the receiver noise figure, in dB
     * @return the noise power, in dBm
     */
    static double GetThermalNoiseDbm(double bandwidthHz, double noiseFigureDb);

    /// @return the noise figure of the receiver, in dB
    double GetNoiseFigureDb() const;

    /**
     * The rejection a receiver applies to a UWB signal that uses a different preamble code.
     *
     * @return the rejection, in dB
     */
    double GetCodeRejectionDb() const;

    /// Noise figure the published sensitivities are referred to, in dB.
    static constexpr double REFERENCE_NOISE_FIGURE_DB = 6.0;

  private:
    double m_noiseFigureDb;                //!< receiver noise figure, in dB
    double m_acquisitionThresholdDb;       //!< correlator output needed to lock, in dB
    double m_codeRejectionDb;              //!< rejection of a foreign preamble code, in dB
    double m_extraImplementationLossDb;    //!< loss added on top of the calibration, in dB
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_ERROR_MODEL_H */
