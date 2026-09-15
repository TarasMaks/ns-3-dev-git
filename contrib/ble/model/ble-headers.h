/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_HEADERS_H
#define BLE_HEADERS_H

#include "ble-constants.h"
#include "ble-utils.h"

#include "ns3/header.h"
#include "ns3/mac48-address.h"
#include "ns3/trailer.h"

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * Advertising channel PDU types (Vol 6, Part B, Section 2.3).
 */
enum class BleAdvPduType : uint8_t
{
    ADV_IND = 0x00,         //!< connectable and scannable undirected advertising
    ADV_DIRECT_IND = 0x01,  //!< connectable directed advertising
    ADV_NONCONN_IND = 0x02, //!< non-connectable and non-scannable undirected advertising
    SCAN_REQ = 0x03,        //!< scan request
    SCAN_RSP = 0x04,        //!< scan response
    CONNECT_IND = 0x05,     //!< connection indication
    ADV_SCAN_IND = 0x06,    //!< scannable undirected advertising
};

/**
 * @ingroup ble
 * @param type an advertising PDU type
 * @return a printable name
 */
const char* BleAdvPduTypeName(BleAdvPduType type);

/**
 * @ingroup ble
 * Header of an advertising channel PDU (Vol 6, Part B, Section 2.3): a 16-bit header carrying
 * the PDU type, the channel selection and address type flags and the payload length.
 */
class BleAdvHeader : public Header
{
  public:
    BleAdvHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param type the PDU type
     */
    void SetPduType(BleAdvPduType type);
    /// @return the PDU type
    BleAdvPduType GetPduType() const;

    /**
     * @param supported whether the advertiser supports channel selection algorithm #2
     */
    void SetChSel(bool supported);
    /// @return whether channel selection algorithm #2 is supported
    bool GetChSel() const;

    /**
     * @param random whether the advertiser address is a random address
     */
    void SetTxAdd(bool random);
    /// @return whether the advertiser address is a random address
    bool GetTxAdd() const;

    /**
     * @param random whether the target address is a random address
     */
    void SetRxAdd(bool random);
    /// @return whether the target address is a random address
    bool GetRxAdd() const;

    /**
     * @param length the payload length in octets
     */
    void SetLength(uint8_t length);
    /// @return the payload length in octets
    uint8_t GetLength() const;

  private:
    BleAdvPduType m_pduType; //!< PDU type
    bool m_chSel;            //!< channel selection algorithm #2 supported
    bool m_txAdd;            //!< advertiser address is random
    bool m_rxAdd;            //!< target address is random
    uint8_t m_length;        //!< payload length
};

/**
 * @ingroup ble
 * Payload of an ADV_IND, ADV_NONCONN_IND or ADV_SCAN_IND PDU: the advertiser address followed
 * by the advertising data.
 */
class BleAdvPayloadHeader : public Header
{
  public:
    BleAdvPayloadHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param address the advertiser address
     */
    void SetAdvA(Mac48Address address);
    /// @return the advertiser address
    Mac48Address GetAdvA() const;

    /**
     * @param length the number of octets of advertising data carried after the address
     */
    void SetAdvDataLength(uint8_t length);
    /// @return the number of octets of advertising data
    uint8_t GetAdvDataLength() const;

  private:
    Mac48Address m_advA;     //!< advertiser address
    uint8_t m_advDataLength; //!< advertising data length
};

/**
 * @ingroup ble
 * Payload of a SCAN_REQ or ADV_DIRECT_IND PDU: the scanner or initiator address followed by the
 * advertiser address.
 */
class BleTwoAddressHeader : public Header
{
  public:
    BleTwoAddressHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param address the address of the device sending the PDU
     */
    void SetSourceAddress(Mac48Address address);
    /// @return the address of the device sending the PDU
    Mac48Address GetSourceAddress() const;

    /**
     * @param address the address of the advertiser the PDU is directed to
     */
    void SetAdvA(Mac48Address address);
    /// @return the address of the advertiser the PDU is directed to
    Mac48Address GetAdvA() const;

  private:
    Mac48Address m_source; //!< scanner or initiator address
    Mac48Address m_advA;   //!< advertiser address
};

/**
 * @ingroup ble
 * Payload of a CONNECT_IND PDU (Vol 6, Part B, Section 2.3.3.1): the initiator and advertiser
 * addresses followed by the LLData field that parameterises the connection.
 */
class BleConnectIndHeader : public Header
{
  public:
    BleConnectIndHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param address the initiator address
     */
    void SetInitA(Mac48Address address);
    /// @return the initiator address
    Mac48Address GetInitA() const;

    /**
     * @param address the advertiser address
     */
    void SetAdvA(Mac48Address address);
    /// @return the advertiser address
    Mac48Address GetAdvA() const;

    /**
     * @param accessAddress the access address of the connection
     */
    void SetAccessAddress(uint32_t accessAddress);
    /// @return the access address of the connection
    uint32_t GetAccessAddress() const;

    /**
     * @param crcInit the initial CRC value of the connection
     */
    void SetCrcInit(uint32_t crcInit);
    /// @return the initial CRC value of the connection
    uint32_t GetCrcInit() const;

    /**
     * @param winSize the transmit window size, in units of 1.25 ms
     */
    void SetWinSize(uint8_t winSize);
    /// @return the transmit window size, in units of 1.25 ms
    uint8_t GetWinSize() const;

    /**
     * @param winOffset the transmit window offset, in units of 1.25 ms
     */
    void SetWinOffset(uint16_t winOffset);
    /// @return the transmit window offset, in units of 1.25 ms
    uint16_t GetWinOffset() const;

    /**
     * @param interval the connection interval, in units of 1.25 ms
     */
    void SetInterval(uint16_t interval);
    /// @return the connection interval, in units of 1.25 ms
    uint16_t GetInterval() const;

    /**
     * @param latency the peripheral latency, in connection events
     */
    void SetLatency(uint16_t latency);
    /// @return the peripheral latency, in connection events
    uint16_t GetLatency() const;

    /**
     * @param timeout the supervision timeout, in units of 10 ms
     */
    void SetTimeout(uint16_t timeout);
    /// @return the supervision timeout, in units of 10 ms
    uint16_t GetTimeout() const;

    /**
     * @param map the channel map of the connection
     */
    void SetChannelMap(const BleChannelMap& map);
    /// @return the channel map of the connection
    BleChannelMap GetChannelMap() const;

    /**
     * @param hopIncrement the hop increment of channel selection algorithm #1 (5 to 16)
     */
    void SetHopIncrement(uint8_t hopIncrement);
    /// @return the hop increment of channel selection algorithm #1
    uint8_t GetHopIncrement() const;

    /**
     * @param sca the sleep clock accuracy code (0 to 7)
     */
    void SetSca(uint8_t sca);
    /// @return the sleep clock accuracy code
    uint8_t GetSca() const;

  private:
    Mac48Address m_initA;       //!< initiator address
    Mac48Address m_advA;        //!< advertiser address
    uint32_t m_accessAddress;   //!< access address
    uint32_t m_crcInit;         //!< CRC initial value
    uint8_t m_winSize;          //!< transmit window size
    uint16_t m_winOffset;       //!< transmit window offset
    uint16_t m_interval;        //!< connection interval
    uint16_t m_latency;         //!< peripheral latency
    uint16_t m_timeout;         //!< supervision timeout
    BleChannelMap m_channelMap; //!< channel map
    uint8_t m_hopIncrement;     //!< hop increment
    uint8_t m_sca;              //!< sleep clock accuracy
};

/**
 * @ingroup ble
 * LLID field of a data channel PDU (Vol 6, Part B, Section 2.4).
 */
enum class BleLlid : uint8_t
{
    RESERVED = 0x00,          //!< reserved, not used
    DATA_CONTINUATION = 0x01, //!< continuation of an L2CAP message, or an empty PDU
    DATA_START = 0x02,        //!< start of an L2CAP message or a complete message
    CONTROL = 0x03,           //!< LL Control PDU
};

/**
 * @ingroup ble
 * Header of a data channel PDU (Vol 6, Part B, Section 2.4): the LLID, the sequence and
 * acknowledgement numbers, the more data flag and the payload length.
 */
class BleDataHeader : public Header
{
  public:
    BleDataHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param llid the LLID field
     */
    void SetLlid(BleLlid llid);
    /// @return the LLID field
    BleLlid GetLlid() const;

    /**
     * @param nesn the next expected sequence number
     */
    void SetNesn(bool nesn);
    /// @return the next expected sequence number
    bool GetNesn() const;

    /**
     * @param sn the sequence number
     */
    void SetSn(bool sn);
    /// @return the sequence number
    bool GetSn() const;

    /**
     * @param md whether the sender has more data to send in this connection event
     */
    void SetMd(bool md);
    /// @return whether the sender has more data to send in this connection event
    bool GetMd() const;

    /**
     * @param length the payload length in octets
     */
    void SetLength(uint8_t length);
    /// @return the payload length in octets
    uint8_t GetLength() const;

    /// @return true if the PDU carries no payload
    bool IsEmpty() const;

  private:
    BleLlid m_llid;   //!< LLID
    bool m_nesn;      //!< next expected sequence number
    bool m_sn;        //!< sequence number
    bool m_md;        //!< more data
    uint8_t m_length; //!< payload length
};

/**
 * @ingroup ble
 * Opcodes of the LL Control PDUs modelled here (Vol 6, Part B, Section 2.4.2).
 */
enum class BleLlControlOpcode : uint8_t
{
    LL_CONNECTION_UPDATE_IND = 0x00, //!< change the connection parameters
    LL_CHANNEL_MAP_IND = 0x01,       //!< change the channel map
    LL_TERMINATE_IND = 0x02,         //!< terminate the connection
    LL_FEATURE_REQ = 0x08,           //!< request the supported features
    LL_FEATURE_RSP = 0x09,           //!< report the supported features
    LL_VERSION_IND = 0x0C,           //!< report the version information
    LL_PHY_REQ = 0x16,               //!< request a PHY change
    LL_PHY_RSP = 0x17,               //!< respond to a PHY change request
    LL_PHY_UPDATE_IND = 0x18,        //!< instruct a PHY change
    LL_LENGTH_REQ = 0x14,            //!< request a data length change
    LL_LENGTH_RSP = 0x15,            //!< respond to a data length change request
};

/**
 * @ingroup ble
 * Payload of an LL Control PDU: the opcode followed by opcode specific octets. The control
 * procedures modelled here carry at most a PHY mode, a channel map or a length, so the payload
 * is represented by an opcode and up to eight octets of parameters.
 */
class BleLlControlHeader : public Header
{
  public:
    BleLlControlHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param opcode the control PDU opcode
     */
    void SetOpcode(BleLlControlOpcode opcode);
    /// @return the control PDU opcode
    BleLlControlOpcode GetOpcode() const;

    /**
     * @param parameters the opcode specific octets (at most eight)
     */
    void SetParameters(const std::vector<uint8_t>& parameters);
    /// @return the opcode specific octets
    std::vector<uint8_t> GetParameters() const;

  private:
    BleLlControlOpcode m_opcode;       //!< opcode
    std::vector<uint8_t> m_parameters; //!< opcode specific octets
};

/**
 * @ingroup ble
 * The L2CAP basic header that precedes every payload handed to the Link Layer (Vol 3, Part A,
 * Section 3.1): the length of the payload and the channel identifier it belongs to. It lets the
 * receiver know how many fragments make up a payload.
 */
class BleL2capHeader : public Header
{
  public:
    BleL2capHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param length the length of the payload, in octets
     */
    void SetLength(uint16_t length);
    /// @return the length of the payload, in octets
    uint16_t GetLength() const;

    /**
     * @param cid the channel identifier
     */
    void SetChannelId(uint16_t cid);
    /// @return the channel identifier
    uint16_t GetChannelId() const;

  private:
    uint16_t m_length; //!< payload length
    uint16_t m_cid;    //!< channel identifier
};

/**
 * @ingroup ble
 * The 24-bit CRC that terminates every BLE packet (Vol 6, Part B, Section 2.1.4).
 */
class BleCrcTrailer : public Trailer
{
  public:
    BleCrcTrailer();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /**
     * @param crc the 24-bit CRC
     */
    void SetCrc(uint32_t crc);
    /// @return the 24-bit CRC
    uint32_t GetCrc() const;

  private:
    uint32_t m_crc; //!< the CRC
};

} // namespace ble
} // namespace ns3

#endif /* BLE_HEADERS_H */
