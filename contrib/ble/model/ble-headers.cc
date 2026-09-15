/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-headers.h"

#include "ns3/abort.h"

namespace ns3
{
namespace ble
{

const char*
BleAdvPduTypeName(BleAdvPduType type)
{
    switch (type)
    {
    case BleAdvPduType::ADV_IND:
        return "ADV_IND";
    case BleAdvPduType::ADV_DIRECT_IND:
        return "ADV_DIRECT_IND";
    case BleAdvPduType::ADV_NONCONN_IND:
        return "ADV_NONCONN_IND";
    case BleAdvPduType::SCAN_REQ:
        return "SCAN_REQ";
    case BleAdvPduType::SCAN_RSP:
        return "SCAN_RSP";
    case BleAdvPduType::CONNECT_IND:
        return "CONNECT_IND";
    case BleAdvPduType::ADV_SCAN_IND:
        return "ADV_SCAN_IND";
    default:
        return "reserved";
    }
}

namespace
{

/**
 * Write a 48-bit address in the little endian order used on the air.
 * @param start the buffer iterator
 * @param address the address
 */
void
WriteAddress(Buffer::Iterator& start, Mac48Address address)
{
    uint8_t buffer[6];
    address.CopyTo(buffer);
    for (uint8_t i = 0; i < 6; ++i)
    {
        start.WriteU8(buffer[5 - i]);
    }
}

/**
 * Read a 48-bit address written in the little endian order used on the air.
 * @param start the buffer iterator
 * @return the address
 */
Mac48Address
ReadAddress(Buffer::Iterator& start)
{
    uint8_t buffer[6];
    for (uint8_t i = 0; i < 6; ++i)
    {
        buffer[5 - i] = start.ReadU8();
    }
    Mac48Address address;
    address.CopyFrom(buffer);
    return address;
}

} // namespace

/* -------------------------------------------------------------------------------------------- */
/* BleAdvHeader                                                                                   */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleAdvHeader);

BleAdvHeader::BleAdvHeader()
    : m_pduType(BleAdvPduType::ADV_IND),
      m_chSel(false),
      m_txAdd(false),
      m_rxAdd(false),
      m_length(0)
{
}

TypeId
BleAdvHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleAdvHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleAdvHeader>();
    return tid;
}

TypeId
BleAdvHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleAdvHeader::GetSerializedSize() const
{
    return PDU_HEADER_OCTETS;
}

void
BleAdvHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t first = static_cast<uint8_t>(m_pduType) & 0x0F;
    if (m_chSel)
    {
        first |= 0x20;
    }
    if (m_txAdd)
    {
        first |= 0x40;
    }
    if (m_rxAdd)
    {
        first |= 0x80;
    }
    start.WriteU8(first);
    start.WriteU8(m_length);
}

uint32_t
BleAdvHeader::Deserialize(Buffer::Iterator start)
{
    const uint8_t first = start.ReadU8();
    m_pduType = static_cast<BleAdvPduType>(first & 0x0F);
    m_chSel = (first & 0x20) != 0;
    m_txAdd = (first & 0x40) != 0;
    m_rxAdd = (first & 0x80) != 0;
    m_length = start.ReadU8();
    return GetSerializedSize();
}

void
BleAdvHeader::Print(std::ostream& os) const
{
    os << "type=" << BleAdvPduTypeName(m_pduType) << " chSel=" << m_chSel << " txAdd=" << m_txAdd
       << " rxAdd=" << m_rxAdd << " length=" << +m_length;
}

void
BleAdvHeader::SetPduType(BleAdvPduType type)
{
    m_pduType = type;
}

BleAdvPduType
BleAdvHeader::GetPduType() const
{
    return m_pduType;
}

void
BleAdvHeader::SetChSel(bool supported)
{
    m_chSel = supported;
}

bool
BleAdvHeader::GetChSel() const
{
    return m_chSel;
}

void
BleAdvHeader::SetTxAdd(bool random)
{
    m_txAdd = random;
}

bool
BleAdvHeader::GetTxAdd() const
{
    return m_txAdd;
}

void
BleAdvHeader::SetRxAdd(bool random)
{
    m_rxAdd = random;
}

bool
BleAdvHeader::GetRxAdd() const
{
    return m_rxAdd;
}

void
BleAdvHeader::SetLength(uint8_t length)
{
    NS_ABORT_MSG_IF(length > ADV_PDU_PAYLOAD_MAX,
                    "Advertising payload of " << +length << " octets is too long");
    m_length = length;
}

uint8_t
BleAdvHeader::GetLength() const
{
    return m_length;
}

/* -------------------------------------------------------------------------------------------- */
/* BleAdvPayloadHeader                                                                            */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleAdvPayloadHeader);

BleAdvPayloadHeader::BleAdvPayloadHeader()
    : m_advA(Mac48Address::GetBroadcast()),
      m_advDataLength(0)
{
}

TypeId
BleAdvPayloadHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleAdvPayloadHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleAdvPayloadHeader>();
    return tid;
}

TypeId
BleAdvPayloadHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleAdvPayloadHeader::GetSerializedSize() const
{
    return BD_ADDR_OCTETS;
}

void
BleAdvPayloadHeader::Serialize(Buffer::Iterator start) const
{
    WriteAddress(start, m_advA);
}

uint32_t
BleAdvPayloadHeader::Deserialize(Buffer::Iterator start)
{
    m_advA = ReadAddress(start);
    return GetSerializedSize();
}

void
BleAdvPayloadHeader::Print(std::ostream& os) const
{
    os << "advA=" << m_advA << " advDataLength=" << +m_advDataLength;
}

void
BleAdvPayloadHeader::SetAdvA(Mac48Address address)
{
    m_advA = address;
}

Mac48Address
BleAdvPayloadHeader::GetAdvA() const
{
    return m_advA;
}

void
BleAdvPayloadHeader::SetAdvDataLength(uint8_t length)
{
    m_advDataLength = length;
}

uint8_t
BleAdvPayloadHeader::GetAdvDataLength() const
{
    return m_advDataLength;
}

/* -------------------------------------------------------------------------------------------- */
/* BleTwoAddressHeader                                                                            */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleTwoAddressHeader);

BleTwoAddressHeader::BleTwoAddressHeader()
    : m_source(Mac48Address::GetBroadcast()),
      m_advA(Mac48Address::GetBroadcast())
{
}

TypeId
BleTwoAddressHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleTwoAddressHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleTwoAddressHeader>();
    return tid;
}

TypeId
BleTwoAddressHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleTwoAddressHeader::GetSerializedSize() const
{
    return 2 * BD_ADDR_OCTETS;
}

void
BleTwoAddressHeader::Serialize(Buffer::Iterator start) const
{
    WriteAddress(start, m_source);
    WriteAddress(start, m_advA);
}

uint32_t
BleTwoAddressHeader::Deserialize(Buffer::Iterator start)
{
    m_source = ReadAddress(start);
    m_advA = ReadAddress(start);
    return GetSerializedSize();
}

void
BleTwoAddressHeader::Print(std::ostream& os) const
{
    os << "source=" << m_source << " advA=" << m_advA;
}

void
BleTwoAddressHeader::SetSourceAddress(Mac48Address address)
{
    m_source = address;
}

Mac48Address
BleTwoAddressHeader::GetSourceAddress() const
{
    return m_source;
}

void
BleTwoAddressHeader::SetAdvA(Mac48Address address)
{
    m_advA = address;
}

Mac48Address
BleTwoAddressHeader::GetAdvA() const
{
    return m_advA;
}

/* -------------------------------------------------------------------------------------------- */
/* BleConnectIndHeader                                                                            */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleConnectIndHeader);

BleConnectIndHeader::BleConnectIndHeader()
    : m_initA(Mac48Address::GetBroadcast()),
      m_advA(Mac48Address::GetBroadcast()),
      m_accessAddress(0),
      m_crcInit(0),
      m_winSize(1),
      m_winOffset(0),
      m_interval(CONN_INTERVAL_MIN_UNITS),
      m_latency(0),
      m_timeout(100),
      m_hopIncrement(HOP_INCREMENT_MIN),
      m_sca(0)
{
}

TypeId
BleConnectIndHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleConnectIndHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleConnectIndHeader>();
    return tid;
}

TypeId
BleConnectIndHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleConnectIndHeader::GetSerializedSize() const
{
    // InitA(6) + AdvA(6) + AA(4) + CRCInit(3) + WinSize(1) + WinOffset(2) + Interval(2)
    // + Latency(2) + Timeout(2) + ChM(5) + Hop/SCA(1)
    return 2 * BD_ADDR_OCTETS + 4 + 3 + 1 + 2 + 2 + 2 + 2 + 5 + 1;
}

void
BleConnectIndHeader::Serialize(Buffer::Iterator start) const
{
    WriteAddress(start, m_initA);
    WriteAddress(start, m_advA);
    start.WriteU32(m_accessAddress);
    start.WriteU8(static_cast<uint8_t>(m_crcInit & 0xFF));
    start.WriteU8(static_cast<uint8_t>((m_crcInit >> 8) & 0xFF));
    start.WriteU8(static_cast<uint8_t>((m_crcInit >> 16) & 0xFF));
    start.WriteU8(m_winSize);
    start.WriteU16(m_winOffset);
    start.WriteU16(m_interval);
    start.WriteU16(m_latency);
    start.WriteU16(m_timeout);
    uint8_t channelMap[5];
    m_channelMap.Serialize(channelMap);
    for (uint8_t i = 0; i < 5; ++i)
    {
        start.WriteU8(channelMap[i]);
    }
    start.WriteU8(static_cast<uint8_t>((m_hopIncrement & 0x1F) | ((m_sca & 0x07) << 5)));
}

uint32_t
BleConnectIndHeader::Deserialize(Buffer::Iterator start)
{
    m_initA = ReadAddress(start);
    m_advA = ReadAddress(start);
    m_accessAddress = start.ReadU32();
    const uint32_t crc0 = start.ReadU8();
    const uint32_t crc1 = start.ReadU8();
    const uint32_t crc2 = start.ReadU8();
    m_crcInit = crc0 | (crc1 << 8) | (crc2 << 16);
    m_winSize = start.ReadU8();
    m_winOffset = start.ReadU16();
    m_interval = start.ReadU16();
    m_latency = start.ReadU16();
    m_timeout = start.ReadU16();
    uint8_t channelMap[5];
    for (uint8_t i = 0; i < 5; ++i)
    {
        channelMap[i] = start.ReadU8();
    }
    m_channelMap = BleChannelMap(channelMap);
    const uint8_t hopSca = start.ReadU8();
    m_hopIncrement = hopSca & 0x1F;
    m_sca = (hopSca >> 5) & 0x07;
    return GetSerializedSize();
}

void
BleConnectIndHeader::Print(std::ostream& os) const
{
    os << "initA=" << m_initA << " advA=" << m_advA << " aa=0x" << std::hex << m_accessAddress
       << std::dec << " interval=" << m_interval << " latency=" << m_latency
       << " timeout=" << m_timeout << " hop=" << +m_hopIncrement
       << " usedChannels=" << +m_channelMap.GetNUsedChannels();
}

void
BleConnectIndHeader::SetInitA(Mac48Address address)
{
    m_initA = address;
}

Mac48Address
BleConnectIndHeader::GetInitA() const
{
    return m_initA;
}

void
BleConnectIndHeader::SetAdvA(Mac48Address address)
{
    m_advA = address;
}

Mac48Address
BleConnectIndHeader::GetAdvA() const
{
    return m_advA;
}

void
BleConnectIndHeader::SetAccessAddress(uint32_t accessAddress)
{
    m_accessAddress = accessAddress;
}

uint32_t
BleConnectIndHeader::GetAccessAddress() const
{
    return m_accessAddress;
}

void
BleConnectIndHeader::SetCrcInit(uint32_t crcInit)
{
    m_crcInit = crcInit & 0xFFFFFF;
}

uint32_t
BleConnectIndHeader::GetCrcInit() const
{
    return m_crcInit;
}

void
BleConnectIndHeader::SetWinSize(uint8_t winSize)
{
    m_winSize = winSize;
}

uint8_t
BleConnectIndHeader::GetWinSize() const
{
    return m_winSize;
}

void
BleConnectIndHeader::SetWinOffset(uint16_t winOffset)
{
    m_winOffset = winOffset;
}

uint16_t
BleConnectIndHeader::GetWinOffset() const
{
    return m_winOffset;
}

void
BleConnectIndHeader::SetInterval(uint16_t interval)
{
    NS_ABORT_MSG_IF(interval < CONN_INTERVAL_MIN_UNITS || interval > CONN_INTERVAL_MAX_UNITS,
                    "Connection interval of " << interval << " units is out of range");
    m_interval = interval;
}

uint16_t
BleConnectIndHeader::GetInterval() const
{
    return m_interval;
}

void
BleConnectIndHeader::SetLatency(uint16_t latency)
{
    m_latency = latency;
}

uint16_t
BleConnectIndHeader::GetLatency() const
{
    return m_latency;
}

void
BleConnectIndHeader::SetTimeout(uint16_t timeout)
{
    m_timeout = timeout;
}

uint16_t
BleConnectIndHeader::GetTimeout() const
{
    return m_timeout;
}

void
BleConnectIndHeader::SetChannelMap(const BleChannelMap& map)
{
    m_channelMap = map;
}

BleChannelMap
BleConnectIndHeader::GetChannelMap() const
{
    return m_channelMap;
}

void
BleConnectIndHeader::SetHopIncrement(uint8_t hopIncrement)
{
    NS_ABORT_MSG_IF(hopIncrement < HOP_INCREMENT_MIN || hopIncrement > HOP_INCREMENT_MAX,
                    "Invalid hop increment " << +hopIncrement);
    m_hopIncrement = hopIncrement;
}

uint8_t
BleConnectIndHeader::GetHopIncrement() const
{
    return m_hopIncrement;
}

void
BleConnectIndHeader::SetSca(uint8_t sca)
{
    m_sca = sca & 0x07;
}

uint8_t
BleConnectIndHeader::GetSca() const
{
    return m_sca;
}

/* -------------------------------------------------------------------------------------------- */
/* BleDataHeader                                                                                  */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleDataHeader);

BleDataHeader::BleDataHeader()
    : m_llid(BleLlid::DATA_CONTINUATION),
      m_nesn(false),
      m_sn(false),
      m_md(false),
      m_length(0)
{
}

TypeId
BleDataHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleDataHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleDataHeader>();
    return tid;
}

TypeId
BleDataHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleDataHeader::GetSerializedSize() const
{
    return PDU_HEADER_OCTETS;
}

void
BleDataHeader::Serialize(Buffer::Iterator start) const
{
    uint8_t first = static_cast<uint8_t>(m_llid) & 0x03;
    if (m_nesn)
    {
        first |= 0x04;
    }
    if (m_sn)
    {
        first |= 0x08;
    }
    if (m_md)
    {
        first |= 0x10;
    }
    start.WriteU8(first);
    start.WriteU8(m_length);
}

uint32_t
BleDataHeader::Deserialize(Buffer::Iterator start)
{
    const uint8_t first = start.ReadU8();
    m_llid = static_cast<BleLlid>(first & 0x03);
    m_nesn = (first & 0x04) != 0;
    m_sn = (first & 0x08) != 0;
    m_md = (first & 0x10) != 0;
    m_length = start.ReadU8();
    return GetSerializedSize();
}

void
BleDataHeader::Print(std::ostream& os) const
{
    os << "llid=" << static_cast<uint32_t>(m_llid) << " nesn=" << m_nesn << " sn=" << m_sn
       << " md=" << m_md << " length=" << +m_length;
}

void
BleDataHeader::SetLlid(BleLlid llid)
{
    m_llid = llid;
}

BleLlid
BleDataHeader::GetLlid() const
{
    return m_llid;
}

void
BleDataHeader::SetNesn(bool nesn)
{
    m_nesn = nesn;
}

bool
BleDataHeader::GetNesn() const
{
    return m_nesn;
}

void
BleDataHeader::SetSn(bool sn)
{
    m_sn = sn;
}

bool
BleDataHeader::GetSn() const
{
    return m_sn;
}

void
BleDataHeader::SetMd(bool md)
{
    m_md = md;
}

bool
BleDataHeader::GetMd() const
{
    return m_md;
}

void
BleDataHeader::SetLength(uint8_t length)
{
    m_length = length;
}

uint8_t
BleDataHeader::GetLength() const
{
    return m_length;
}

bool
BleDataHeader::IsEmpty() const
{
    return m_length == 0;
}

/* -------------------------------------------------------------------------------------------- */
/* BleLlControlHeader                                                                             */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleLlControlHeader);

BleLlControlHeader::BleLlControlHeader()
    : m_opcode(BleLlControlOpcode::LL_FEATURE_REQ)
{
}

TypeId
BleLlControlHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleLlControlHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleLlControlHeader>();
    return tid;
}

TypeId
BleLlControlHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleLlControlHeader::GetSerializedSize() const
{
    return 2 + static_cast<uint32_t>(m_parameters.size());
}

void
BleLlControlHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(static_cast<uint8_t>(m_opcode));
    start.WriteU8(static_cast<uint8_t>(m_parameters.size()));
    for (const auto octet : m_parameters)
    {
        start.WriteU8(octet);
    }
}

uint32_t
BleLlControlHeader::Deserialize(Buffer::Iterator start)
{
    m_opcode = static_cast<BleLlControlOpcode>(start.ReadU8());
    const uint8_t count = start.ReadU8();
    m_parameters.clear();
    for (uint8_t i = 0; i < count; ++i)
    {
        m_parameters.push_back(start.ReadU8());
    }
    return GetSerializedSize();
}

void
BleLlControlHeader::Print(std::ostream& os) const
{
    os << "opcode=0x" << std::hex << static_cast<uint32_t>(m_opcode) << std::dec
       << " parameters=" << m_parameters.size();
}

void
BleLlControlHeader::SetOpcode(BleLlControlOpcode opcode)
{
    m_opcode = opcode;
}

BleLlControlOpcode
BleLlControlHeader::GetOpcode() const
{
    return m_opcode;
}

void
BleLlControlHeader::SetParameters(const std::vector<uint8_t>& parameters)
{
    NS_ABORT_MSG_IF(parameters.size() > 8, "An LL Control PDU carries at most eight parameters");
    m_parameters = parameters;
}

std::vector<uint8_t>
BleLlControlHeader::GetParameters() const
{
    return m_parameters;
}

/* -------------------------------------------------------------------------------------------- */
/* BleL2capHeader                                                                                 */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleL2capHeader);

BleL2capHeader::BleL2capHeader()
    : m_length(0),
      m_cid(4) // the attribute protocol channel
{
}

TypeId
BleL2capHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleL2capHeader")
                            .SetParent<Header>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleL2capHeader>();
    return tid;
}

TypeId
BleL2capHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleL2capHeader::GetSerializedSize() const
{
    return L2CAP_HEADER_OCTETS;
}

void
BleL2capHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU16(m_length);
    start.WriteU16(m_cid);
}

uint32_t
BleL2capHeader::Deserialize(Buffer::Iterator start)
{
    m_length = start.ReadU16();
    m_cid = start.ReadU16();
    return GetSerializedSize();
}

void
BleL2capHeader::Print(std::ostream& os) const
{
    os << "length=" << m_length << " cid=" << m_cid;
}

void
BleL2capHeader::SetLength(uint16_t length)
{
    m_length = length;
}

uint16_t
BleL2capHeader::GetLength() const
{
    return m_length;
}

void
BleL2capHeader::SetChannelId(uint16_t cid)
{
    m_cid = cid;
}

uint16_t
BleL2capHeader::GetChannelId() const
{
    return m_cid;
}

/* -------------------------------------------------------------------------------------------- */
/* BleCrcTrailer                                                                                  */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(BleCrcTrailer);

BleCrcTrailer::BleCrcTrailer()
    : m_crc(0)
{
}

TypeId
BleCrcTrailer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ble::BleCrcTrailer")
                            .SetParent<Trailer>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleCrcTrailer>();
    return tid;
}

TypeId
BleCrcTrailer::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
BleCrcTrailer::GetSerializedSize() const
{
    return CRC_OCTETS;
}

void
BleCrcTrailer::Serialize(Buffer::Iterator start) const
{
    // trailers are serialized from the end of the buffer backwards
    start.Prev(CRC_OCTETS);
    start.WriteU8(static_cast<uint8_t>(m_crc & 0xFF));
    start.WriteU8(static_cast<uint8_t>((m_crc >> 8) & 0xFF));
    start.WriteU8(static_cast<uint8_t>((m_crc >> 16) & 0xFF));
}

uint32_t
BleCrcTrailer::Deserialize(Buffer::Iterator start)
{
    start.Prev(CRC_OCTETS);
    const uint32_t byte0 = start.ReadU8();
    const uint32_t byte1 = start.ReadU8();
    const uint32_t byte2 = start.ReadU8();
    m_crc = byte0 | (byte1 << 8) | (byte2 << 16);
    return CRC_OCTETS;
}

void
BleCrcTrailer::Print(std::ostream& os) const
{
    os << "crc=0x" << std::hex << m_crc << std::dec;
}

void
BleCrcTrailer::SetCrc(uint32_t crc)
{
    m_crc = crc & 0xFFFFFF;
}

uint32_t
BleCrcTrailer::GetCrc() const
{
    return m_crc;
}

} // namespace ble
} // namespace ns3
