/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_UTILS_H
#define BLE_UTILS_H

#include "ble-constants.h"

#include "ns3/random-variable-stream.h"

#include <cstdint>
#include <vector>

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * Compute the 24-bit CRC of Bluetooth Core Specification 5.x, Vol 6, Part B, Section 3.1.1.
 *
 * The linear feedback shift register of the specification is implemented directly: the bit
 * shifted out of position 23 is exclusive-ORed with the incoming data bit and the result is fed
 * back into the positions selected by the polynomial x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1.
 * Data bits enter least significant bit first, as they are transmitted.
 *
 * @param crcInit the initial value of the register (24 bits)
 * @param data the octets to compute the CRC over
 * @param length the number of octets
 * @return the 24-bit CRC
 */
uint32_t Crc24(uint32_t crcInit, const uint8_t* data, std::size_t length);

/**
 * @ingroup ble
 * @param crcInit the initial value of the register (24 bits)
 * @param data the octets to compute the CRC over
 * @return the 24-bit CRC
 */
uint32_t Crc24(uint32_t crcInit, const std::vector<uint8_t>& data);

/**
 * @ingroup ble
 * Apply the data whitening of Vol 6, Part B, Section 3.2 in place.
 *
 * Whitening exclusive-ORs the data with the output of a 7-bit linear feedback shift register
 * with the polynomial x^7 + x^4 + 1, seeded with the channel index. The operation is its own
 * inverse, so the same function de-whitens.
 *
 * @param channel the logical channel index (0-39) used to seed the register
 * @param data the octets to whiten, modified in place
 * @param length the number of octets
 */
void Whiten(uint8_t channel, uint8_t* data, std::size_t length);

/**
 * @ingroup ble
 * @param channel the logical channel index (0-39) used to seed the register
 * @param data the octets to whiten, modified in place
 */
void Whiten(uint8_t channel, std::vector<uint8_t>& data);

/**
 * @ingroup ble
 * The channel map of a connection: which of the 37 data channels may be used.
 */
class BleChannelMap
{
  public:
    /// Create a map with all 37 data channels enabled.
    BleChannelMap();

    /**
     * Create a map from the 5-octet representation used on the air (bit i of octet j is
     * data channel 8*j + i).
     * @param octets the five octets of the ChM field
     */
    explicit BleChannelMap(const uint8_t octets[5]);

    /**
     * @param channel a data channel index (0-36)
     * @param used whether the channel may be used
     */
    void SetUsed(uint8_t channel, bool used);

    /**
     * @param channel a data channel index (0-36)
     * @return whether the channel may be used
     */
    bool IsUsed(uint8_t channel) const;

    /// @return the number of used channels
    uint8_t GetNUsedChannels() const;

    /**
     * @param index a position in the ordered list of used channels
     * @return the corresponding data channel index
     */
    uint8_t GetUsedChannel(uint8_t index) const;

    /**
     * Write the 5-octet representation used on the air.
     * @param octets the destination buffer
     */
    void Serialize(uint8_t octets[5]) const;

    /// @return the indices of the used channels, in increasing order
    std::vector<uint8_t> GetUsedChannels() const;

  private:
    /// Recompute the ordered list of used channels.
    void Rebuild();

    bool m_used[N_DATA_CHANNELS];    //!< whether each data channel is used
    std::vector<uint8_t> m_usedList; //!< the used channels, in increasing order
};

/**
 * @ingroup ble
 * Channel selection algorithm #1 (Vol 6, Part B, Section 4.5.8.2).
 *
 * The unmapped channel advances by a fixed hop increment modulo 37 at every connection event;
 * if it is not in the channel map it is remapped into the list of used channels.
 */
class BleChannelSelectionAlgorithm1
{
  public:
    /**
     * @param hopIncrement the hop increment (5 to 16)
     */
    explicit BleChannelSelectionAlgorithm1(uint8_t hopIncrement = HOP_INCREMENT_MIN);

    /**
     * Advance to the next connection event and return the channel to use.
     * @param map the channel map of the connection
     * @return the data channel index (0-36)
     */
    uint8_t NextChannel(const BleChannelMap& map);

    /// @return the current unmapped channel
    uint8_t GetLastUnmappedChannel() const;

    /**
     * Reset the unmapped channel, used when a connection starts.
     * @param lastUnmappedChannel the value to restart from
     */
    void Reset(uint8_t lastUnmappedChannel = 0);

  private:
    uint8_t m_hopIncrement;        //!< hop increment
    uint8_t m_lastUnmappedChannel; //!< unmapped channel of the previous event
};

/**
 * @ingroup ble
 * Channel selection algorithm #2 (Vol 6, Part B, Section 4.5.8.3).
 *
 * The channel of an event is derived from a pseudo-random value computed from the channel
 * identifier of the connection and the event counter, so consecutive events are uncorrelated.
 * The pseudo-random generator applies three rounds of the specification's bit permutation and
 * "multiply, add and modulo" operations.
 */
class BleChannelSelectionAlgorithm2
{
  public:
    /**
     * @param accessAddress the access address of the connection, from which the channel
     *                      identifier is derived
     */
    explicit BleChannelSelectionAlgorithm2(uint32_t accessAddress = 0);

    /**
     * @param accessAddress the access address of the connection
     */
    void SetAccessAddress(uint32_t accessAddress);

    /// @return the channel identifier derived from the access address
    uint16_t GetChannelIdentifier() const;

    /**
     * @param map the channel map of the connection
     * @param eventCounter the connection event counter
     * @return the data channel index (0-36)
     */
    uint8_t GetChannel(const BleChannelMap& map, uint16_t eventCounter) const;

    /**
     * The pseudo-random value of an event, exposed for testing.
     * @param eventCounter the connection event counter
     * @return the 16-bit pseudo-random value
     */
    uint16_t GetPseudoRandomNumber(uint16_t eventCounter) const;

  private:
    uint16_t m_channelIdentifier; //!< channel identifier
};

/**
 * @ingroup ble
 * Check the constraints that an access address must satisfy (Vol 6, Part B, Section 2.1.2):
 * it differs from the advertising access address and from any address that differs from it in
 * only one bit, it has no more than six consecutive equal bits, its four octets are not all
 * equal, it has no more than 24 transitions and at least two transitions in its six most
 * significant bits.
 *
 * @param accessAddress the candidate access address
 * @return true if all the constraints are met
 */
bool IsValidAccessAddress(uint32_t accessAddress);

/**
 * @ingroup ble
 * Draw an access address that satisfies IsValidAccessAddress().
 *
 * @param random a uniform random variable stream used to draw candidates
 * @return a valid access address
 */
uint32_t GenerateAccessAddress(Ptr<UniformRandomVariable> random);

} // namespace ble
} // namespace ns3

#endif /* BLE_UTILS_H */
