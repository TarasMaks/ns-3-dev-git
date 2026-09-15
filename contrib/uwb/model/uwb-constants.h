/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_CONSTANTS_H
#define UWB_CONSTANTS_H

#include "ns3/nstime.h"

#include <cstdint>
#include <ostream>
#include <string>

/**
 * @defgroup uwb Ultra-Wideband (UWB) models
 *
 * A custom Ultra-Wideband model for ns-3, covering the High Rate Pulse repetition frequency
 * (HRP) UWB physical layer of IEEE Std 802.15.4 and the ranging procedures of IEEE Std
 * 802.15.4z: single-sided and double-sided two-way ranging and time difference of arrival.
 *
 * ns-3 has no UWB physical layer and no notion of ranging, so both are built here on the
 * spectrum framework: the physical layer occupies a real 499.2 MHz channel, obeys the
 * -41.3 dBm/MHz regulatory limit that defines UWB, timestamps arrivals with the picosecond
 * resolution real radios provide, and every device carries its own crystal with a frequency
 * offset, which is what makes the difference between the ranging schemes visible.
 */

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * Nominal data rates of the HRP UWB physical layer (IEEE Std 802.15.4, Clause 15).
 *
 * A data rate selects the duration of a burst position modulation symbol. The nominal names
 * already account for the Reed-Solomon outer code, so the information rate of the 6.81 Mb/s
 * mode is the symbol rate multiplied by 55/63.
 */
enum class UwbDataRate : uint8_t
{
    RATE_110K = 0,  //!< 0.11 Mb/s, the long range mode
    RATE_850K,      //!< 0.85 Mb/s
    RATE_6M81,      //!< 6.81 Mb/s, the mode most products use
    RATE_27M24,     //!< 27.24 Mb/s, added by IEEE Std 802.15.4z
    DATA_RATE_COUNT
};

/**
 * @ingroup uwb
 * Mean pulse repetition frequency of the preamble (IEEE Std 802.15.4, Clause 15.2).
 */
enum class UwbPrf : uint8_t
{
    PRF_16M = 0, //!< 15.6 MHz mean pulse repetition frequency, preamble codes of length 31
    PRF_64M,     //!< 62.4 MHz mean pulse repetition frequency, preamble codes of length 127
    PRF_COUNT
};

/**
 * @ingroup uwb
 * Ranging procedures of IEEE Std 802.15.4z.
 */
enum class UwbRangingMethod : uint8_t
{
    SS_TWR = 0, //!< single-sided two-way ranging, two messages
    DS_TWR,     //!< double-sided two-way ranging, three messages
    TDOA,       //!< time difference of arrival, one message and synchronised anchors
};

/// @ingroup uwb
/// @param rate a data rate
/// @return a printable name
const char* UwbDataRateName(UwbDataRate rate);

/// @ingroup uwb
/// @param name "110k", "850k", "6M81" or "27M24"
/// @return the data rate
UwbDataRate UwbDataRateFromString(const std::string& name);

/// @ingroup uwb
/// @param method a ranging method
/// @return a printable name
const char* UwbRangingMethodName(UwbRangingMethod method);

/// @ingroup uwb
/// @param name "SS-TWR", "DS-TWR" or "TDOA"
/// @return the ranging method
UwbRangingMethod UwbRangingMethodFromString(const std::string& name);

/// @ingroup uwb
/// @param os the stream
/// @param rate a data rate
/// @return the stream
std::ostream& operator<<(std::ostream& os, UwbDataRate rate);

/// @ingroup uwb
/// @param os the stream
/// @param prf a mean pulse repetition frequency
/// @return the stream
std::ostream& operator<<(std::ostream& os, UwbPrf prf);

/// @ingroup uwb
/// @param os the stream
/// @param method a ranging method
/// @return the stream
std::ostream& operator<<(std::ostream& os, UwbRangingMethod method);

/// Chip rate of the HRP UWB physical layer, in Hz. Every UWB timing derives from it.
constexpr double CHIP_RATE_HZ = 499.2e6;
/// Duration of one chip, in seconds.
constexpr double CHIP_DURATION_S = 1.0 / CHIP_RATE_HZ;

/**
 * Resolution of the arrival timestamps a receiver produces, in seconds. Real radios count in
 * units of one chip divided by 128, which is about 15.65 ps, or a little under five millimetres
 * of propagation.
 */
constexpr double TIMESTAMP_RESOLUTION_S = CHIP_DURATION_S / 128.0;

/**
 * Rate of the device time counter, in Hz. Real radios run it at 128 times the chip rate,
 * 63.8976 GHz, and every timestamp they report is a whole number of its ticks.
 */
constexpr double DTU_RATE_HZ = CHIP_RATE_HZ * 128.0;

/**
 * Modulus of the device time counter. Real radios count in 40 bits, which wraps about every
 * 17.2 seconds, and ranging firmware has to subtract timestamps modulo this value. The model
 * keeps the wrap so that code written against it stays correct on hardware.
 */
constexpr uint64_t DTU_COUNTER_MODULUS = 1ULL << 40;

/// Speed of light in vacuum, in metres per second.
constexpr double SPEED_OF_LIGHT = 299792458.0;

/// Lowest channel index this model supports.
constexpr uint8_t FIRST_CHANNEL = 1;
/// Highest channel index this model supports.
constexpr uint8_t LAST_CHANNEL = 15;
/// Nominal bandwidth of most UWB channels, in MHz.
constexpr double NOMINAL_BANDWIDTH_MHZ = 499.2;

/**
 * Regulatory limit on the mean equivalent isotropically radiated power density, in dBm per
 * megahertz. Both the FCC and ETSI hold UWB to this figure, and it is the reason a technology
 * with half a gigahertz of bandwidth is nevertheless short range.
 */
constexpr double REGULATORY_EIRP_DBM_PER_MHZ = -41.3;

/// Duration of a preamble symbol at a mean pulse repetition frequency of 15.6 MHz, in seconds.
constexpr double PREAMBLE_SYMBOL_16M_S = 31 * 16 * CHIP_DURATION_S;
/// Duration of a preamble symbol at a mean pulse repetition frequency of 62.4 MHz, in seconds.
constexpr double PREAMBLE_SYMBOL_64M_S = 127 * 4 * CHIP_DURATION_S;

/// Number of information bits of the physical layer header, including its parity bits.
constexpr uint32_t PHR_BITS = 19;
/// Information bits of a Reed-Solomon block.
constexpr uint32_t RS_BLOCK_INFORMATION_BITS = 330;
/// Parity bits a Reed-Solomon block adds.
constexpr uint32_t RS_BLOCK_PARITY_BITS = 48;
/// Octets of the frame check sequence.
constexpr uint32_t FCS_OCTETS = 2;
/// Largest payload of a frame, in octets.
constexpr uint32_t MAX_PSDU_OCTETS = 127;
/// Largest payload of a frame when the extended length of IEEE Std 802.15.4z is used.
constexpr uint32_t MAX_PSDU_OCTETS_EXTENDED = 1023;

/// Number of symbols of the start of frame delimiter in its short form.
constexpr uint32_t SFD_SYMBOLS_SHORT = 8;
/// Number of symbols of the start of frame delimiter in its long form, used at 0.11 Mb/s.
constexpr uint32_t SFD_SYMBOLS_LONG = 64;

/**
 * Receiver sensitivities published for a widely used UWB transceiver, in dBm, against which the
 * error model of this module is calibrated (Qorvo DW1000 datasheet, 0.11 Mb/s with a 1024 symbol
 * preamble and 6.81 Mb/s with a 128 symbol preamble).
 */
constexpr double PUBLISHED_SENSITIVITY_110K_DBM = -105.0;
constexpr double PUBLISHED_SENSITIVITY_6M81_DBM = -93.0;

/// Packet error rate at which a receiver sensitivity is quoted.
constexpr double SENSITIVITY_PER = 0.01;
/// Payload, in octets, of the frame a sensitivity is quoted for.
constexpr uint32_t SENSITIVITY_PAYLOAD_OCTETS = 127;

/**
 * @ingroup uwb
 * @param channel a channel index (1 to 15)
 * @return the centre frequency, in MHz
 */
double ChannelToFrequencyMhz(uint8_t channel);

/**
 * @ingroup uwb
 * @param channel a channel index (1 to 15)
 * @return the bandwidth of the channel, in MHz
 */
double ChannelToBandwidthMhz(uint8_t channel);

/**
 * @ingroup uwb
 * @param channel a channel index
 * @return true if the channel is one of the three the standard makes mandatory
 */
bool IsMandatoryChannel(uint8_t channel);

/**
 * @ingroup uwb
 * @param rate a data rate
 * @return the duration of one burst position modulation symbol, in seconds
 */
double GetSymbolDurationS(UwbDataRate rate);

/**
 * @ingroup uwb
 * @param rate a data rate
 * @return the information bit rate, in bits per second
 */
double GetBitRate(UwbDataRate rate);

/**
 * @ingroup uwb
 * @param prf a mean pulse repetition frequency
 * @return the duration of one preamble symbol, in seconds
 */
double GetPreambleSymbolDurationS(UwbPrf prf);

/**
 * @ingroup uwb
 * The data rate the physical layer header is sent at, which is 0.11 Mb/s in the long range mode
 * and 0.85 Mb/s otherwise.
 *
 * @param rate the data rate of the payload
 * @return the data rate of the header
 */
UwbDataRate GetPhrDataRate(UwbDataRate rate);

/**
 * @ingroup uwb
 * @param prf a mean pulse repetition frequency
 * @return a printable name
 */
const char* UwbPrfName(UwbPrf prf);

} // namespace uwb
} // namespace ns3

#endif /* UWB_CONSTANTS_H */
