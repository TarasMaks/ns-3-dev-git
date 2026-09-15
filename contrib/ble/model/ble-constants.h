/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_CONSTANTS_H
#define BLE_CONSTANTS_H

#include "ns3/nstime.h"

#include <cstdint>

/**
 * @defgroup ble Bluetooth Low Energy (BLE) models
 *
 * A custom Bluetooth Low Energy model for ns-3, covering the Link Layer and the physical
 * layer of Bluetooth Core Specification 5.x (Vol 6, Parts A and B): the four LE PHYs
 * (LE 1M, LE 2M, LE Coded S=2 and S=8), advertising, scanning, connection establishment and
 * connection events with channel hopping.
 */

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * LE PHY variants of Bluetooth Core Specification 5.x (Vol 6, Part A, Section 2).
 */
enum class BlePhyMode : uint8_t
{
    LE_1M = 0,   //!< uncoded, 1 Msym/s, 1 Mb/s
    LE_2M,       //!< uncoded, 2 Msym/s, 2 Mb/s
    LE_CODED_S2, //!< coded, 1 Msym/s, 500 kb/s (2 symbols per bit)
    LE_CODED_S8, //!< coded, 1 Msym/s, 125 kb/s (8 symbols per bit)
    PHY_MODE_COUNT
};

/**
 * @ingroup ble
 * @param mode a PHY mode
 * @return a printable name
 */
const char* BlePhyModeName(BlePhyMode mode);

/**
 * @ingroup ble
 * @param name "1M", "2M", "S2" or "S8" (also "LE_1M", "Coded-S2", ...)
 * @return the PHY mode
 */
BlePhyMode BlePhyModeFromString(const std::string& name);

/**
 * @ingroup ble
 * @param mode a PHY mode
 * @return the symbol rate in symbols per second
 */
double GetSymbolRate(BlePhyMode mode);

/**
 * @ingroup ble
 * @param mode a PHY mode
 * @return the information bit rate in bits per second
 */
double GetBitRate(BlePhyMode mode);

/**
 * @ingroup ble
 * @param mode a PHY mode
 * @return the number of symbols transmitted per information bit (1, 1, 2 or 8)
 */
uint32_t GetSymbolsPerBit(BlePhyMode mode);

/**
 * @ingroup ble
 * @param mode a PHY mode
 * @return true for the LE Coded PHY variants
 */
bool IsCoded(BlePhyMode mode);

/// @ingroup ble
/// Number of RF channels of the LE system (Vol 6, Part A, Section 2).
constexpr uint8_t N_RF_CHANNELS = 40;
/// Number of data channels usable by a connection (Vol 6, Part B, Section 4.5.8).
constexpr uint8_t N_DATA_CHANNELS = 37;
/// Lowest RF centre frequency, in MHz (RF channel 0).
constexpr double RF_CHANNEL_0_FREQUENCY_MHZ = 2402.0;
/// Spacing between RF channels, in MHz.
constexpr double RF_CHANNEL_SPACING_MHZ = 2.0;

/// Logical channel index of the first primary advertising channel.
constexpr uint8_t ADV_CHANNEL_37 = 37;
/// Logical channel index of the second primary advertising channel.
constexpr uint8_t ADV_CHANNEL_38 = 38;
/// Logical channel index of the third primary advertising channel.
constexpr uint8_t ADV_CHANNEL_39 = 39;

/// Inter Frame Space between consecutive packets of an exchange (Vol 6, Part B, Section 4.1.1).
constexpr uint32_t T_IFS_US = 150;
/// Tolerance of the Inter Frame Space.
constexpr uint32_t T_IFS_TOLERANCE_US = 2;
/// Minimum AUX Frame Space.
constexpr uint32_t T_MAFS_US = 300;

/// Length of the access address, in octets.
constexpr uint32_t ACCESS_ADDRESS_OCTETS = 4;
/// Length of the CRC, in octets.
constexpr uint32_t CRC_OCTETS = 3;
/// Length of the header of an advertising or data channel PDU, in octets.
constexpr uint32_t PDU_HEADER_OCTETS = 2;
/// Length of a device address (BD_ADDR), in octets.
constexpr uint32_t BD_ADDR_OCTETS = 6;

/// Preamble length in symbols for the uncoded PHYs at 1 Msym/s.
constexpr uint32_t PREAMBLE_SYMBOLS_1M = 8;
/// Preamble length in symbols for the uncoded PHYs at 2 Msym/s.
constexpr uint32_t PREAMBLE_SYMBOLS_2M = 16;
/// Preamble length in symbols for the LE Coded PHY (Vol 6, Part B, Section 2.2).
constexpr uint32_t PREAMBLE_SYMBOLS_CODED = 80;
/// Coding indicator length, in bits (LE Coded PHY only).
constexpr uint32_t CODING_INDICATOR_BITS = 2;
/// Length of TERM1 and TERM2, in bits (LE Coded PHY only).
constexpr uint32_t TERMINATOR_BITS = 3;
/// Spreading factor of FEC block 1 of the LE Coded PHY (always S=8).
constexpr uint32_t CODED_FEC1_SYMBOLS_PER_BIT = 8;

/// Smallest advertising interval of the undirected advertising events, in microseconds.
constexpr uint32_t ADV_INTERVAL_MIN_US = 20000;
/// Largest advertising interval, in microseconds.
constexpr uint64_t ADV_INTERVAL_MAX_US = 10485750;
/// Largest random delay added to each advertising event (Vol 6, Part B, Section 4.4.2.2).
constexpr uint32_t ADV_DELAY_MAX_US = 10000;

/// Unit of the connection interval and of the connection timing parameters, in microseconds.
constexpr uint32_t CONN_INTERVAL_UNIT_US = 1250;
/// Smallest connection interval, in units of 1.25 ms.
constexpr uint16_t CONN_INTERVAL_MIN_UNITS = 6;
/// Largest connection interval, in units of 1.25 ms.
constexpr uint16_t CONN_INTERVAL_MAX_UNITS = 3200;
/// Unit of the supervision timeout, in microseconds.
constexpr uint32_t SUPERVISION_TIMEOUT_UNIT_US = 10000;
/// Largest peripheral latency, in connection events.
constexpr uint16_t PERIPHERAL_LATENCY_MAX = 499;
/// Smallest hop increment of channel selection algorithm #1.
constexpr uint8_t HOP_INCREMENT_MIN = 5;
/// Largest hop increment of channel selection algorithm #1.
constexpr uint8_t HOP_INCREMENT_MAX = 16;
/// Unit of the transmit window size and offset, in microseconds.
constexpr uint32_t CONN_WINDOW_UNIT_US = 1250;
/// Fixed offset between the end of CONNECT_IND and the transmit window (Vol 6, Part B, 4.5.3).
constexpr uint32_t TRANSMIT_WINDOW_DELAY_US = 1250;

/// Largest payload of an advertising channel PDU, in octets (legacy advertising).
constexpr uint32_t ADV_PDU_PAYLOAD_MAX = 37;
/// Largest payload of a data channel PDU without Data Length Extension, in octets.
constexpr uint32_t DATA_PDU_PAYLOAD_MIN_MAX = 27;
/// Largest payload of a data channel PDU with Data Length Extension, in octets.
constexpr uint32_t DATA_PDU_PAYLOAD_MAX = 251;
/// Octets consumed by the L2CAP basic header carried in the first fragment of an SDU.
constexpr uint32_t L2CAP_HEADER_OCTETS = 4;

/// Initial value of the CRC for advertising channel PDUs (Vol 6, Part B, Section 3.1.1).
constexpr uint32_t ADV_CRC_INIT = 0x555555;
/// Access address of all advertising channel PDUs (Vol 6, Part B, Section 2.1.2).
constexpr uint32_t ADV_ACCESS_ADDRESS = 0x8E89BED6;

/// Reference sensitivity required by the specification for LE 1M and LE 2M, in dBm.
constexpr double REFERENCE_SENSITIVITY_DBM = -70.0;
/// Packet error rate at which the reference sensitivity is defined (Vol 6, Part A, Section 4.1).
constexpr double REFERENCE_SENSITIVITY_PER = 0.308;
/// Payload of the reference packets used for the sensitivity, in octets.
constexpr uint32_t REFERENCE_SENSITIVITY_PAYLOAD = 37;

/**
 * @ingroup ble
 * @param channel a logical channel index (0-36 data, 37-39 advertising)
 * @return the RF channel index (0-39)
 */
uint8_t ChannelToRfChannel(uint8_t channel);

/**
 * @ingroup ble
 * @param channel a logical channel index (0-36 data, 37-39 advertising)
 * @return the centre frequency in MHz
 */
double ChannelToFrequencyMhz(uint8_t channel);

/**
 * @ingroup ble
 * @param channel a logical channel index
 * @return true if the channel is one of the three primary advertising channels
 */
bool IsAdvertisingChannel(uint8_t channel);

} // namespace ble
} // namespace ns3

#endif /* BLE_CONSTANTS_H */
