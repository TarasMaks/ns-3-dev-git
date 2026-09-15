/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-headers.h"

#include "uwb-utils.h"

#include "ns3/log.h"
#include "ns3/packet.h"

#include <cmath>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbHeaders");

namespace uwb
{

namespace
{

/// Octets a forty bit counter reading occupies on the air.
constexpr uint32_t TIMESTAMP_OCTETS = 5;

/// @param start the buffer to write into
/// @param value a counter reading, reduced to the width of the hardware counter
void
WriteTimestamp(Buffer::Iterator& start, uint64_t value)
{
    const uint64_t masked = value & (DTU_COUNTER_MODULUS - 1);
    for (int shift = 32; shift >= 0; shift -= 8)
    {
        start.WriteU8(static_cast<uint8_t>((masked >> shift) & 0xFF));
    }
}

/// @param start the buffer to read from
/// @return the counter reading
uint64_t
ReadTimestamp(Buffer::Iterator& start)
{
    uint64_t value = 0;
    for (int i = 0; i < 5; ++i)
    {
        value = (value << 8) | start.ReadU8();
    }
    return value;
}

/// @param address a short address
/// @return its two octets as a 16-bit number
uint16_t
AddressToU16(Mac16Address address)
{
    uint8_t buffer[2];
    address.CopyTo(buffer);
    return static_cast<uint16_t>(buffer[0]) << 8 | buffer[1];
}

/// @param value two octets
/// @return the short address they stand for
Mac16Address
U16ToAddress(uint16_t value)
{
    const uint8_t buffer[2]{static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    Mac16Address address;
    address.CopyFrom(buffer);
    return address;
}

} // namespace

std::ostream&
operator<<(std::ostream& os, UwbFrameType type)
{
    switch (type)
    {
    case UwbFrameType::BEACON:
        return os << "BEACON";
    case UwbFrameType::DATA:
        return os << "DATA";
    case UwbFrameType::ACK:
        return os << "ACK";
    case UwbFrameType::COMMAND:
        return os << "COMMAND";
    default:
        return os << "invalid";
    }
}

std::ostream&
operator<<(std::ostream& os, UwbRangingMessage message)
{
    return os << UwbRangingMessageName(message);
}

/* -------------------------------------------------------------------------------------------- */
/* MAC header                                                                                     */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(UwbMacHeader);

UwbMacHeader::UwbMacHeader()
    : m_frameType(UwbFrameType::DATA),
      m_ackRequest(false),
      m_sequenceNumber(0),
      m_panId(0)
{
}

TypeId
UwbMacHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::uwb::UwbMacHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uwb")
                            .AddConstructor<UwbMacHeader>();
    return tid;
}

TypeId
UwbMacHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UwbMacHeader::GetSerializedSize() const
{
    // frame control, sequence number, destination network and address, source address
    return 2 + 1 + 2 + 2 + 2;
}

void
UwbMacHeader::Serialize(Buffer::Iterator start) const
{
    // frame type in the lowest three bits, acknowledgement request at bit five, network
    // identifier compression at bit six, and both addressing modes set to short addresses
    uint16_t frameControl = static_cast<uint16_t>(m_frameType) & 0x07;
    frameControl |= m_ackRequest ? (1u << 5) : 0u;
    frameControl |= (1u << 6);
    frameControl |= (2u << 10);
    frameControl |= (2u << 14);

    start.WriteHtolsbU16(frameControl);
    start.WriteU8(m_sequenceNumber);
    start.WriteHtolsbU16(m_panId);
    start.WriteHtolsbU16(AddressToU16(m_destination));
    start.WriteHtolsbU16(AddressToU16(m_source));
}

uint32_t
UwbMacHeader::Deserialize(Buffer::Iterator start)
{
    const uint16_t frameControl = start.ReadLsbtohU16();
    m_frameType = static_cast<UwbFrameType>(frameControl & 0x07);
    m_ackRequest = (frameControl & (1u << 5)) != 0;
    m_sequenceNumber = start.ReadU8();
    m_panId = start.ReadLsbtohU16();
    m_destination = U16ToAddress(start.ReadLsbtohU16());
    m_source = U16ToAddress(start.ReadLsbtohU16());
    return GetSerializedSize();
}

void
UwbMacHeader::Print(std::ostream& os) const
{
    os << "type=" << static_cast<uint32_t>(m_frameType) << " seq=" << +m_sequenceNumber
       << " pan=" << m_panId << " " << m_source << " -> " << m_destination
       << (m_ackRequest ? " ack" : "");
}

void
UwbMacHeader::SetFrameType(UwbFrameType type)
{
    m_frameType = type;
}

UwbFrameType
UwbMacHeader::GetFrameType() const
{
    return m_frameType;
}

void
UwbMacHeader::SetSequenceNumber(uint8_t sequence)
{
    m_sequenceNumber = sequence;
}

uint8_t
UwbMacHeader::GetSequenceNumber() const
{
    return m_sequenceNumber;
}

void
UwbMacHeader::SetPanId(uint16_t panId)
{
    m_panId = panId;
}

uint16_t
UwbMacHeader::GetPanId() const
{
    return m_panId;
}

void
UwbMacHeader::SetDestination(Mac16Address address)
{
    m_destination = address;
}

Mac16Address
UwbMacHeader::GetDestination() const
{
    return m_destination;
}

void
UwbMacHeader::SetSource(Mac16Address address)
{
    m_source = address;
}

Mac16Address
UwbMacHeader::GetSource() const
{
    return m_source;
}

void
UwbMacHeader::SetAckRequest(bool request)
{
    m_ackRequest = request;
}

bool
UwbMacHeader::GetAckRequest() const
{
    return m_ackRequest;
}

/* -------------------------------------------------------------------------------------------- */
/* Frame check sequence                                                                           */
/* -------------------------------------------------------------------------------------------- */

NS_OBJECT_ENSURE_REGISTERED(UwbFcsTrailer);

UwbFcsTrailer::UwbFcsTrailer()
    : m_fcs(0)
{
}

TypeId
UwbFcsTrailer::GetTypeId()
{
    static TypeId tid = TypeId("ns3::uwb::UwbFcsTrailer")
                            .SetParent<Trailer>()
                            .SetGroupName("Uwb")
                            .AddConstructor<UwbFcsTrailer>();
    return tid;
}

TypeId
UwbFcsTrailer::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UwbFcsTrailer::GetSerializedSize() const
{
    return FCS_OCTETS;
}

void
UwbFcsTrailer::Serialize(Buffer::Iterator start) const
{
    start.Prev(FCS_OCTETS);
    start.WriteHtolsbU16(m_fcs);
}

uint32_t
UwbFcsTrailer::Deserialize(Buffer::Iterator start)
{
    start.Prev(FCS_OCTETS);
    m_fcs = start.ReadLsbtohU16();
    return FCS_OCTETS;
}

void
UwbFcsTrailer::Print(std::ostream& os) const
{
    os << "fcs=" << m_fcs;
}

void
UwbFcsTrailer::SetFcs(uint16_t fcs)
{
    m_fcs = fcs;
}

uint16_t
UwbFcsTrailer::GetFcs() const
{
    return m_fcs;
}

void
UwbFcsTrailer::CalculateFcs(Ptr<const Packet> packet)
{
    std::vector<uint8_t> buffer(packet->GetSize());
    packet->CopyData(buffer.data(), buffer.size());
    m_fcs = Fcs16(buffer);
}

bool
UwbFcsTrailer::CheckFcs(Ptr<const Packet> packet) const
{
    std::vector<uint8_t> buffer(packet->GetSize());
    packet->CopyData(buffer.data(), buffer.size());
    return Fcs16(buffer) == m_fcs;
}

/* -------------------------------------------------------------------------------------------- */
/* Ranging messages                                                                               */
/* -------------------------------------------------------------------------------------------- */

const char*
UwbRangingMessageName(UwbRangingMessage message)
{
    switch (message)
    {
    case UwbRangingMessage::POLL:
        return "POLL";
    case UwbRangingMessage::RESPONSE:
        return "RESPONSE";
    case UwbRangingMessage::FINAL:
        return "FINAL";
    case UwbRangingMessage::REPORT:
        return "REPORT";
    case UwbRangingMessage::BLINK:
        return "BLINK";
    default:
        return "invalid";
    }
}

NS_OBJECT_ENSURE_REGISTERED(UwbRangingHeader);

UwbRangingHeader::UwbRangingHeader()
    : m_message(UwbRangingMessage::POLL),
      m_session(0),
      m_method(UwbRangingMethod::DS_TWR),
      m_pollTx(0),
      m_pollRx(0),
      m_responseTx(0),
      m_responseRx(0),
      m_finalTx(0),
      m_rangeMillimetres(0)
{
}

TypeId
UwbRangingHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::uwb::UwbRangingHeader")
                            .SetParent<Header>()
                            .SetGroupName("Uwb")
                            .AddConstructor<UwbRangingHeader>();
    return tid;
}

TypeId
UwbRangingHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
UwbRangingHeader::GetSerializedSize() const
{
    // the message type, the exchange identifier and the method, then whatever the other end
    // needs to finish its share of the calculation
    uint32_t size = 3;
    switch (m_message)
    {
    case UwbRangingMessage::RESPONSE:
        size += 2 * TIMESTAMP_OCTETS;
        break;
    case UwbRangingMessage::FINAL:
        size += 3 * TIMESTAMP_OCTETS;
        break;
    case UwbRangingMessage::REPORT:
        size += 4;
        break;
    default:
        break;
    }
    return size;
}

void
UwbRangingHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU8(static_cast<uint8_t>(m_message));
    start.WriteU8(m_session);
    start.WriteU8(static_cast<uint8_t>(m_method));
    switch (m_message)
    {
    case UwbRangingMessage::RESPONSE:
        WriteTimestamp(start, m_pollRx);
        WriteTimestamp(start, m_responseTx);
        break;
    case UwbRangingMessage::FINAL:
        WriteTimestamp(start, m_pollTx);
        WriteTimestamp(start, m_responseRx);
        WriteTimestamp(start, m_finalTx);
        break;
    case UwbRangingMessage::REPORT:
        start.WriteHtolsbU32(static_cast<uint32_t>(m_rangeMillimetres));
        break;
    default:
        break;
    }
}

uint32_t
UwbRangingHeader::Deserialize(Buffer::Iterator start)
{
    m_message = static_cast<UwbRangingMessage>(start.ReadU8());
    m_session = start.ReadU8();
    m_method = static_cast<UwbRangingMethod>(start.ReadU8());
    switch (m_message)
    {
    case UwbRangingMessage::RESPONSE:
        m_pollRx = ReadTimestamp(start);
        m_responseTx = ReadTimestamp(start);
        break;
    case UwbRangingMessage::FINAL:
        m_pollTx = ReadTimestamp(start);
        m_responseRx = ReadTimestamp(start);
        m_finalTx = ReadTimestamp(start);
        break;
    case UwbRangingMessage::REPORT:
        m_rangeMillimetres = static_cast<int32_t>(start.ReadLsbtohU32());
        break;
    default:
        break;
    }
    return GetSerializedSize();
}

void
UwbRangingHeader::Print(std::ostream& os) const
{
    os << UwbRangingMessageName(m_message) << " session=" << +m_session << " "
       << UwbRangingMethodName(m_method);
    if (m_message == UwbRangingMessage::REPORT)
    {
        os << " range=" << m_rangeMillimetres << " mm";
    }
}

void
UwbRangingHeader::SetMessage(UwbRangingMessage message)
{
    m_message = message;
}

UwbRangingMessage
UwbRangingHeader::GetMessage() const
{
    return m_message;
}

void
UwbRangingHeader::SetSession(uint8_t session)
{
    m_session = session;
}

uint8_t
UwbRangingHeader::GetSession() const
{
    return m_session;
}

void
UwbRangingHeader::SetMethod(UwbRangingMethod method)
{
    m_method = method;
}

UwbRangingMethod
UwbRangingHeader::GetMethod() const
{
    return m_method;
}

void
UwbRangingHeader::SetResponderTimestamps(uint64_t pollRx, uint64_t responseTx)
{
    m_pollRx = pollRx;
    m_responseTx = responseTx;
}

void
UwbRangingHeader::SetInitiatorTimestamps(uint64_t pollTx, uint64_t responseRx, uint64_t finalTx)
{
    m_pollTx = pollTx;
    m_responseRx = responseRx;
    m_finalTx = finalTx;
}

uint64_t
UwbRangingHeader::GetPollRx() const
{
    return m_pollRx;
}

uint64_t
UwbRangingHeader::GetResponseTx() const
{
    return m_responseTx;
}

uint64_t
UwbRangingHeader::GetPollTx() const
{
    return m_pollTx;
}

uint64_t
UwbRangingHeader::GetResponseRx() const
{
    return m_responseRx;
}

uint64_t
UwbRangingHeader::GetFinalTx() const
{
    return m_finalTx;
}

void
UwbRangingHeader::SetRange(double metres)
{
    m_rangeMillimetres = static_cast<int32_t>(std::lround(metres * 1000.0));
}

double
UwbRangingHeader::GetRange() const
{
    return m_rangeMillimetres / 1000.0;
}

} // namespace uwb
} // namespace ns3
