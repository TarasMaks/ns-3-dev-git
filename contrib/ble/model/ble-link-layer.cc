/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-link-layer.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleLinkLayer");

namespace ble
{

NS_OBJECT_ENSURE_REGISTERED(BleLinkLayer);

const char*
BleLinkStateName(BleLinkState state)
{
    switch (state)
    {
    case BleLinkState::STANDBY:
        return "STANDBY";
    case BleLinkState::ADVERTISING:
        return "ADVERTISING";
    case BleLinkState::SCANNING:
        return "SCANNING";
    case BleLinkState::INITIATING:
        return "INITIATING";
    case BleLinkState::CONNECTION:
        return "CONNECTION";
    default:
        return "invalid";
    }
}

const char*
BleRoleName(BleRole role)
{
    switch (role)
    {
    case BleRole::NONE:
        return "NONE";
    case BleRole::CENTRAL:
        return "CENTRAL";
    case BleRole::PERIPHERAL:
        return "PERIPHERAL";
    default:
        return "invalid";
    }
}

namespace
{
/// How long a receiver waits past the inter frame space before giving up on an answer.
constexpr uint32_t RX_WINDOW_SLACK_US = 100;
/// How long a peripheral opens its receive window around an anchor point, for clock drift.
constexpr uint32_t ANCHOR_WINDOW_US = 500;
} // namespace

TypeId
BleLinkLayer::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ble::BleLinkLayer")
            .SetParent<Object>()
            .SetGroupName("Ble")
            .AddConstructor<BleLinkLayer>()
            .AddAttribute("AdvInterval",
                          "Time between two advertising events, before the random delay the "
                          "specification adds to each of them.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&BleLinkLayer::m_advInterval),
                          MakeTimeChecker(MicroSeconds(ADV_INTERVAL_MIN_US),
                                          MicroSeconds(ADV_INTERVAL_MAX_US)))
            .AddAttribute("AdvType",
                          "The type of advertising packet sent in an advertising event.",
                          EnumValue(BleAdvPduType::ADV_IND),
                          MakeEnumAccessor<BleAdvPduType>(&BleLinkLayer::m_advType),
                          MakeEnumChecker(BleAdvPduType::ADV_IND,
                                          "ADV_IND",
                                          BleAdvPduType::ADV_NONCONN_IND,
                                          "ADV_NONCONN_IND",
                                          BleAdvPduType::ADV_SCAN_IND,
                                          "ADV_SCAN_IND"))
            .AddAttribute("AdvDataLength",
                          "Octets of advertising data carried by each advertising packet.",
                          UintegerValue(21),
                          MakeUintegerAccessor(&BleLinkLayer::m_advDataLength),
                          MakeUintegerChecker<uint8_t>(0, ADV_PDU_PAYLOAD_MAX - BD_ADDR_OCTETS))
            .AddAttribute("AdvChannel37",
                          "Whether advertising channel 37 is used.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&BleLinkLayer::m_advChannel37),
                          MakeBooleanChecker())
            .AddAttribute("AdvChannel38",
                          "Whether advertising channel 38 is used.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&BleLinkLayer::m_advChannel38),
                          MakeBooleanChecker())
            .AddAttribute("AdvChannel39",
                          "Whether advertising channel 39 is used.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&BleLinkLayer::m_advChannel39),
                          MakeBooleanChecker())
            .AddAttribute("ScanInterval",
                          "Time between the starts of two scan windows.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&BleLinkLayer::m_scanInterval),
                          MakeTimeChecker(MicroSeconds(2500)))
            .AddAttribute("ScanWindow",
                          "Duration a scanner listens inside each scan interval.",
                          TimeValue(MilliSeconds(50)),
                          MakeTimeAccessor(&BleLinkLayer::m_scanWindow),
                          MakeTimeChecker(MicroSeconds(2500)))
            .AddAttribute("ActiveScanning",
                          "Whether a scanner answers scannable advertising packets with a scan "
                          "request in order to obtain a scan response.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&BleLinkLayer::m_activeScanning),
                          MakeBooleanChecker())
            .AddAttribute(
                "ConnInterval",
                "The connection interval requested when this device initiates a "
                "connection. It is rounded to a multiple of 1.25 ms.",
                TimeValue(MilliSeconds(50)),
                MakeTimeAccessor(&BleLinkLayer::m_connInterval),
                MakeTimeChecker(MicroSeconds(CONN_INTERVAL_MIN_UNITS * CONN_INTERVAL_UNIT_US),
                                MicroSeconds(CONN_INTERVAL_MAX_UNITS * CONN_INTERVAL_UNIT_US)))
            .AddAttribute("PeripheralLatency",
                          "How many connection events a peripheral with nothing to send may skip.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&BleLinkLayer::m_connLatency),
                          MakeUintegerChecker<uint16_t>(0, PERIPHERAL_LATENCY_MAX))
            .AddAttribute("SupervisionTimeout",
                          "Time without a received packet after which a connection is lost.",
                          TimeValue(Seconds(5)),
                          MakeTimeAccessor(&BleLinkLayer::m_connSupervisionTimeout),
                          MakeTimeChecker(MilliSeconds(100), Seconds(32)))
            .AddAttribute("UseCsa2",
                          "Whether channel selection algorithm #2 is used by the connections "
                          "this device initiates.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&BleLinkLayer::m_useCsa2),
                          MakeBooleanChecker())
            .AddAttribute(
                "MaxPduPayload",
                "The largest data channel PDU payload, 27 octets without the Data "
                "Length Extension and up to 251 with it.",
                UintegerValue(DATA_PDU_PAYLOAD_MAX),
                MakeUintegerAccessor(&BleLinkLayer::m_maxPduPayload),
                MakeUintegerChecker<uint8_t>(DATA_PDU_PAYLOAD_MIN_MAX, DATA_PDU_PAYLOAD_MAX))
            .AddAttribute("QueueSize",
                          "How many payloads may wait to be sent over the connection.",
                          UintegerValue(100),
                          MakeUintegerAccessor(&BleLinkLayer::m_maxQueueSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddTraceSource("AdvSent",
                            "An advertising packet was sent.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_advSentTrace),
                            "ns3::ble::BleLinkLayer::AdvSentTracedCallback")
            .AddTraceSource("AdvReport",
                            "An advertising packet was received by a scanner.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_advReportTrace),
                            "ns3::ble::BleLinkLayer::AdvReportTracedCallback")
            .AddTraceSource("ConnectionEstablished",
                            "A connection was established.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_connectionEstablishedTrace),
                            "ns3::ble::BleLinkLayer::ConnectionTracedCallback")
            .AddTraceSource("ConnectionClosed",
                            "A connection ended.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_connectionClosedTrace),
                            "ns3::ble::BleLinkLayer::ConnectionClosedTracedCallback")
            .AddTraceSource("ConnectionEvent",
                            "A connection event started.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_connectionEventTrace),
                            "ns3::ble::BleLinkLayer::ConnectionEventTracedCallback")
            .AddTraceSource("PduSent",
                            "A data channel PDU was sent.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_pduSentTrace),
                            "ns3::ble::BleLinkLayer::PduSentTracedCallback")
            .AddTraceSource("Delivered",
                            "A payload was delivered to the upper layer.",
                            MakeTraceSourceAccessor(&BleLinkLayer::m_deliveredTrace),
                            "ns3::Packet::AddressTracedCallback");
    return tid;
}

BleLinkLayer::BleLinkLayer()
    : m_address(Mac48Address::GetBroadcast()),
      m_state(BleLinkState::STANDBY),
      m_role(BleRole::NONE),
      m_advInterval(MilliSeconds(100)),
      m_advType(BleAdvPduType::ADV_IND),
      m_advDataLength(21),
      m_advChannel37(true),
      m_advChannel38(true),
      m_advChannel39(true),
      m_advChannelIndex(0),
      m_scanInterval(MilliSeconds(100)),
      m_scanWindow(MilliSeconds(50)),
      m_activeScanning(false),
      m_scanChannelIndex(0),
      m_initiatorTarget(Mac48Address::GetBroadcast()),
      m_pendingPeer(Mac48Address::GetBroadcast()),
      m_connInterval(MilliSeconds(50)),
      m_connLatency(0),
      m_connSupervisionTimeout(Seconds(5)),
      m_useCsa2(true),
      m_maxQueueSize(100),
      m_maxPduPayload(DATA_PDU_PAYLOAD_MAX),
      m_pendingIsStart(false),
      m_reassemblyRemaining(0),
      m_eventOpen(false),
      m_peerMoreData(false),
      m_localMoreData(false),
      m_eventPdus(0),
      m_skippedEvents(0)
{
    NS_LOG_FUNCTION(this);
    m_random = CreateObject<UniformRandomVariable>();
}

BleLinkLayer::~BleLinkLayer()
{
    NS_LOG_FUNCTION(this);
}

void
BleLinkLayer::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    m_advChannels.clear();
    if (m_advChannel37)
    {
        m_advChannels.push_back(ADV_CHANNEL_37);
    }
    if (m_advChannel38)
    {
        m_advChannels.push_back(ADV_CHANNEL_38);
    }
    if (m_advChannel39)
    {
        m_advChannels.push_back(ADV_CHANNEL_39);
    }
    NS_ABORT_MSG_IF(m_advChannels.empty(), "At least one advertising channel must be enabled");
    if (m_phy)
    {
        m_phy->SetReceiveOkCallback(MakeCallback(&BleLinkLayer::OnPhyReceiveOk, this));
        m_phy->SetReceiveErrorCallback(MakeCallback(&BleLinkLayer::OnPhyReceiveError, this));
        m_phy->SetTxEndCallback(MakeCallback(&BleLinkLayer::OnPhyTxEnd, this));
    }
    Object::DoInitialize();
}

void
BleLinkLayer::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_nextEvent.Cancel();
    m_rxTimeout.Cancel();
    m_supervision.Cancel();
    m_phy = nullptr;
    m_random = nullptr;
    m_queue.clear();
    m_fragments.clear();
    m_pendingFragment = nullptr;
    m_reassembly = nullptr;
    m_receiveCallback.Nullify();
    m_afterTx = nullptr;
    Object::DoDispose();
}

void
BleLinkLayer::SetPhy(Ptr<BlePhy> phy)
{
    m_phy = phy;
    if (m_phy)
    {
        m_phy->SetReceiveOkCallback(MakeCallback(&BleLinkLayer::OnPhyReceiveOk, this));
        m_phy->SetReceiveErrorCallback(MakeCallback(&BleLinkLayer::OnPhyReceiveError, this));
        m_phy->SetTxEndCallback(MakeCallback(&BleLinkLayer::OnPhyTxEnd, this));
    }
}

Ptr<BlePhy>
BleLinkLayer::GetPhy() const
{
    return m_phy;
}

void
BleLinkLayer::SetAddress(Mac48Address address)
{
    m_address = address;
}

Mac48Address
BleLinkLayer::GetAddress() const
{
    return m_address;
}

BleLinkState
BleLinkLayer::GetState() const
{
    return m_state;
}

BleRole
BleLinkLayer::GetRole() const
{
    return m_role;
}

bool
BleLinkLayer::IsConnected() const
{
    return m_state == BleLinkState::CONNECTION && m_connection.has_value();
}

std::optional<BleConnection>
BleLinkLayer::GetConnection() const
{
    return m_connection;
}

void
BleLinkLayer::SetReceiveCallback(ReceiveCallback callback)
{
    m_receiveCallback = callback;
}

int64_t
BleLinkLayer::AssignStreams(int64_t stream)
{
    m_random->SetStream(stream);
    return 1;
}

void
BleLinkLayer::ResetRadio()
{
    m_nextEvent.Cancel();
    m_rxTimeout.Cancel();
    m_afterTx = nullptr;
    if (m_phy)
    {
        m_phy->SetRxEnabled(false);
    }
}

bool
BleLinkLayer::IsPhyReceiving() const
{
    return m_phy && m_phy->GetState() == BlePhyState::RX;
}

void
BleLinkLayer::AppendCrc(Ptr<Packet> packet, uint32_t crcInit) const
{
    const uint32_t size = packet->GetSize();
    std::vector<uint8_t> buffer(size);
    packet->CopyData(buffer.data(), size);
    BleCrcTrailer trailer;
    trailer.SetCrc(Crc24(crcInit, buffer));
    packet->AddTrailer(trailer);
}

void
BleLinkLayer::Transmit(Ptr<Packet> packet, std::function<void()> afterTx)
{
    NS_LOG_FUNCTION(this << packet->GetSize());
    m_afterTx = std::move(afterTx);
    m_phy->SetRxEnabled(false);
    if (!m_phy->StartTx(packet))
    {
        NS_LOG_WARN("The PHY refused a transmission, the action is dropped");
        m_afterTx = nullptr;
    }
}

void
BleLinkLayer::OnPhyTxEnd()
{
    NS_LOG_FUNCTION(this);
    auto action = m_afterTx;
    m_afterTx = nullptr;
    if (action)
    {
        action();
    }
}

/* -------------------------------------------------------------------------------------------- */
/* Advertising                                                                                    */
/* -------------------------------------------------------------------------------------------- */

void
BleLinkLayer::StartAdvertising()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_phy, "A PHY is required before advertising");
    NS_ABORT_MSG_IF(m_state == BleLinkState::CONNECTION,
                    "A connected device cannot advertise in this model");
    ResetRadio();
    m_state = BleLinkState::ADVERTISING;
    m_phy->SetAccessAddress(ADV_ACCESS_ADDRESS);
    // the first event is spread over one advertising interval so that devices started together
    // do not advertise in lockstep
    const Time first = MicroSeconds(m_random->GetValue(0.0, m_advInterval.GetMicroSeconds()));
    m_nextEvent = Simulator::Schedule(first, &BleLinkLayer::StartAdvertisingEvent, this);
}

void
BleLinkLayer::StopAdvertising()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    ResetRadio();
    m_state = BleLinkState::STANDBY;
}

void
BleLinkLayer::StartAdvertisingEvent()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    m_advChannelIndex = 0;
    SendAdvertisingPacket();
}

void
BleLinkLayer::SendAdvertisingPacket()
{
    NS_LOG_FUNCTION(this << m_advChannelIndex);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    const uint8_t channel = m_advChannels[m_advChannelIndex];
    m_phy->SetRfChannel(channel);
    m_phy->SetAccessAddress(ADV_ACCESS_ADDRESS);

    auto packet = Create<Packet>(m_advDataLength);
    BleAdvPayloadHeader payload;
    payload.SetAdvA(m_address);
    payload.SetAdvDataLength(m_advDataLength);
    packet->AddHeader(payload);
    BleAdvHeader header;
    header.SetPduType(m_advType);
    header.SetChSel(m_useCsa2);
    header.SetTxAdd(false);
    header.SetLength(static_cast<uint8_t>(BD_ADDR_OCTETS + m_advDataLength));
    packet->AddHeader(header);
    AppendCrc(packet, ADV_CRC_INIT);

    m_advSentTrace(m_address, static_cast<uint8_t>(m_advType), channel);
    const bool answerable =
        (m_advType == BleAdvPduType::ADV_IND || m_advType == BleAdvPduType::ADV_SCAN_IND);
    Transmit(packet, [this, answerable]() {
        if (answerable)
        {
            OpenAdvertisingResponseWindow();
        }
        else
        {
            NextAdvertisingChannel();
        }
    });
}

void
BleLinkLayer::OpenAdvertisingResponseWindow()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    m_phy->SetRxEnabled(true);
    m_rxTimeout = Simulator::Schedule(MicroSeconds(T_IFS_US + RX_WINDOW_SLACK_US),
                                      &BleLinkLayer::CloseAdvertisingResponseWindow,
                                      this);
}

void
BleLinkLayer::CloseAdvertisingResponseWindow()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    if (IsPhyReceiving())
    {
        // a packet is arriving, so the window stays open until it has been received
        m_rxTimeout =
            Simulator::Schedule(BlePhy::CalculateTxDuration(ADV_PDU_PAYLOAD_MAX + PDU_HEADER_OCTETS,
                                                            m_phy->GetPhyMode()),
                                &BleLinkLayer::CloseAdvertisingResponseWindow,
                                this);
        return;
    }
    m_phy->SetRxEnabled(false);
    NextAdvertisingChannel();
}

void
BleLinkLayer::NextAdvertisingChannel()
{
    NS_LOG_FUNCTION(this << m_advChannelIndex);
    if (m_state != BleLinkState::ADVERTISING)
    {
        return;
    }
    ++m_advChannelIndex;
    if (m_advChannelIndex < m_advChannels.size())
    {
        m_nextEvent = Simulator::ScheduleNow(&BleLinkLayer::SendAdvertisingPacket, this);
        return;
    }
    // the specification adds a random delay of up to ten milliseconds to every advertising event
    const Time delay = MicroSeconds(m_random->GetValue(0.0, ADV_DELAY_MAX_US));
    m_phy->SetRxEnabled(false);
    m_nextEvent =
        Simulator::Schedule(m_advInterval + delay, &BleLinkLayer::StartAdvertisingEvent, this);
}

/* -------------------------------------------------------------------------------------------- */
/* Scanning and initiating                                                                        */
/* -------------------------------------------------------------------------------------------- */

void
BleLinkLayer::StartScanning()
{
    NS_LOG_FUNCTION(this);
    NS_ABORT_MSG_IF(!m_phy, "A PHY is required before scanning");
    ResetRadio();
    m_state = BleLinkState::SCANNING;
    m_scanChannelIndex = 0;
    m_phy->SetAccessAddress(ADV_ACCESS_ADDRESS);
    const Time first = MicroSeconds(m_random->GetValue(0.0, m_scanInterval.GetMicroSeconds()));
    m_nextEvent = Simulator::Schedule(first, &BleLinkLayer::OpenScanWindow, this);
}

void
BleLinkLayer::StopScanning()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::SCANNING)
    {
        return;
    }
    ResetRadio();
    m_state = BleLinkState::STANDBY;
}

void
BleLinkLayer::StartInitiating(Mac48Address peer)
{
    NS_LOG_FUNCTION(this << peer);
    NS_ABORT_MSG_IF(!m_phy, "A PHY is required before initiating");
    ResetRadio();
    m_initiatorTarget = peer;
    m_state = BleLinkState::INITIATING;
    m_scanChannelIndex = 0;
    m_phy->SetAccessAddress(ADV_ACCESS_ADDRESS);
    const Time first = MicroSeconds(m_random->GetValue(0.0, m_scanInterval.GetMicroSeconds()));
    m_nextEvent = Simulator::Schedule(first, &BleLinkLayer::OpenScanWindow, this);
}

void
BleLinkLayer::StopInitiating()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::INITIATING)
    {
        return;
    }
    ResetRadio();
    m_state = BleLinkState::STANDBY;
}

void
BleLinkLayer::OpenScanWindow()
{
    NS_LOG_FUNCTION(this << +m_scanChannelIndex);
    if (m_state != BleLinkState::SCANNING && m_state != BleLinkState::INITIATING)
    {
        return;
    }
    // a scanner listens to one advertising channel per scan window and rotates through them
    m_phy->SetRfChannel(m_advChannels[m_scanChannelIndex % m_advChannels.size()]);
    m_phy->SetAccessAddress(ADV_ACCESS_ADDRESS);
    m_phy->SetRxEnabled(true);
    const Time window = std::min(m_scanWindow, m_scanInterval);
    m_rxTimeout = Simulator::Schedule(window, &BleLinkLayer::CloseScanWindow, this);
}

void
BleLinkLayer::CloseScanWindow()
{
    NS_LOG_FUNCTION(this);
    if (m_state != BleLinkState::SCANNING && m_state != BleLinkState::INITIATING)
    {
        return;
    }
    if (IsPhyReceiving())
    {
        // an advertising packet is arriving, so the window stays open until it has been received
        m_rxTimeout =
            Simulator::Schedule(BlePhy::CalculateTxDuration(ADV_PDU_PAYLOAD_MAX + PDU_HEADER_OCTETS,
                                                            m_phy->GetPhyMode()),
                                &BleLinkLayer::CloseScanWindow,
                                this);
        return;
    }
    m_phy->SetRxEnabled(false);
    ++m_scanChannelIndex;
    const Time window = std::min(m_scanWindow, m_scanInterval);
    m_nextEvent = Simulator::Schedule(m_scanInterval - window, &BleLinkLayer::OpenScanWindow, this);
}

void
BleLinkLayer::SendScanRequest()
{
    NS_LOG_FUNCTION(this << m_pendingPeer);
    if (m_state != BleLinkState::SCANNING)
    {
        return;
    }
    auto packet = Create<Packet>();
    BleTwoAddressHeader payload;
    payload.SetSourceAddress(m_address);
    payload.SetAdvA(m_pendingPeer);
    packet->AddHeader(payload);
    BleAdvHeader header;
    header.SetPduType(BleAdvPduType::SCAN_REQ);
    header.SetLength(static_cast<uint8_t>(2 * BD_ADDR_OCTETS));
    packet->AddHeader(header);
    AppendCrc(packet, ADV_CRC_INIT);

    Transmit(packet, [this]() {
        // listen for the scan response for the rest of the scan window
        m_phy->SetRxEnabled(true);
    });
}

void
BleLinkLayer::SendConnectRequest()
{
    NS_LOG_FUNCTION(this << m_pendingPeer);
    if (m_state != BleLinkState::INITIATING)
    {
        return;
    }

    BleConnection connection;
    connection.accessAddress = GenerateAccessAddress(m_random);
    connection.crcInit = static_cast<uint32_t>(m_random->GetInteger(0, 0xFFFFFF));
    connection.peer = m_pendingPeer;
    const auto units = static_cast<uint16_t>(
        std::max<uint64_t>(CONN_INTERVAL_MIN_UNITS,
                           m_connInterval.GetMicroSeconds() / CONN_INTERVAL_UNIT_US));
    connection.interval = MicroSeconds(static_cast<uint64_t>(units) * CONN_INTERVAL_UNIT_US);
    connection.latency = m_connLatency;
    connection.supervisionTimeout = m_connSupervisionTimeout;
    connection.hopIncrement =
        static_cast<uint8_t>(m_random->GetInteger(HOP_INCREMENT_MIN, HOP_INCREMENT_MAX));
    connection.useCsa2 = m_useCsa2;
    connection.eventCounter = 0;

    auto packet = Create<Packet>();
    BleConnectIndHeader payload;
    payload.SetInitA(m_address);
    payload.SetAdvA(m_pendingPeer);
    payload.SetAccessAddress(connection.accessAddress);
    payload.SetCrcInit(connection.crcInit);
    payload.SetWinSize(1);
    payload.SetWinOffset(0);
    payload.SetInterval(units);
    payload.SetLatency(connection.latency);
    payload.SetTimeout(static_cast<uint16_t>(connection.supervisionTimeout.GetMicroSeconds() /
                                             SUPERVISION_TIMEOUT_UNIT_US));
    payload.SetChannelMap(connection.channelMap);
    payload.SetHopIncrement(connection.hopIncrement);
    payload.SetSca(0);
    packet->AddHeader(payload);
    BleAdvHeader header;
    header.SetPduType(BleAdvPduType::CONNECT_IND);
    header.SetChSel(m_useCsa2);
    header.SetLength(static_cast<uint8_t>(payload.GetSerializedSize()));
    packet->AddHeader(header);
    AppendCrc(packet, ADV_CRC_INIT);

    Transmit(packet, [this, connection]() mutable {
        // the first anchor point follows the connection request by the transmit window delay
        m_state = BleLinkState::CONNECTION;
        m_role = BleRole::CENTRAL;
        connection.lastPacketReceived = Simulator::Now();
        m_connection = connection;
        m_phy->SetRxEnabled(false);
        m_connectionEstablishedTrace(connection.peer, static_cast<uint8_t>(BleRole::CENTRAL));
        m_nextEvent = Simulator::Schedule(MicroSeconds(TRANSMIT_WINDOW_DELAY_US),
                                          &BleLinkLayer::StartConnectionEvent,
                                          this);
        m_supervision = Simulator::Schedule(connection.supervisionTimeout,
                                            &BleLinkLayer::OnSupervisionTimeout,
                                            this);
    });
}

/* -------------------------------------------------------------------------------------------- */
/* Connections                                                                                    */
/* -------------------------------------------------------------------------------------------- */

void
BleLinkLayer::SetupStaticConnection(Ptr<BleLinkLayer> central,
                                    Ptr<BleLinkLayer> peripheral,
                                    Time interval,
                                    Time start)
{
    NS_ABORT_MSG_IF(!central || !peripheral, "Two link layers are required");
    auto random = CreateObject<UniformRandomVariable>();

    BleConnection shared;
    shared.accessAddress = GenerateAccessAddress(random);
    shared.crcInit = static_cast<uint32_t>(random->GetInteger(0, 0xFFFFFF));
    const auto units = static_cast<uint16_t>(
        std::max<uint64_t>(CONN_INTERVAL_MIN_UNITS,
                           interval.GetMicroSeconds() / CONN_INTERVAL_UNIT_US));
    shared.interval = MicroSeconds(static_cast<uint64_t>(units) * CONN_INTERVAL_UNIT_US);
    shared.hopIncrement =
        static_cast<uint8_t>(random->GetInteger(HOP_INCREMENT_MIN, HOP_INCREMENT_MAX));
    shared.useCsa2 = central->m_useCsa2;
    shared.supervisionTimeout = central->m_connSupervisionTimeout;
    shared.latency = central->m_connLatency;
    shared.lastPacketReceived = start;

    BleConnection centralConnection = shared;
    centralConnection.peer = peripheral->GetAddress();
    BleConnection peripheralConnection = shared;
    peripheralConnection.peer = central->GetAddress();

    central->ResetRadio();
    central->m_state = BleLinkState::CONNECTION;
    central->m_role = BleRole::CENTRAL;
    central->m_connection = centralConnection;

    peripheral->ResetRadio();
    peripheral->m_state = BleLinkState::CONNECTION;
    peripheral->m_role = BleRole::PERIPHERAL;
    peripheral->m_connection = peripheralConnection;

    central->m_connectionEstablishedTrace(centralConnection.peer,
                                          static_cast<uint8_t>(BleRole::CENTRAL));
    peripheral->m_connectionEstablishedTrace(peripheralConnection.peer,
                                             static_cast<uint8_t>(BleRole::PERIPHERAL));

    // both sides wake at the same anchor points
    central->m_nextEvent =
        Simulator::Schedule(start - Simulator::Now(), &BleLinkLayer::StartConnectionEvent, central);
    peripheral->m_nextEvent = Simulator::Schedule(start - Simulator::Now(),
                                                  &BleLinkLayer::StartConnectionEvent,
                                                  peripheral);
    central->m_supervision =
        Simulator::Schedule(start - Simulator::Now() + centralConnection.supervisionTimeout,
                            &BleLinkLayer::OnSupervisionTimeout,
                            central);
    peripheral->m_supervision =
        Simulator::Schedule(start - Simulator::Now() + peripheralConnection.supervisionTimeout,
                            &BleLinkLayer::OnSupervisionTimeout,
                            peripheral);
}

uint8_t
BleLinkLayer::SelectConnectionChannel()
{
    NS_ASSERT(m_connection);
    if (m_connection->useCsa2)
    {
        BleChannelSelectionAlgorithm2 csa2(m_connection->accessAddress);
        return csa2.GetChannel(m_connection->channelMap, m_connection->eventCounter);
    }
    // algorithm #1 advances from the channel of the previous event, so it is replayed from the
    // start of the connection to stay consistent with the event counter
    BleChannelSelectionAlgorithm1 csa1(m_connection->hopIncrement);
    uint8_t channel = 0;
    for (uint32_t event = 0; event <= m_connection->eventCounter; ++event)
    {
        channel = csa1.NextChannel(m_connection->channelMap);
    }
    return channel;
}

void
BleLinkLayer::StartConnectionEvent()
{
    NS_LOG_FUNCTION(this << BleRoleName(m_role));
    if (!IsConnected())
    {
        return;
    }

    // a peripheral with nothing to send may skip events, up to the latency of the connection
    if (m_role == BleRole::PERIPHERAL && m_connection->latency > 0 && m_queue.empty() &&
        m_fragments.empty() && !m_pendingFragment && m_connection->established &&
        m_skippedEvents < m_connection->latency)
    {
        ++m_skippedEvents;
        ++m_connection->eventCounter;
        m_connection->lastAnchor = Simulator::Now();
        m_nextEvent =
            Simulator::Schedule(m_connection->interval, &BleLinkLayer::StartConnectionEvent, this);
        return;
    }
    m_skippedEvents = 0;

    m_connection->lastAnchor = Simulator::Now();
    m_connection->currentChannel = SelectConnectionChannel();
    m_phy->SetRfChannel(m_connection->currentChannel);
    m_phy->SetAccessAddress(m_connection->accessAddress);
    m_eventOpen = true;
    m_peerMoreData = false;
    m_localMoreData = false;
    m_eventPdus = 0;
    m_connectionEventTrace(m_connection->eventCounter, m_connection->currentChannel);

    if (m_role == BleRole::CENTRAL)
    {
        // the central owns the timing and speaks first
        SendConnectionPdu();
    }
    else
    {
        // the peripheral listens around the anchor point
        m_phy->SetRxEnabled(true);
        m_rxTimeout = Simulator::Schedule(MicroSeconds(ANCHOR_WINDOW_US),
                                          &BleLinkLayer::OnConnectionRxTimeout,
                                          this);
    }
}

Ptr<Packet>
BleLinkLayer::BuildDataPdu()
{
    NS_ASSERT(m_connection);
    Ptr<Packet> payload;
    BleLlid llid = BleLlid::DATA_CONTINUATION;
    bool retransmission = false;

    if (m_pendingFragment)
    {
        // the previous fragment has not been acknowledged, so it is sent again unchanged
        payload = m_pendingFragment->Copy();
        llid = m_pendingIsStart ? BleLlid::DATA_START : BleLlid::DATA_CONTINUATION;
        retransmission = true;
    }
    else
    {
        if (m_fragments.empty())
        {
            PrepareNextSdu();
        }
        if (!m_fragments.empty())
        {
            // the fragment stays in the queue until it is acknowledged
            m_pendingFragment = m_fragments.front().first->Copy();
            m_pendingIsStart = m_fragments.front().second;
            payload = m_pendingFragment->Copy();
            llid = m_pendingIsStart ? BleLlid::DATA_START : BleLlid::DATA_CONTINUATION;
        }
        else
        {
            // nothing to send, so an empty PDU keeps the connection alive
            payload = Create<Packet>();
            llid = BleLlid::DATA_CONTINUATION;
        }
    }

    // more data is announced while anything beyond the fragment being sent is still waiting
    m_localMoreData = (m_fragments.size() > 1) || !m_queue.empty();

    BleDataHeader header;
    header.SetLlid(llid);
    header.SetSn(m_connection->sn);
    header.SetNesn(m_connection->nesn);
    header.SetMd(m_localMoreData);
    header.SetLength(static_cast<uint8_t>(payload->GetSize()));
    payload->AddHeader(header);
    AppendCrc(payload, m_connection->crcInit);

    m_pduSentTrace(header.GetLength(), retransmission);
    return payload;
}

void
BleLinkLayer::PrepareNextSdu()
{
    if (m_queue.empty())
    {
        return;
    }
    auto entry = m_queue.front();
    m_queue.pop_front();
    auto sdu = entry.first->Copy();

    // the payload is announced by an L2CAP basic header so the receiver knows how many
    // fragments to expect
    BleL2capHeader l2cap;
    l2cap.SetLength(static_cast<uint16_t>(sdu->GetSize()));
    sdu->AddHeader(l2cap);

    uint32_t offset = 0;
    const uint32_t total = sdu->GetSize();
    bool first = true;
    while (offset < total)
    {
        const uint32_t size = std::min<uint32_t>(m_maxPduPayload, total - offset);
        m_fragments.emplace_back(sdu->CreateFragment(offset, size), first);
        offset += size;
        first = false;
    }
}

void
BleLinkLayer::SendConnectionPdu()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected() || !m_eventOpen)
    {
        return;
    }
    auto pdu = BuildDataPdu();
    ++m_eventPdus;
    Transmit(pdu, [this]() { OpenConnectionRxWindow(); });
}

void
BleLinkLayer::OpenConnectionRxWindow()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected() || !m_eventOpen)
    {
        return;
    }
    if (m_role == BleRole::PERIPHERAL && !m_peerMoreData && !m_localMoreData)
    {
        // the peripheral has answered and neither side has more to send, so the event is over
        EndConnectionEvent();
        return;
    }
    m_phy->SetRxEnabled(true);
    m_rxTimeout = Simulator::Schedule(MicroSeconds(T_IFS_US + RX_WINDOW_SLACK_US),
                                      &BleLinkLayer::OnConnectionRxTimeout,
                                      this);
}

void
BleLinkLayer::OnConnectionRxTimeout()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected())
    {
        return;
    }
    if (IsPhyReceiving())
    {
        // the receiver has synchronised on a packet, so it waits for the packet to end rather
        // than abandoning the connection event
        m_rxTimeout = Simulator::Schedule(
            BlePhy::CalculateTxDuration(m_maxPduPayload + PDU_HEADER_OCTETS, m_phy->GetPhyMode()),
            &BleLinkLayer::OnConnectionRxTimeout,
            this);
        return;
    }
    // nothing arrived in the window, so the connection event ends and the pending fragment,
    // if any, will be sent again at the next anchor point
    EndConnectionEvent();
}

void
BleLinkLayer::EndConnectionEvent()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected())
    {
        return;
    }
    m_rxTimeout.Cancel();
    m_eventOpen = false;
    m_phy->SetRxEnabled(false);
    ++m_connection->eventCounter;

    const Time nextAnchor = m_connection->lastAnchor + m_connection->interval;
    Time delay = nextAnchor - Simulator::Now();
    if (delay.IsNegative())
    {
        // the event overran its interval, so the next anchor point is the following one
        const auto intervals =
            1 + (-delay.GetMicroSeconds()) / m_connection->interval.GetMicroSeconds();
        delay = nextAnchor + intervals * m_connection->interval - Simulator::Now();
        m_connection->eventCounter = static_cast<uint16_t>(m_connection->eventCounter + intervals);
    }
    m_nextEvent = Simulator::Schedule(delay, &BleLinkLayer::StartConnectionEvent, this);
}

void
BleLinkLayer::OnSupervisionTimeout()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected())
    {
        return;
    }
    const auto peer = m_connection->peer;
    NS_LOG_WARN("The connection with " << peer << " timed out");
    ResetRadio();
    m_supervision.Cancel();
    m_connection.reset();
    m_state = BleLinkState::STANDBY;
    m_role = BleRole::NONE;
    m_eventOpen = false;
    m_connectionClosedTrace(peer, "supervision-timeout");
}

void
BleLinkLayer::Disconnect()
{
    NS_LOG_FUNCTION(this);
    if (!IsConnected())
    {
        return;
    }
    const auto peer = m_connection->peer;
    ResetRadio();
    m_supervision.Cancel();
    m_connection.reset();
    m_state = BleLinkState::STANDBY;
    m_role = BleRole::NONE;
    m_eventOpen = false;
    m_connectionClosedTrace(peer, "local-request");
}

bool
BleLinkLayer::SetChannelMap(const BleChannelMap& map)
{
    NS_LOG_FUNCTION(this << +map.GetNUsedChannels());
    if (!IsConnected())
    {
        return false;
    }
    // the specification requires at least two usable data channels
    if (map.GetNUsedChannels() < 2)
    {
        NS_LOG_WARN("A channel map must leave at least two channels usable");
        return false;
    }
    m_connection->channelMap = map;
    return true;
}

bool
BleLinkLayer::ProcessSequenceNumbers(const BleDataHeader& header)
{
    NS_ASSERT(m_connection);

    // the peer acknowledges our last packet by returning a next expected sequence number that
    // differs from the sequence number we sent
    if (header.GetNesn() != m_connection->sn)
    {
        m_connection->sn = header.GetNesn();
        if (m_pendingFragment)
        {
            if (!m_fragments.empty())
            {
                m_fragments.pop_front();
            }
            m_pendingFragment = nullptr;
        }
    }

    // a packet whose sequence number matches what we expect carries new data
    if (header.GetSn() == m_connection->nesn)
    {
        m_connection->nesn = !m_connection->nesn;
        return true;
    }
    return false;
}

void
BleLinkLayer::ReassembleFragment(Ptr<Packet> fragment, const BleDataHeader& header)
{
    NS_ASSERT(m_connection);
    if (fragment->GetSize() == 0)
    {
        return;
    }

    if (header.GetLlid() == BleLlid::DATA_START)
    {
        // a new payload begins, so its announced length says how much is still missing
        m_reassembly = fragment->Copy();
        BleL2capHeader l2cap;
        if (m_reassembly->GetSize() >= L2CAP_HEADER_OCTETS)
        {
            m_reassembly->PeekHeader(l2cap);
            const uint32_t total = L2CAP_HEADER_OCTETS + l2cap.GetLength();
            m_reassemblyRemaining =
                (total > m_reassembly->GetSize()) ? total - m_reassembly->GetSize() : 0;
        }
        else
        {
            m_reassemblyRemaining = 0;
        }
    }
    else if (m_reassembly)
    {
        m_reassembly->AddAtEnd(fragment->Copy());
        m_reassemblyRemaining = (m_reassemblyRemaining > fragment->GetSize())
                                    ? m_reassemblyRemaining - fragment->GetSize()
                                    : 0;
    }
    else
    {
        // a continuation without a start, which happens if the start was lost
        return;
    }

    if (m_reassemblyRemaining == 0 && m_reassembly)
    {
        BleL2capHeader l2cap;
        m_reassembly->RemoveHeader(l2cap);
        auto payload = m_reassembly;
        m_reassembly = nullptr;
        m_deliveredTrace(payload, m_connection->peer);
        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(payload, m_connection->peer);
        }
    }
}

bool
BleLinkLayer::Enqueue(Ptr<Packet> packet, Mac48Address destination)
{
    NS_LOG_FUNCTION(this << packet->GetSize() << destination);
    if (m_queue.size() >= m_maxQueueSize)
    {
        NS_LOG_WARN("The transmit queue is full, the payload is dropped");
        return false;
    }
    m_queue.emplace_back(packet, destination);
    return true;
}

uint32_t
BleLinkLayer::GetQueueSize() const
{
    return static_cast<uint32_t>(m_queue.size());
}

/* -------------------------------------------------------------------------------------------- */
/* Reception                                                                                      */
/* -------------------------------------------------------------------------------------------- */

void
BleLinkLayer::OnPhyReceiveError(Ptr<const Packet> packet, double rssiDbm)
{
    NS_LOG_FUNCTION(this << rssiDbm);
    // a corrupted packet is simply not acknowledged; the timers already running end the event
}

void
BleLinkLayer::OnPhyReceiveOk(Ptr<Packet> packet, double rssiDbm, uint8_t channel)
{
    NS_LOG_FUNCTION(this << packet->GetSize() << rssiDbm << +channel);
    auto pdu = packet->Copy();
    BleCrcTrailer crc;
    pdu->RemoveTrailer(crc);

    if (IsAdvertisingChannel(channel))
    {
        HandleAdvertisingPacket(pdu, rssiDbm, channel);
    }
    else
    {
        HandleDataPacket(pdu, rssiDbm);
    }
}

void
BleLinkLayer::HandleAdvertisingPacket(Ptr<Packet> packet, double rssiDbm, uint8_t channel)
{
    NS_LOG_FUNCTION(this << rssiDbm << +channel);
    BleAdvHeader header;
    packet->RemoveHeader(header);

    switch (header.GetPduType())
    {
    case BleAdvPduType::ADV_IND:
    case BleAdvPduType::ADV_NONCONN_IND:
    case BleAdvPduType::ADV_SCAN_IND:
    case BleAdvPduType::SCAN_RSP: {
        if (m_state != BleLinkState::SCANNING && m_state != BleLinkState::INITIATING)
        {
            return;
        }
        BleAdvPayloadHeader payload;
        packet->RemoveHeader(payload);

        BleAdvReport report;
        report.address = payload.GetAdvA();
        report.type = header.GetPduType();
        report.rssiDbm = rssiDbm;
        report.channel = channel;
        report.advDataLength = packet->GetSize();
        report.scanResponse = (header.GetPduType() == BleAdvPduType::SCAN_RSP);
        m_advReportTrace(report);

        m_pendingPeer = report.address;

        if (m_state == BleLinkState::INITIATING &&
            (header.GetPduType() == BleAdvPduType::ADV_IND) &&
            (m_initiatorTarget == Mac48Address::GetBroadcast() ||
             m_initiatorTarget == report.address))
        {
            m_rxTimeout.Cancel();
            m_phy->SetRxEnabled(false);
            Simulator::Schedule(MicroSeconds(T_IFS_US), &BleLinkLayer::SendConnectRequest, this);
            return;
        }
        if (m_state == BleLinkState::SCANNING && m_activeScanning && !report.scanResponse &&
            (header.GetPduType() == BleAdvPduType::ADV_IND ||
             header.GetPduType() == BleAdvPduType::ADV_SCAN_IND))
        {
            m_phy->SetRxEnabled(false);
            Simulator::Schedule(MicroSeconds(T_IFS_US), &BleLinkLayer::SendScanRequest, this);
        }
        return;
    }
    case BleAdvPduType::SCAN_REQ: {
        if (m_state != BleLinkState::ADVERTISING)
        {
            return;
        }
        BleTwoAddressHeader payload;
        packet->RemoveHeader(payload);
        if (payload.GetAdvA() != m_address)
        {
            return;
        }
        m_rxTimeout.Cancel();
        // answer with a scan response after the inter frame space
        Simulator::Schedule(MicroSeconds(T_IFS_US), [this]() {
            if (m_state != BleLinkState::ADVERTISING)
            {
                return;
            }
            auto response = Create<Packet>(m_advDataLength);
            BleAdvPayloadHeader body;
            body.SetAdvA(m_address);
            body.SetAdvDataLength(m_advDataLength);
            response->AddHeader(body);
            BleAdvHeader responseHeader;
            responseHeader.SetPduType(BleAdvPduType::SCAN_RSP);
            responseHeader.SetLength(static_cast<uint8_t>(BD_ADDR_OCTETS + m_advDataLength));
            response->AddHeader(responseHeader);
            AppendCrc(response, ADV_CRC_INIT);
            Transmit(response, [this]() { NextAdvertisingChannel(); });
        });
        return;
    }
    case BleAdvPduType::CONNECT_IND: {
        if (m_state != BleLinkState::ADVERTISING || m_advType != BleAdvPduType::ADV_IND)
        {
            return;
        }
        BleConnectIndHeader payload;
        packet->RemoveHeader(payload);
        if (payload.GetAdvA() != m_address)
        {
            return;
        }

        m_rxTimeout.Cancel();
        m_nextEvent.Cancel();

        BleConnection connection;
        connection.accessAddress = payload.GetAccessAddress();
        connection.crcInit = payload.GetCrcInit();
        connection.peer = payload.GetInitA();
        connection.interval =
            MicroSeconds(static_cast<uint64_t>(payload.GetInterval()) * CONN_INTERVAL_UNIT_US);
        connection.latency = payload.GetLatency();
        connection.supervisionTimeout =
            MicroSeconds(static_cast<uint64_t>(payload.GetTimeout()) * SUPERVISION_TIMEOUT_UNIT_US);
        connection.channelMap = payload.GetChannelMap();
        connection.hopIncrement = payload.GetHopIncrement();
        connection.useCsa2 = header.GetChSel();
        connection.eventCounter = 0;
        connection.lastPacketReceived = Simulator::Now();

        m_state = BleLinkState::CONNECTION;
        m_role = BleRole::PERIPHERAL;
        m_connection = connection;
        m_phy->SetRxEnabled(false);
        m_connectionEstablishedTrace(connection.peer, static_cast<uint8_t>(BleRole::PERIPHERAL));
        m_nextEvent = Simulator::Schedule(MicroSeconds(TRANSMIT_WINDOW_DELAY_US),
                                          &BleLinkLayer::StartConnectionEvent,
                                          this);
        m_supervision = Simulator::Schedule(connection.supervisionTimeout,
                                            &BleLinkLayer::OnSupervisionTimeout,
                                            this);
        return;
    }
    default:
        return;
    }
}

void
BleLinkLayer::HandleDataPacket(Ptr<Packet> packet, double rssiDbm)
{
    NS_LOG_FUNCTION(this << rssiDbm);
    if (!IsConnected() || !m_eventOpen)
    {
        return;
    }

    BleDataHeader header;
    packet->RemoveHeader(header);

    m_rxTimeout.Cancel();
    m_connection->lastPacketReceived = Simulator::Now();
    m_connection->established = true;
    m_supervision.Cancel();
    m_supervision = Simulator::Schedule(m_connection->supervisionTimeout,
                                        &BleLinkLayer::OnSupervisionTimeout,
                                        this);

    m_peerMoreData = header.GetMd();
    const bool isNewData = ProcessSequenceNumbers(header);
    if (isNewData && header.GetLength() > 0)
    {
        ReassembleFragment(packet, header);
    }

    const bool moreToSend = m_pendingFragment || !m_fragments.empty() || !m_queue.empty();

    if (m_role == BleRole::PERIPHERAL)
    {
        // the peripheral always answers the central after the inter frame space
        Simulator::Schedule(MicroSeconds(T_IFS_US), &BleLinkLayer::SendConnectionPdu, this);
        return;
    }

    // The central continues the event while either side has more data, but only if a whole
    // further exchange still fits before the next anchor point. The specification forbids
    // starting a packet that would extend beyond it, and letting an event overrun would push
    // every following anchor point one interval later.
    const Time elapsed = Simulator::Now() - m_connection->lastAnchor;
    const Time exchange =
        MicroSeconds(2 * T_IFS_US) +
        2 * BlePhy::CalculateTxDuration(m_maxPduPayload + PDU_HEADER_OCTETS, m_phy->GetPhyMode());
    if ((m_peerMoreData || moreToSend) && (elapsed + exchange <= m_connection->interval))
    {
        Simulator::Schedule(MicroSeconds(T_IFS_US), &BleLinkLayer::SendConnectionPdu, this);
        return;
    }
    EndConnectionEvent();
}

} // namespace ble
} // namespace ns3
