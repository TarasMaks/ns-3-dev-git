/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_LINK_LAYER_H
#define BLE_LINK_LAYER_H

#include "ble-constants.h"
#include "ble-headers.h"
#include "ble-phy.h"
#include "ble-utils.h"

#include "ns3/event-id.h"
#include "ns3/mac48-address.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/traced-callback.h"

#include <deque>
#include <functional>
#include <optional>

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * The states of the Link Layer state machine (Vol 6, Part B, Section 1.1).
 */
enum class BleLinkState : uint8_t
{
    STANDBY = 0, //!< neither transmitting nor receiving
    ADVERTISING, //!< sending advertising packets
    SCANNING,    //!< listening for advertising packets
    INITIATING,  //!< listening for a specific advertiser in order to connect to it
    CONNECTION,  //!< in a connection, as the central or as the peripheral
};

/**
 * @ingroup ble
 * The role a device plays once it is in a connection.
 */
enum class BleRole : uint8_t
{
    NONE = 0,   //!< not in a connection
    CENTRAL,    //!< started the connection and owns its timing
    PERIPHERAL, //!< accepted the connection and follows its timing
};

/**
 * @ingroup ble
 * @param state a Link Layer state
 * @return a printable name
 */
const char* BleLinkStateName(BleLinkState state);

/**
 * @ingroup ble
 * @param role a connection role
 * @return a printable name
 */
const char* BleRoleName(BleRole role);

/**
 * @ingroup ble
 * The parameters and the running state of a connection (Vol 6, Part B, Section 4.5).
 */
struct BleConnection
{
    uint32_t accessAddress{0};               //!< access address, which identifies the connection
    uint32_t crcInit{0};                     //!< initial value of the CRC
    Mac48Address peer;                       //!< address of the other device
    Time interval{MilliSeconds(50)};         //!< time between two anchor points
    uint16_t latency{0};                     //!< number of events the peripheral may skip
    Time supervisionTimeout{Seconds(5)};     //!< time after which the connection is considered lost
    BleChannelMap channelMap;                //!< the data channels the connection may use
    uint8_t hopIncrement{HOP_INCREMENT_MIN}; //!< hop increment of channel selection algorithm #1
    bool useCsa2{true};                      //!< whether channel selection algorithm #2 is used
    uint16_t eventCounter{0};                //!< counter of the connection events
    bool sn{false};            //!< sequence number of the next packet this device sends
    bool nesn{false};          //!< sequence number this device expects to receive next
    Time lastAnchor;           //!< anchor point of the current connection event
    Time lastPacketReceived;   //!< when a packet was last received, for the supervision timeout
    bool established{false};   //!< whether the first packet has been exchanged
    uint8_t currentChannel{0}; //!< the data channel of the current connection event
};

/**
 * @ingroup ble
 * A report of an advertising packet seen by a scanner.
 */
struct BleAdvReport
{
    Mac48Address address;                       //!< the address of the advertiser
    BleAdvPduType type{BleAdvPduType::ADV_IND}; //!< the type of the advertising packet
    double rssiDbm{0};                          //!< the power the packet was received with
    uint8_t channel{0};                         //!< the advertising channel it was received on
    uint32_t advDataLength{0}; //!< the number of octets of advertising data it carried
    bool scanResponse{false};  //!< whether it is a scan response
};

/**
 * @ingroup ble
 * The Link Layer of a BLE device.
 *
 * It implements the state machine of Vol 6, Part B: advertising events on the three primary
 * advertising channels with the random delay the specification mandates, passive and active
 * scanning with a duty cycle set by a scan window inside a scan interval, connection
 * establishment through CONNECT_IND, and connections whose events follow anchor points one
 * connection interval apart, hopping over the data channels with either channel selection
 * algorithm.
 *
 * Inside a connection the Link Layer runs the one-bit stop and wait protocol of the
 * specification: every packet carries a sequence number and the sequence number its sender
 * expects next, a packet is retransmitted until it is acknowledged, and the more data bit
 * extends a connection event while either side still has something to send.
 *
 * Payloads handed in by the upper layer are prefixed with an L2CAP basic header and fragmented
 * over as many data channel PDUs as needed; the receiver reassembles them.
 */
class BleLinkLayer : public Object
{
  public:
    /// Signature of the callback that delivers a reassembled payload to the upper layer.
    using ReceiveCallback = Callback<void, Ptr<Packet>, Mac48Address>;

    /// @return the TypeId
    static TypeId GetTypeId();

    BleLinkLayer();
    ~BleLinkLayer() override;

    /**
     * @param phy the physical layer this Link Layer drives
     */
    void SetPhy(Ptr<BlePhy> phy);
    /// @return the physical layer
    Ptr<BlePhy> GetPhy() const;

    /**
     * @param address the device address of this device
     */
    void SetAddress(Mac48Address address);
    /// @return the device address of this device
    Mac48Address GetAddress() const;

    /// @return the state of the Link Layer
    BleLinkState GetState() const;
    /// @return the role played in the current connection
    BleRole GetRole() const;
    /// @return true if a connection is established
    bool IsConnected() const;
    /// @return the current connection, if there is one
    std::optional<BleConnection> GetConnection() const;

    /// Start sending advertising events on the enabled advertising channels.
    void StartAdvertising();
    /// Stop sending advertising events and return to standby.
    void StopAdvertising();

    /// Start listening for advertising packets.
    void StartScanning();
    /// Stop listening for advertising packets and return to standby.
    void StopScanning();

    /**
     * Listen for a connectable advertiser and connect to it.
     * @param peer the address of the advertiser, or the broadcast address to accept any
     */
    void StartInitiating(Mac48Address peer = Mac48Address::GetBroadcast());
    /// Stop initiating and return to standby.
    void StopInitiating();

    /// Terminate the current connection.
    void Disconnect();

    /**
     * Replace the channel map of the current connection, which is how adaptive frequency hopping
     * removes the data channels that a persistent interferer occupies. Both ends of a connection
     * must be given the same map.
     *
     * @param map the channel map, which must leave at least two channels usable
     * @return true if the map was applied
     */
    bool SetChannelMap(const BleChannelMap& map);

    /**
     * Hand a payload to the Link Layer for transmission over the connection.
     *
     * @param packet the payload
     * @param destination the address of the peer, which must be the peer of the connection
     * @return true if the payload was queued
     */
    bool Enqueue(Ptr<Packet> packet, Mac48Address destination);

    /// @return the number of payloads waiting to be sent
    uint32_t GetQueueSize() const;

    /**
     * @param callback invoked with every reassembled payload and the address it came from
     */
    void SetReceiveCallback(ReceiveCallback callback);

    /**
     * Establish a connection between two Link Layers without going through advertising, so that
     * studies of the connection itself do not have to wait for the discovery procedure.
     *
     * @param central the Link Layer that becomes the central
     * @param peripheral the Link Layer that becomes the peripheral
     * @param interval the connection interval
     * @param start when the first anchor point occurs
     */
    static void SetupStaticConnection(Ptr<BleLinkLayer> central,
                                      Ptr<BleLinkLayer> peripheral,
                                      Time interval,
                                      Time start);

    /**
     * Assign the streams of the random variables of this object.
     * @param stream the first stream index to use
     * @return the number of streams used
     */
    int64_t AssignStreams(int64_t stream);

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    /* ---------------------------------------------------------------------------------------- */
    /* Advertising                                                                              */
    /* ---------------------------------------------------------------------------------------- */

    /// Start an advertising event on the first enabled advertising channel.
    void StartAdvertisingEvent();
    /// Send the advertising packet of the current advertising channel.
    void SendAdvertisingPacket();
    /// Open the window that catches a scan request or a connection request.
    void OpenAdvertisingResponseWindow();
    /// Close that window and move on to the next advertising channel.
    void CloseAdvertisingResponseWindow();
    /// Move to the next advertising channel, or schedule the next advertising event.
    void NextAdvertisingChannel();

    /* ---------------------------------------------------------------------------------------- */
    /* Scanning and initiating                                                                  */
    /* ---------------------------------------------------------------------------------------- */

    /// Open a scan window on the current advertising channel.
    void OpenScanWindow();
    /// Close the scan window and schedule the next one.
    void CloseScanWindow();
    /// Send a scan request to the advertiser that was just heard.
    void SendScanRequest();
    /// Send a connection request to the advertiser that was just heard.
    void SendConnectRequest();

    /* ---------------------------------------------------------------------------------------- */
    /* Connections                                                                              */
    /* ---------------------------------------------------------------------------------------- */

    /// Begin a connection event, as the central or as the peripheral.
    void StartConnectionEvent();
    /// Send the next data channel PDU of the current connection event.
    void SendConnectionPdu();
    /// Open the window that catches the answer of the peer.
    void OpenConnectionRxWindow();
    /// Give up on the current connection event because nothing was received.
    void OnConnectionRxTimeout();
    /// Close the current connection event and schedule the next anchor point.
    void EndConnectionEvent();
    /// Declare the connection lost because nothing was received for the supervision timeout.
    void OnSupervisionTimeout();
    /// @return the data channel of the current connection event
    uint8_t SelectConnectionChannel();

    /**
     * Build the data channel PDU to send next: the pending fragment if one is waiting to be
     * acknowledged, the next fragment of the queue, or an empty PDU.
     * @return the PDU
     */
    Ptr<Packet> BuildDataPdu();

    /**
     * Apply the acknowledgement and sequencing rules of the specification to a received PDU.
     * @param header the header of the received PDU
     * @return true if the PDU carries data that has not been seen before
     */
    bool ProcessSequenceNumbers(const BleDataHeader& header);

    /**
     * Reassemble a received fragment into the payload it belongs to.
     * @param fragment the payload of the received PDU
     * @param header the header of the received PDU
     */
    void ReassembleFragment(Ptr<Packet> fragment, const BleDataHeader& header);

    /// Take the next payload out of the queue and prepare its fragments.
    void PrepareNextSdu();

    /* ---------------------------------------------------------------------------------------- */
    /* Physical layer interface                                                                 */
    /* ---------------------------------------------------------------------------------------- */

    /**
     * Invoked by the PHY when a packet is received without error.
     * @param packet the packet
     * @param rssiDbm the power it was received with
     * @param channel the channel it was received on
     */
    void OnPhyReceiveOk(Ptr<Packet> packet, double rssiDbm, uint8_t channel);

    /**
     * Invoked by the PHY when a reception fails.
     * @param packet the packet
     * @param rssiDbm the power it was received with
     */
    void OnPhyReceiveError(Ptr<const Packet> packet, double rssiDbm);

    /// Invoked by the PHY when a transmission ends.
    void OnPhyTxEnd();

    /**
     * Handle an advertising channel packet.
     * @param packet the packet, without its CRC
     * @param rssiDbm the power it was received with
     * @param channel the channel it was received on
     */
    void HandleAdvertisingPacket(Ptr<Packet> packet, double rssiDbm, uint8_t channel);

    /**
     * Handle a data channel packet.
     * @param packet the packet, without its CRC
     * @param rssiDbm the power it was received with
     */
    void HandleDataPacket(Ptr<Packet> packet, double rssiDbm);

    /**
     * Append the CRC of a packet, computed over the PDU with the initial value of the current
     * context.
     * @param packet the packet to terminate
     * @param crcInit the initial value of the CRC register
     */
    void AppendCrc(Ptr<Packet> packet, uint32_t crcInit) const;

    /**
     * Transmit a packet and remember what to do once the transmission ends.
     * @param packet the packet
     * @param afterTx the action to take when the transmission ends
     */
    void Transmit(Ptr<Packet> packet, std::function<void()> afterTx);

    /// Put the radio back to standby and cancel every pending event.
    void ResetRadio();

    /**
     * Whether the PHY has locked onto a packet that is still arriving. A receive window must not
     * be closed while that is the case, because a real receiver that has synchronised on a
     * preamble stays on the channel until the packet ends.
     *
     * @return true if a reception is in progress
     */
    bool IsPhyReceiving() const;

    Ptr<BlePhy> m_phy;                         //!< the physical layer
    Mac48Address m_address;                    //!< the device address
    BleLinkState m_state;                      //!< the state of the Link Layer
    BleRole m_role;                            //!< the role in the current connection
    std::optional<BleConnection> m_connection; //!< the current connection

    // advertising
    Time m_advInterval;                 //!< time between advertising events
    BleAdvPduType m_advType;            //!< the type of advertising packet sent
    uint8_t m_advDataLength;            //!< octets of advertising data carried
    bool m_advChannel37;                //!< whether advertising channel 37 is used
    bool m_advChannel38;                //!< whether advertising channel 38 is used
    bool m_advChannel39;                //!< whether advertising channel 39 is used
    std::vector<uint8_t> m_advChannels; //!< the enabled advertising channels
    std::size_t m_advChannelIndex;      //!< the channel of the current advertising event

    // scanning and initiating
    Time m_scanInterval;            //!< time between the starts of two scan windows
    Time m_scanWindow;              //!< duration of a scan window
    bool m_activeScanning;          //!< whether scan requests are sent
    uint8_t m_scanChannelIndex;     //!< the advertising channel currently scanned
    Mac48Address m_initiatorTarget; //!< the advertiser to connect to
    Mac48Address m_pendingPeer;     //!< the advertiser that was just heard

    // connection parameters used when this device is the initiator
    Time m_connInterval;           //!< the connection interval requested
    uint16_t m_connLatency;        //!< the peripheral latency requested
    Time m_connSupervisionTimeout; //!< the supervision timeout requested
    bool m_useCsa2;                //!< whether channel selection algorithm #2 is requested

    // data path
    uint32_t m_maxQueueSize; //!< how many payloads may wait
    uint8_t m_maxPduPayload; //!< the largest data channel PDU payload
    std::deque<std::pair<Ptr<Packet>, Mac48Address>> m_queue; //!< payloads waiting to be sent
    /// fragments of the payload being sent, each flagged as starting a payload or continuing it
    std::deque<std::pair<Ptr<Packet>, bool>> m_fragments;
    Ptr<Packet> m_pendingFragment;  //!< the fragment waiting to be acknowledged
    bool m_pendingIsStart;          //!< whether that fragment starts a payload
    Ptr<Packet> m_reassembly;       //!< the payload being reassembled
    uint32_t m_reassemblyRemaining; //!< octets still missing from it

    // running state of a connection event
    bool m_eventOpen;         //!< whether a connection event is in progress
    bool m_peerMoreData;      //!< whether the peer announced more data
    bool m_localMoreData;     //!< whether this device announced more data
    uint32_t m_eventPdus;     //!< PDUs exchanged in the current connection event
    uint16_t m_skippedEvents; //!< events the peripheral has skipped in a row

    // scheduling
    EventId m_nextEvent;             //!< the next advertising event, scan window or anchor point
    EventId m_rxTimeout;             //!< the timeout of the receive window currently open
    EventId m_supervision;           //!< the supervision timeout of the connection
    std::function<void()> m_afterTx; //!< what to do when the current transmission ends

    Ptr<UniformRandomVariable> m_random; //!< draws advertising delays and access addresses
    ReceiveCallback m_receiveCallback;   //!< delivers payloads to the upper layer

    /// Traced when an advertising packet is sent, with its type and channel.
    TracedCallback<Mac48Address, uint8_t, uint8_t> m_advSentTrace;
    /// Traced when an advertising packet is received by a scanner.
    TracedCallback<BleAdvReport> m_advReportTrace;
    /// Traced when a connection is established, with the peer and the role.
    TracedCallback<Mac48Address, uint8_t> m_connectionEstablishedTrace;
    /// Traced when a connection ends, with the peer and the reason.
    TracedCallback<Mac48Address, std::string> m_connectionClosedTrace;
    /// Traced at the start of a connection event, with its counter and its channel.
    TracedCallback<uint16_t, uint8_t> m_connectionEventTrace;
    /// Traced when a data channel PDU is sent, with its payload length and whether it is a
    /// retransmission.
    TracedCallback<uint8_t, bool> m_pduSentTrace;
    /// Traced when a payload is delivered to the upper layer.
    TracedCallback<Ptr<const Packet>, Mac48Address> m_deliveredTrace;
};

} // namespace ble
} // namespace ns3

#endif /* BLE_LINK_LAYER_H */
