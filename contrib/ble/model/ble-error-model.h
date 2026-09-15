/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_ERROR_MODEL_H
#define BLE_ERROR_MODEL_H

#include "ble-constants.h"

#include "ns3/object.h"

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * The bit error rate of the LE PHYs as a function of the signal to interference and noise ratio.
 *
 * The LE PHYs modulate with GFSK at a modulation index of about 0.5. The model takes the bit
 * error rate of non-coherent binary frequency shift keying,
 *
 *     BER = 0.5 exp(-0.5 Eb/N0),
 *
 * and derives the energy per information bit from the signal to noise ratio measured in the
 * receiver bandwidth, Eb/N0 = SINR * B / R, where B is one symbol rate and R is the information
 * bit rate. The coded PHYs therefore gain 3 dB (S=2) and 9 dB (S=8) from their lower information
 * rate; an implementation loss, 2 dB by default for S=8, accounts for the pattern mapper of the
 * highest spreading factor being less efficient than an ideal code of the same rate.
 *
 * With the default noise figure of 8 dB this yields receiver sensitivities of about -95, -92,
 * -98 and -102 dBm for LE 1M, LE 2M, LE Coded S=2 and LE Coded S=8, each within about 1 dB of
 * the figures published for a common BLE radio (Nordic nRF52840: -96, -93, -99 and -103 dBm)
 * and far above the -70 dBm minimum that the Bluetooth Core Specification requires. The test
 * suite checks both of these properties.
 */
class BleErrorModel : public Object
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();

    BleErrorModel();
    ~BleErrorModel() override;

    /**
     * The bit error rate of a PHY mode at a given signal to interference and noise ratio.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio measured in the
     *             receiver bandwidth of the mode
     * @param mode the PHY mode
     * @return the bit error rate, between 0 and 0.5
     */
    double GetBitErrorRate(double sinr, BlePhyMode mode) const;

    /**
     * The probability that a run of bits is received without any error.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio
     * @param mode the PHY mode
     * @param nbits the number of information bits
     * @return the probability that all the bits are correct
     */
    double GetChunkSuccessRate(double sinr, BlePhyMode mode, uint64_t nbits) const;

    /**
     * The energy per information bit over the noise density.
     *
     * @param sinr the signal to interference and noise ratio, as a linear ratio
     * @param mode the PHY mode
     * @return Eb/N0 as a linear ratio
     */
    double GetEbNo(double sinr, BlePhyMode mode) const;

    /**
     * The implementation loss applied to a PHY mode, in dB.
     *
     * @param mode the PHY mode
     * @return the loss in dB
     */
    double GetImplementationLossDb(BlePhyMode mode) const;

  private:
    double m_implementationLossDb;        //!< loss applied to every mode
    double m_codedS2ImplementationLossDb; //!< additional loss of the LE Coded S=2 PHY
    double m_codedS8ImplementationLossDb; //!< additional loss of the LE Coded S=8 PHY
};

} // namespace ble
} // namespace ns3

#endif /* BLE_ERROR_MODEL_H */
