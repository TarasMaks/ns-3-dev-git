/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-utils.h"

#include "ns3/abort.h"

#include <algorithm>

namespace ns3
{
namespace ble
{

namespace
{
/// Feedback taps of the CRC polynomial x^24 + x^10 + x^9 + x^6 + x^4 + x^3 + x + 1.
constexpr uint32_t CRC_POLYNOMIAL = 0x00065B;
/// Mask of the 24-bit CRC register.
constexpr uint32_t CRC_MASK = 0xFFFFFF;
/// Feedback taps of the whitening polynomial x^7 + x^4 + 1.
constexpr uint8_t WHITENING_POLYNOMIAL = 0x44;
} // namespace

uint32_t
Crc24(uint32_t crcInit, const uint8_t* data, std::size_t length)
{
    uint32_t lfsr = crcInit & CRC_MASK;
    for (std::size_t i = 0; i < length; ++i)
    {
        const uint8_t octet = data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            const uint32_t feedback = ((lfsr >> 23) & 0x01) ^ ((octet >> bit) & 0x01);
            lfsr = (lfsr << 1) & CRC_MASK;
            if (feedback)
            {
                lfsr ^= CRC_POLYNOMIAL;
            }
        }
    }
    return lfsr;
}

uint32_t
Crc24(uint32_t crcInit, const std::vector<uint8_t>& data)
{
    return Crc24(crcInit, data.data(), data.size());
}

void
Whiten(uint8_t channel, uint8_t* data, std::size_t length)
{
    // position 6 of the register is preset to one, positions 5 down to 0 hold the channel index
    uint8_t lfsr = 0x40 | (channel & 0x3F);
    for (std::size_t i = 0; i < length; ++i)
    {
        uint8_t octet = data[i];
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            const uint8_t output = lfsr & 0x01;
            lfsr >>= 1;
            if (output)
            {
                lfsr ^= WHITENING_POLYNOMIAL;
            }
            octet ^= static_cast<uint8_t>(output << bit);
        }
        data[i] = octet;
    }
}

void
Whiten(uint8_t channel, std::vector<uint8_t>& data)
{
    Whiten(channel, data.data(), data.size());
}

/* -------------------------------------------------------------------------------------------- */
/* Channel map                                                                                    */
/* -------------------------------------------------------------------------------------------- */

BleChannelMap::BleChannelMap()
{
    std::fill(std::begin(m_used), std::end(m_used), true);
    Rebuild();
}

BleChannelMap::BleChannelMap(const uint8_t octets[5])
{
    for (uint8_t channel = 0; channel < N_DATA_CHANNELS; ++channel)
    {
        m_used[channel] = ((octets[channel / 8] >> (channel % 8)) & 0x01) != 0;
    }
    Rebuild();
}

void
BleChannelMap::SetUsed(uint8_t channel, bool used)
{
    NS_ABORT_MSG_IF(channel >= N_DATA_CHANNELS, "Invalid data channel " << +channel);
    m_used[channel] = used;
    Rebuild();
}

bool
BleChannelMap::IsUsed(uint8_t channel) const
{
    NS_ABORT_MSG_IF(channel >= N_DATA_CHANNELS, "Invalid data channel " << +channel);
    return m_used[channel];
}

uint8_t
BleChannelMap::GetNUsedChannels() const
{
    return static_cast<uint8_t>(m_usedList.size());
}

uint8_t
BleChannelMap::GetUsedChannel(uint8_t index) const
{
    NS_ABORT_MSG_IF(m_usedList.empty(), "The channel map has no used channel");
    return m_usedList.at(index % m_usedList.size());
}

void
BleChannelMap::Serialize(uint8_t octets[5]) const
{
    std::fill(octets, octets + 5, 0);
    for (uint8_t channel = 0; channel < N_DATA_CHANNELS; ++channel)
    {
        if (m_used[channel])
        {
            octets[channel / 8] |= static_cast<uint8_t>(1U << (channel % 8));
        }
    }
}

std::vector<uint8_t>
BleChannelMap::GetUsedChannels() const
{
    return m_usedList;
}

void
BleChannelMap::Rebuild()
{
    m_usedList.clear();
    for (uint8_t channel = 0; channel < N_DATA_CHANNELS; ++channel)
    {
        if (m_used[channel])
        {
            m_usedList.push_back(channel);
        }
    }
}

/* -------------------------------------------------------------------------------------------- */
/* Channel selection algorithm #1                                                                 */
/* -------------------------------------------------------------------------------------------- */

BleChannelSelectionAlgorithm1::BleChannelSelectionAlgorithm1(uint8_t hopIncrement)
    : m_hopIncrement(hopIncrement),
      m_lastUnmappedChannel(0)
{
    NS_ABORT_MSG_IF(hopIncrement < HOP_INCREMENT_MIN || hopIncrement > HOP_INCREMENT_MAX,
                    "Invalid hop increment " << +hopIncrement);
}

uint8_t
BleChannelSelectionAlgorithm1::NextChannel(const BleChannelMap& map)
{
    m_lastUnmappedChannel =
        static_cast<uint8_t>((m_lastUnmappedChannel + m_hopIncrement) % N_DATA_CHANNELS);
    if (map.IsUsed(m_lastUnmappedChannel))
    {
        return m_lastUnmappedChannel;
    }
    const uint8_t remappingIndex =
        static_cast<uint8_t>(m_lastUnmappedChannel % map.GetNUsedChannels());
    return map.GetUsedChannel(remappingIndex);
}

uint8_t
BleChannelSelectionAlgorithm1::GetLastUnmappedChannel() const
{
    return m_lastUnmappedChannel;
}

void
BleChannelSelectionAlgorithm1::Reset(uint8_t lastUnmappedChannel)
{
    m_lastUnmappedChannel = lastUnmappedChannel % N_DATA_CHANNELS;
}

/* -------------------------------------------------------------------------------------------- */
/* Channel selection algorithm #2                                                                 */
/* -------------------------------------------------------------------------------------------- */

namespace
{

/**
 * Reverse the order of the bits of each octet of a 16-bit word, the permutation used by the
 * pseudo-random generator of channel selection algorithm #2.
 * @param value the input
 * @return the permuted value
 */
uint16_t
PermuteBits(uint16_t value)
{
    auto reverse = [](uint8_t octet) {
        uint8_t result = 0;
        for (uint8_t bit = 0; bit < 8; ++bit)
        {
            if ((octet >> bit) & 0x01)
            {
                result |= static_cast<uint8_t>(1U << (7 - bit));
            }
        }
        return result;
    };
    const uint8_t low = reverse(static_cast<uint8_t>(value & 0xFF));
    const uint8_t high = reverse(static_cast<uint8_t>(value >> 8));
    return static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | low);
}

/**
 * The "multiply, add and modulo" operation of channel selection algorithm #2.
 * @param a the value to transform
 * @param b the channel identifier
 * @return (17 * a + b) modulo 2^16
 */
uint16_t
MultiplyAddModulo(uint16_t a, uint16_t b)
{
    return static_cast<uint16_t>((static_cast<uint32_t>(a) * 17 + b) & 0xFFFF);
}

} // namespace

BleChannelSelectionAlgorithm2::BleChannelSelectionAlgorithm2(uint32_t accessAddress)
{
    SetAccessAddress(accessAddress);
}

void
BleChannelSelectionAlgorithm2::SetAccessAddress(uint32_t accessAddress)
{
    m_channelIdentifier =
        static_cast<uint16_t>(((accessAddress >> 16) & 0xFFFF) ^ (accessAddress & 0xFFFF));
}

uint16_t
BleChannelSelectionAlgorithm2::GetChannelIdentifier() const
{
    return m_channelIdentifier;
}

uint16_t
BleChannelSelectionAlgorithm2::GetPseudoRandomNumber(uint16_t eventCounter) const
{
    uint16_t prn = static_cast<uint16_t>(eventCounter ^ m_channelIdentifier);
    for (uint8_t round = 0; round < 3; ++round)
    {
        prn = PermuteBits(prn);
        prn = MultiplyAddModulo(prn, m_channelIdentifier);
    }
    return static_cast<uint16_t>(prn ^ m_channelIdentifier);
}

uint8_t
BleChannelSelectionAlgorithm2::GetChannel(const BleChannelMap& map, uint16_t eventCounter) const
{
    const uint16_t prn = GetPseudoRandomNumber(eventCounter);
    const uint8_t unmappedChannel = static_cast<uint8_t>(prn % N_DATA_CHANNELS);
    if (map.IsUsed(unmappedChannel))
    {
        return unmappedChannel;
    }
    const uint8_t remappingIndex =
        static_cast<uint8_t>((static_cast<uint32_t>(map.GetNUsedChannels()) * prn) >> 16);
    return map.GetUsedChannel(remappingIndex);
}

/* -------------------------------------------------------------------------------------------- */
/* Access addresses                                                                               */
/* -------------------------------------------------------------------------------------------- */

namespace
{

/**
 * @param accessAddress an access address
 * @return the number of bit transitions in its 32 bits
 */
uint8_t
CountTransitions(uint32_t accessAddress)
{
    uint8_t transitions = 0;
    for (uint8_t bit = 1; bit < 32; ++bit)
    {
        if (((accessAddress >> bit) & 0x01) != ((accessAddress >> (bit - 1)) & 0x01))
        {
            ++transitions;
        }
    }
    return transitions;
}

/**
 * @param accessAddress an access address
 * @return the longest run of equal consecutive bits
 */
uint8_t
LongestRun(uint32_t accessAddress)
{
    uint8_t longest = 1;
    uint8_t current = 1;
    for (uint8_t bit = 1; bit < 32; ++bit)
    {
        if (((accessAddress >> bit) & 0x01) == ((accessAddress >> (bit - 1)) & 0x01))
        {
            ++current;
            longest = std::max(longest, current);
        }
        else
        {
            current = 1;
        }
    }
    return longest;
}

} // namespace

bool
IsValidAccessAddress(uint32_t accessAddress)
{
    // it shall differ from the advertising access address in at least two bits
    uint32_t difference = accessAddress ^ ADV_ACCESS_ADDRESS;
    uint8_t differingBits = 0;
    while (difference)
    {
        differingBits += difference & 0x01;
        difference >>= 1;
    }
    if (differingBits < 2)
    {
        return false;
    }
    // its four octets shall not be all equal
    const uint8_t octet0 = accessAddress & 0xFF;
    if (((accessAddress >> 8) & 0xFF) == octet0 && ((accessAddress >> 16) & 0xFF) == octet0 &&
        ((accessAddress >> 24) & 0xFF) == octet0)
    {
        return false;
    }
    // it shall have no more than six consecutive equal bits
    if (LongestRun(accessAddress) > 6)
    {
        return false;
    }
    // it shall have no more than 24 transitions
    if (CountTransitions(accessAddress) > 24)
    {
        return false;
    }
    // it shall have at least two transitions in its six most significant bits
    uint8_t highTransitions = 0;
    for (uint8_t bit = 27; bit <= 30; ++bit)
    {
        if (((accessAddress >> bit) & 0x01) != ((accessAddress >> (bit + 1)) & 0x01))
        {
            ++highTransitions;
        }
    }
    return highTransitions >= 2;
}

uint32_t
GenerateAccessAddress(Ptr<UniformRandomVariable> random)
{
    NS_ABORT_MSG_IF(!random, "A random variable stream is required");
    for (uint32_t attempt = 0; attempt < 10000; ++attempt)
    {
        const uint32_t candidate = static_cast<uint32_t>(random->GetInteger(0, 0xFFFFFFFF));
        if (IsValidAccessAddress(candidate))
        {
            return candidate;
        }
    }
    NS_ABORT_MSG("Could not draw a valid access address");
    return 0;
}

/* -------------------------------------------------------------------------------------------- */
/* Constants helpers                                                                              */
/* -------------------------------------------------------------------------------------------- */

const char*
BlePhyModeName(BlePhyMode mode)
{
    switch (mode)
    {
    case BlePhyMode::LE_1M:
        return "LE_1M";
    case BlePhyMode::LE_2M:
        return "LE_2M";
    case BlePhyMode::LE_CODED_S2:
        return "LE_CODED_S2";
    case BlePhyMode::LE_CODED_S8:
        return "LE_CODED_S8";
    default:
        return "invalid";
    }
}

BlePhyMode
BlePhyModeFromString(const std::string& name)
{
    if (name == "1M" || name == "LE_1M" || name == "LE1M")
    {
        return BlePhyMode::LE_1M;
    }
    if (name == "2M" || name == "LE_2M" || name == "LE2M")
    {
        return BlePhyMode::LE_2M;
    }
    if (name == "S2" || name == "LE_CODED_S2" || name == "Coded-S2" || name == "CodedS2")
    {
        return BlePhyMode::LE_CODED_S2;
    }
    if (name == "S8" || name == "LE_CODED_S8" || name == "Coded-S8" || name == "CodedS8")
    {
        return BlePhyMode::LE_CODED_S8;
    }
    NS_ABORT_MSG("Unknown PHY mode " << name << " (use 1M, 2M, S2 or S8)");
    return BlePhyMode::LE_1M;
}

double
GetSymbolRate(BlePhyMode mode)
{
    return (mode == BlePhyMode::LE_2M) ? 2.0e6 : 1.0e6;
}

uint32_t
GetSymbolsPerBit(BlePhyMode mode)
{
    switch (mode)
    {
    case BlePhyMode::LE_CODED_S2:
        return 2;
    case BlePhyMode::LE_CODED_S8:
        return 8;
    default:
        return 1;
    }
}

double
GetBitRate(BlePhyMode mode)
{
    return GetSymbolRate(mode) / GetSymbolsPerBit(mode);
}

bool
IsCoded(BlePhyMode mode)
{
    return mode == BlePhyMode::LE_CODED_S2 || mode == BlePhyMode::LE_CODED_S8;
}

uint8_t
ChannelToRfChannel(uint8_t channel)
{
    NS_ABORT_MSG_IF(channel >= N_RF_CHANNELS, "Invalid channel index " << +channel);
    if (channel == ADV_CHANNEL_37)
    {
        return 0;
    }
    if (channel == ADV_CHANNEL_38)
    {
        return 12;
    }
    if (channel == ADV_CHANNEL_39)
    {
        return 39;
    }
    // data channels 0 to 10 occupy RF channels 1 to 11, data channels 11 to 36 occupy 13 to 38
    return (channel <= 10) ? static_cast<uint8_t>(channel + 1) : static_cast<uint8_t>(channel + 2);
}

double
ChannelToFrequencyMhz(uint8_t channel)
{
    return RF_CHANNEL_0_FREQUENCY_MHZ + RF_CHANNEL_SPACING_MHZ * ChannelToRfChannel(channel);
}

bool
IsAdvertisingChannel(uint8_t channel)
{
    return channel >= ADV_CHANNEL_37 && channel <= ADV_CHANNEL_39;
}

} // namespace ble
} // namespace ns3
