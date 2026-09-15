/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_HEADERS_H
#define UWB_HEADERS_H

#include "uwb-constants.h"

#include "ns3/header.h"
#include "ns3/mac16-address.h"
#include "ns3/ptr.h"
#include "ns3/trailer.h"

namespace ns3
{

class Packet;

namespace uwb
{

/**
 * @ingroup uwb
 * The frame types of IEEE Std 802.15.4 this model uses.
 */
enum class UwbFrameType : uint8_t
{
    BEACON = 0, //!< a beacon
    DATA = 1,   //!< a data frame
    ACK = 2,    //!< an acknowledgement
    COMMAND = 3 //!< a MAC command, which carries the ranging messages of this model
};

/**
 * @ingroup uwb
 * @param os the stream
 * @param type a frame type
 * @return the stream
 */
std::ostream& operator<<(std::ostream& os, UwbFrameType type);

/**
 * @ingroup uwb
 * The MAC header of IEEE Std 802.15.4, in the shape this model uses: short addresses, one PAN
 * identifier shared by both ends, and no security or information elements.
 *
 * Nine octets, which against a 127 octet frame is small, but against the 12 octet payload of a
 * ranging message it is most of the frame. That matters for UWB, because the air time of a
 * ranging exchange is what sets how many tags a channel can serve.
 *
 * The bit that marks a frame as one to be timestamped is not here. In UWB it lives in the
 * physical layer header, which is why this model carries it in the signal parameters rather
 * than in the MAC header.
 */
class UwbMacHeader : public Header
{
  public:
    UwbMacHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /// @param type the frame type
    void SetFrameType(UwbFrameType type);
    /// @return the frame type
    UwbFrameType GetFrameType() const;

    /// @param sequence the sequence number
    void SetSequenceNumber(uint8_t sequence);
    /// @return the sequence number
    uint8_t GetSequenceNumber() const;

    /// @param panId the identifier of the personal area network
    void SetPanId(uint16_t panId);
    /// @return the identifier of the personal area network
    uint16_t GetPanId() const;

    /// @param address the destination
    void SetDestination(Mac16Address address);
    /// @return the destination
    Mac16Address GetDestination() const;

    /// @param address the source
    void SetSource(Mac16Address address);
    /// @return the source
    Mac16Address GetSource() const;

    /// @param request whether the sender wants the frame acknowledged
    void SetAckRequest(bool request);
    /// @return whether the sender wants the frame acknowledged
    bool GetAckRequest() const;

  private:
    UwbFrameType m_frameType;    //!< the frame type
    bool m_ackRequest;           //!< whether an acknowledgement is asked for
    uint8_t m_sequenceNumber;    //!< the sequence number
    uint16_t m_panId;            //!< the personal area network
    Mac16Address m_destination;  //!< the destination
    Mac16Address m_source;       //!< the source
};

/**
 * @ingroup uwb
 * The frame check sequence of IEEE Std 802.15.4, two octets at the end of every frame.
 */
class UwbFcsTrailer : public Trailer
{
  public:
    UwbFcsTrailer();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /// @param fcs the frame check sequence
    void SetFcs(uint16_t fcs);
    /// @return the frame check sequence
    uint16_t GetFcs() const;

    /**
     * Compute the frame check sequence over a packet and store it.
     *
     * @param packet the packet the trailer will be added to
     */
    void CalculateFcs(Ptr<const Packet> packet);

    /**
     * Check the stored frame check sequence against a packet.
     *
     * @param packet the packet the trailer was removed from
     * @return true if they agree
     */
    bool CheckFcs(Ptr<const Packet> packet) const;

  private:
    uint16_t m_fcs; //!< the frame check sequence
};

/**
 * @ingroup uwb
 * The messages of a ranging exchange.
 */
enum class UwbRangingMessage : uint8_t
{
    POLL = 0, //!< the initiator opens an exchange
    RESPONSE, //!< the responder answers, carrying the two timestamps it took
    FINAL,    //!< the initiator closes a double-sided exchange, carrying its three timestamps
    REPORT,   //!< the device that computed the range tells the other one what it found
    BLINK     //!< a tag announces itself for the anchors to timestamp
};

/**
 * @ingroup uwb
 * @param message a ranging message type
 * @return a printable name
 */
const char* UwbRangingMessageName(UwbRangingMessage message);

/**
 * @ingroup uwb
 * @param os the stream
 * @param message a ranging message type
 * @return the stream
 */
std::ostream& operator<<(std::ostream& os, UwbRangingMessage message);

/**
 * @ingroup uwb
 * The payload of a ranging message.
 *
 * Timestamps travel as forty bit counter readings, five octets each, exactly as the hardware
 * counter holds them, wrap included. A responder that put a sixty-four bit number on the air
 * would be inventing precision its own radio does not have, and code written against this
 * header keeps working when the counter turns over.
 *
 * Which timestamps a message carries depends on what the other end needs to finish the
 * calculation, so the header is not a fixed size: a poll carries none, a response carries the
 * two readings the responder took, and a final message carries the three the initiator took.
 */
class UwbRangingHeader : public Header
{
  public:
    UwbRangingHeader();

    /// @return the TypeId
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    void Print(std::ostream& os) const override;

    /// @param message the message type
    void SetMessage(UwbRangingMessage message);
    /// @return the message type
    UwbRangingMessage GetMessage() const;

    /// @param session the identifier of the exchange
    void SetSession(uint8_t session);
    /// @return the identifier of the exchange
    uint8_t GetSession() const;

    /// @param method the ranging method the exchange uses
    void SetMethod(UwbRangingMethod method);
    /// @return the ranging method the exchange uses
    UwbRangingMethod GetMethod() const;

    /**
     * Store the two readings a responder takes.
     *
     * @param pollRx when the poll arrived
     * @param responseTx when the response left
     */
    void SetResponderTimestamps(uint64_t pollRx, uint64_t responseTx);

    /**
     * Store the three readings an initiator takes.
     *
     * @param pollTx when the poll left
     * @param responseRx when the response arrived
     * @param finalTx when the final message left
     */
    void SetInitiatorTimestamps(uint64_t pollTx, uint64_t responseRx, uint64_t finalTx);

    /// @return when the poll arrived at the responder
    uint64_t GetPollRx() const;
    /// @return when the response left the responder
    uint64_t GetResponseTx() const;
    /// @return when the poll left the initiator
    uint64_t GetPollTx() const;
    /// @return when the response arrived at the initiator
    uint64_t GetResponseRx() const;
    /// @return when the final message left the initiator
    uint64_t GetFinalTx() const;

    /**
     * Store a range that has already been computed.
     *
     * @param metres the range, in metres
     */
    void SetRange(double metres);

    /// @return the range that was computed, in metres
    double GetRange() const;

  private:
    UwbRangingMessage m_message; //!< the message type
    uint8_t m_session;           //!< the identifier of the exchange
    UwbRangingMethod m_method;   //!< the ranging method
    uint64_t m_pollTx;           //!< initiator, sends the poll
    uint64_t m_pollRx;           //!< responder, receives the poll
    uint64_t m_responseTx;       //!< responder, sends the response
    uint64_t m_responseRx;       //!< initiator, receives the response
    uint64_t m_finalTx;          //!< initiator, sends the final message
    int32_t m_rangeMillimetres;  //!< a range that has already been computed
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_HEADERS_H */
