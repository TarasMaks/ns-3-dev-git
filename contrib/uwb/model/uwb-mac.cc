/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-mac.h"

#include "uwb-clock-model.h"

#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbMac");

namespace uwb
{

NS_OBJECT_ENSURE_REGISTERED(UwbMac);

TypeId
UwbMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbMac")
            .SetParent<Object>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbMac>()
            .AddAttribute("ResponseDelay",
                          "How long this device waits between receiving a ranging frame and "
                          "placing the marker of its reply. It is the single number that decides "
                          "how wrong a single-sided exchange is, because the crystals of the two "
                          "devices drift apart for exactly this long. A shorter turnaround is "
                          "always better and is bounded below by the preamble of the reply.",
                          TimeValue(MicroSeconds(300)),
                          MakeTimeAccessor(&UwbMac::SetResponseDelay, &UwbMac::GetResponseDelay),
                          MakeTimeChecker())
            .AddAttribute("RangingTimeout",
                          "How long an exchange may stall before it is abandoned.",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&UwbMac::m_rangingTimeout),
                          MakeTimeChecker())
            .AddAttribute("AckTimeout",
                          "How long to wait for an acknowledgement before sending again.",
                          TimeValue(MilliSeconds(2)),
                          MakeTimeAccessor(&UwbMac::m_ackTimeout),
                          MakeTimeChecker())
            .AddAttribute("BackoffMinimum",
                          "Shortest wait before trying a busy radio again.",
                          TimeValue(MicroSeconds(200)),
                          MakeTimeAccessor(&UwbMac::m_backoffMinimum),
                          MakeTimeChecker())
            .AddAttribute("BackoffMaximum",
                          "Longest wait before trying a busy radio again. A UWB signal sits "
                          "below the noise floor, so there is nothing for a carrier sense to "
                          "hear and IEEE Std 802.15.4 lets UWB devices use unslotted ALOHA; the "
                          "spread between these two bounds is what keeps two devices that "
                          "collided from colliding again.",
                          TimeValue(MilliSeconds(2)),
                          MakeTimeAccessor(&UwbMac::m_backoffMaximum),
                          MakeTimeChecker())
            .AddAttribute("MaxRetries",
                          "How many times a data frame is sent before it is given up on.",
                          UintegerValue(3),
                          MakeUintegerAccessor(&UwbMac::m_maxRetries),
                          MakeUintegerChecker<uint32_t>(0, 16))
            .AddAttribute("QueueLimit",
                          "How many data frames may wait to be sent.",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UwbMac::m_queueLimit),
                          MakeUintegerChecker<uint32_t>(1, 100000))
            .AddAttribute("AckEnabled",
                          "Whether data frames ask to be acknowledged.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&UwbMac::m_ackEnabled),
                          MakeBooleanChecker())
            .AddAttribute("ReportRange",
                          "Whether the device that computed a double-sided range sends it back "
                          "to the other end, so that both know it.",
                          BooleanValue(true),
                          MakeBooleanAccessor(&UwbMac::m_reportRange),
                          MakeBooleanChecker())
            .AddTraceSource("Ranging",
                            "A ranging exchange finished.",
                            MakeTraceSourceAccessor(&UwbMac::m_rangingTrace),
                            "ns3::uwb::UwbMac::RangingTracedCallback")
            .AddTraceSource("MacTx",
                            "A frame was handed to the radio.",
                            MakeTraceSourceAccessor(&UwbMac::m_txTrace),
                            "ns3::uwb::UwbMac::FrameTracedCallback")
            .AddTraceSource("MacRx",
                            "A data frame was delivered.",
                            MakeTraceSourceAccessor(&UwbMac::m_rxTrace),
                            "ns3::uwb::UwbMac::FrameTracedCallback")
            .AddTraceSource("MacDrop",
                            "A data frame was given up on.",
                            MakeTraceSourceAccessor(&UwbMac::m_dropTrace),
                            "ns3::uwb::UwbMac::FrameTracedCallback");
    return tid;
}

UwbMac::UwbMac()
    : m_panId(0),
      m_responseDelay(MicroSeconds(300)),
      m_rangingTimeout(MilliSeconds(10)),
      m_ackTimeout(MilliSeconds(2)),
      m_backoffMinimum(MicroSeconds(200)),
      m_backoffMaximum(MilliSeconds(2)),
      m_maxRetries(3),
      m_queueLimit(100),
      m_ackEnabled(true),
      m_reportRange(true),
      m_sequence(0),
      m_session(0),
      m_awaitingAck(false)
{
    NS_LOG_FUNCTION(this);
    m_backoff = CreateObject<UniformRandomVariable>();
}

UwbMac::~UwbMac()
{
    NS_LOG_FUNCTION(this);
}

void
UwbMac::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_sendEvent.Cancel();
    m_ackEvent.Cancel();
    m_initiator.timeout.Cancel();
    m_responder.timeout.Cancel();
    m_queue.clear();
    m_phy = nullptr;
    m_receiveCallback.Nullify();
    m_rangingResultCallback.Nullify();
    m_blinkCallback.Nullify();
    Object::DoDispose();
}

void
UwbMac::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    Object::DoInitialize();
}

void
UwbMac::SetPhy(Ptr<UwbPhy> phy)
{
    NS_LOG_FUNCTION(this << phy);
    m_phy = phy;
    if (m_phy)
    {
        m_phy->SetReceiveOkCallback(MakeCallback(&UwbMac::ReceiveOk, this));
    }
}

Ptr<UwbPhy>
UwbMac::GetPhy() const
{
    return m_phy;
}

void
UwbMac::SetAddress(Mac16Address address)
{
    m_address = address;
}

Mac16Address
UwbMac::GetAddress() const
{
    return m_address;
}

void
UwbMac::SetPanId(uint16_t panId)
{
    m_panId = panId;
}

uint16_t
UwbMac::GetPanId() const
{
    return m_panId;
}

void
UwbMac::SetResponseDelay(Time delay)
{
    m_responseDelay = delay;
}

Time
UwbMac::GetResponseDelay() const
{
    return m_responseDelay;
}

std::size_t
UwbMac::GetQueueLength() const
{
    return m_queue.size();
}

void
UwbMac::SetReceiveCallback(ReceiveCallback callback)
{
    m_receiveCallback = callback;
}

void
UwbMac::SetRangingResultCallback(RangingResultCallback callback)
{
    m_rangingResultCallback = callback;
}

void
UwbMac::SetBlinkCallback(BlinkCallback callback)
{
    m_blinkCallback = callback;
}

bool
UwbMac::IsRanging() const
{
    return m_initiator.active;
}

int64_t
UwbMac::AssignStreams(int64_t stream)
{
    m_backoff->SetStream(stream);
    return 1;
}

/* -------------------------------------------------------------------------------------------- */
/* Sending                                                                                        */
/* -------------------------------------------------------------------------------------------- */

Time
UwbMac::GetReplyMarkerTime() const
{
    // the marker of a frame is the first pulse after its preamble, so a reply can never be
    // marked sooner than one preamble from now, however short the configured turnaround is
    const Time earliest = m_phy->GetShrDuration() + MicroSeconds(10);
    return Simulator::Now() + std::max(m_responseDelay, earliest);
}

bool
UwbMac::SendFrame(Ptr<Packet> payload,
                  Mac16Address destination,
                  UwbFrameType type,
                  uint8_t sequence,
                  bool ackRequest,
                  bool ranging)
{
    auto packet = payload ? payload->Copy() : Create<Packet>();
    UwbMacHeader header;
    header.SetFrameType(type);
    header.SetSequenceNumber(sequence);
    header.SetPanId(m_panId);
    header.SetSource(m_address);
    header.SetDestination(destination);
    header.SetAckRequest(ackRequest);
    packet->AddHeader(header);

    UwbFcsTrailer trailer;
    trailer.CalculateFcs(packet);
    packet->AddTrailer(trailer);

    if (!m_phy->StartTx(packet, ranging))
    {
        return false;
    }
    m_txTrace(packet, destination);
    return true;
}

bool
UwbMac::ScheduleFrame(Ptr<Packet> payload, Mac16Address destination, Time markerAt)
{
    auto packet = payload ? payload->Copy() : Create<Packet>();
    UwbMacHeader header;
    header.SetFrameType(UwbFrameType::COMMAND);
    header.SetSequenceNumber(m_sequence++);
    header.SetPanId(m_panId);
    header.SetSource(m_address);
    header.SetDestination(destination);
    packet->AddHeader(header);

    UwbFcsTrailer trailer;
    trailer.CalculateFcs(packet);
    packet->AddTrailer(trailer);

    if (!m_phy->ScheduleTx(packet, markerAt, true))
    {
        return false;
    }
    m_txTrace(packet, destination);
    return true;
}

bool
UwbMac::Enqueue(Ptr<Packet> packet, Mac16Address destination)
{
    NS_LOG_FUNCTION(this << packet << destination);
    if (m_queue.size() >= m_queueLimit)
    {
        m_dropTrace(packet, destination);
        return false;
    }
    m_queue.push_back(Pending{packet, destination, m_sequence++, 0});
    if (!m_sendEvent.IsPending() && !m_awaitingAck)
    {
        m_sendEvent = Simulator::ScheduleNow(&UwbMac::TrySend, this);
    }
    return true;
}

void
UwbMac::Backoff()
{
    const Time wait = m_backoffMinimum + (m_backoffMaximum - m_backoffMinimum) *
                                             m_backoff->GetValue(0.0, 1.0);
    m_sendEvent = Simulator::Schedule(wait, &UwbMac::TrySend, this);
}

void
UwbMac::TrySend()
{
    if (m_queue.empty() || m_awaitingAck)
    {
        return;
    }
    // unslotted ALOHA: go when this radio is free, wait a random interval when it is not
    if (m_phy->IsBusy())
    {
        Backoff();
        return;
    }

    auto& head = m_queue.front();
    const bool broadcast = head.destination == Mac16Address::GetBroadcast();
    const bool wantAck = m_ackEnabled && !broadcast;
    if (!SendFrame(head.packet, head.destination, UwbFrameType::DATA, head.sequence, wantAck, false))
    {
        Backoff();
        return;
    }
    ++head.attempts;

    if (wantAck)
    {
        m_awaitingAck = true;
        m_ackEvent = Simulator::Schedule(m_ackTimeout, &UwbMac::AckTimeout, this);
        return;
    }

    m_queue.pop_front();
    if (!m_queue.empty())
    {
        Backoff();
    }
}

void
UwbMac::AckTimeout()
{
    NS_LOG_FUNCTION(this);
    m_awaitingAck = false;
    if (m_queue.empty())
    {
        return;
    }
    auto& head = m_queue.front();
    if (head.attempts > m_maxRetries)
    {
        m_dropTrace(head.packet, head.destination);
        m_queue.pop_front();
    }
    if (!m_queue.empty())
    {
        Backoff();
    }
}

/* -------------------------------------------------------------------------------------------- */
/* Ranging                                                                                        */
/* -------------------------------------------------------------------------------------------- */

bool
UwbMac::StartRanging(Mac16Address peer, UwbRangingMethod method)
{
    NS_LOG_FUNCTION(this << peer << UwbRangingMethodName(method));
    NS_ABORT_MSG_IF(method == UwbRangingMethod::TDOA,
                    "Time difference of arrival is not a two-way exchange; send a blink instead");
    if (m_initiator.active || m_phy->IsBusy())
    {
        return false;
    }

    m_initiator = Session{};
    m_initiator.active = true;
    m_initiator.peer = peer;
    m_initiator.method = method;
    m_initiator.id = m_session++;

    UwbRangingHeader ranging;
    ranging.SetMessage(UwbRangingMessage::POLL);
    ranging.SetSession(m_initiator.id);
    ranging.SetMethod(method);
    auto payload = Create<Packet>();
    payload->AddHeader(ranging);

    if (!SendFrame(payload, peer, UwbFrameType::COMMAND, m_sequence++, false, true))
    {
        m_initiator.active = false;
        return false;
    }

    // the radio has committed to the frame, so its marker is fixed and can be read back
    m_initiator.stamps.pollTx = m_phy->GetLastTxTimestamp();
    m_initiator.timeout =
        Simulator::Schedule(m_rangingTimeout, &UwbMac::InitiatorTimeout, this);
    return true;
}

bool
UwbMac::SendBlink()
{
    NS_LOG_FUNCTION(this);
    if (m_phy->IsBusy())
    {
        return false;
    }
    UwbRangingHeader ranging;
    ranging.SetMessage(UwbRangingMessage::BLINK);
    ranging.SetSession(m_session++);
    ranging.SetMethod(UwbRangingMethod::TDOA);
    auto payload = Create<Packet>();
    payload->AddHeader(ranging);
    return SendFrame(payload,
                     Mac16Address::GetBroadcast(),
                     UwbFrameType::COMMAND,
                     m_sequence++,
                     false,
                     true);
}

void
UwbMac::InitiatorTimeout()
{
    NS_LOG_FUNCTION(this);
    UwbRangingResult result;
    result.peer = m_initiator.peer;
    result.method = m_initiator.method;
    result.session = m_initiator.id;
    result.valid = false;
    result.when = Simulator::Now();
    result.measuredHere = true;
    m_initiator.active = false;
    Report(result);
}

void
UwbMac::ResponderTimeout()
{
    NS_LOG_FUNCTION(this);
    m_responder.active = false;
}

void
UwbMac::Report(const UwbRangingResult& result)
{
    m_rangingTrace(result);
    if (!m_rangingResultCallback.IsNull())
    {
        m_rangingResultCallback(result);
    }
}

/* -------------------------------------------------------------------------------------------- */
/* Receiving                                                                                      */
/* -------------------------------------------------------------------------------------------- */

void
UwbMac::ReceiveOk(Ptr<Packet> packet, const UwbRxInfo& info)
{
    NS_LOG_FUNCTION(this << packet);
    auto copy = packet->Copy();

    UwbFcsTrailer trailer;
    copy->RemoveTrailer(trailer);
    if (!trailer.CheckFcs(copy))
    {
        NS_LOG_DEBUG("A frame arrived with a broken frame check sequence");
        return;
    }

    UwbMacHeader header;
    copy->RemoveHeader(header);
    if (header.GetPanId() != m_panId)
    {
        return;
    }
    const bool broadcast = header.GetDestination() == Mac16Address::GetBroadcast();
    if (!broadcast && header.GetDestination() != m_address)
    {
        return;
    }

    switch (header.GetFrameType())
    {
    case UwbFrameType::ACK:
        if (m_awaitingAck && !m_queue.empty() &&
            header.GetSequenceNumber() == m_queue.front().sequence)
        {
            m_ackEvent.Cancel();
            m_awaitingAck = false;
            m_queue.pop_front();
            if (!m_queue.empty())
            {
                Backoff();
            }
        }
        return;

    case UwbFrameType::COMMAND:
        HandleRanging(copy, header.GetSource(), info);
        return;

    case UwbFrameType::DATA:
        if (header.GetAckRequest() && !broadcast)
        {
            // the acknowledgement goes out after the same turnaround a ranging reply uses, so
            // that the sender knows when to stop listening
            Simulator::Schedule(m_responseDelay,
                                &UwbMac::SendFrame,
                                this,
                                Ptr<Packet>(nullptr),
                                header.GetSource(),
                                UwbFrameType::ACK,
                                header.GetSequenceNumber(),
                                false,
                                false);
        }
        m_rxTrace(copy, header.GetSource());
        if (!m_receiveCallback.IsNull())
        {
            m_receiveCallback(copy, header.GetSource());
        }
        return;

    default:
        return;
    }
}

void
UwbMac::HandleRanging(Ptr<Packet> packet, Mac16Address source, const UwbRxInfo& info)
{
    UwbRangingHeader ranging;
    packet->RemoveHeader(ranging);
    NS_LOG_FUNCTION(this << source << UwbRangingMessageName(ranging.GetMessage()));

    switch (ranging.GetMessage())
    {
    case UwbRangingMessage::BLINK:
    {
        if (!m_blinkCallback.IsNull())
        {
            m_blinkCallback(source, ranging.GetSession(), info);
        }
        return;
    }

    case UwbRangingMessage::POLL:
    {
        // answering one poll at a time is what a real responder does as well
        if (m_responder.active)
        {
            return;
        }
        m_responder = Session{};
        m_responder.active = true;
        m_responder.peer = source;
        m_responder.method = ranging.GetMethod();
        m_responder.id = ranging.GetSession();
        m_responder.stamps.pollRx = info.rxTimestamp;
        m_responder.rxPowerDbm = info.rxPowerDbm;
        m_responder.sinrDb = info.sinrDb;

        const Time markerAt = GetReplyMarkerTime();
        m_responder.stamps.responseTx = m_phy->GetTxTimestampFor(markerAt);
        // the turnaround that matters is the one between the two markers, because that is the
        // interval over which the two crystals drift apart. It is longer than the delay that
        // was scheduled, by the tail of the poll that still had to arrive after its own marker
        m_responder.replyDelay = UwbClockModel::TicksToTime(
            UwbClockModel::TicksDifference(m_responder.stamps.responseTx,
                                           m_responder.stamps.pollRx));

        UwbRangingHeader response;
        response.SetMessage(UwbRangingMessage::RESPONSE);
        response.SetSession(m_responder.id);
        response.SetMethod(m_responder.method);
        response.SetResponderTimestamps(m_responder.stamps.pollRx, m_responder.stamps.responseTx);
        auto payload = Create<Packet>();
        payload->AddHeader(response);

        if (!ScheduleFrame(payload, source, markerAt))
        {
            m_responder.active = false;
            return;
        }
        if (m_responder.method == UwbRangingMethod::DS_TWR)
        {
            m_responder.timeout =
                Simulator::Schedule(m_rangingTimeout, &UwbMac::ResponderTimeout, this);
        }
        else
        {
            m_responder.active = false;
        }
        return;
    }

    case UwbRangingMessage::RESPONSE:
    {
        if (!m_initiator.active || source != m_initiator.peer ||
            ranging.GetSession() != m_initiator.id)
        {
            return;
        }
        m_initiator.stamps.pollRx = ranging.GetPollRx();
        m_initiator.stamps.responseTx = ranging.GetResponseTx();
        m_initiator.stamps.responseRx = info.rxTimestamp;
        m_initiator.rxPowerDbm = info.rxPowerDbm;
        m_initiator.sinrDb = info.sinrDb;

        if (m_initiator.method == UwbRangingMethod::SS_TWR)
        {
            m_initiator.timeout.Cancel();
            UwbRangingResult result;
            result.peer = source;
            result.method = UwbRangingMethod::SS_TWR;
            result.session = m_initiator.id;
            result.valid = true;
            result.rangeMetres = SolveSsTwrRange(m_initiator.stamps);
            result.rxPowerDbm = info.rxPowerDbm;
            result.sinrDb = info.sinrDb;
            result.replyDelay = UwbClockModel::TicksToTime(UwbClockModel::TicksDifference(
                m_initiator.stamps.responseTx,
                m_initiator.stamps.pollRx));
            result.when = Simulator::Now();
            result.measuredHere = true;
            m_initiator.active = false;
            Report(result);
            return;
        }

        // the third frame carries what the responder is missing, and its own transmit
        // timestamp, which is only knowable because the transmission is scheduled in advance
        const Time markerAt = GetReplyMarkerTime();
        m_initiator.stamps.finalTx = m_phy->GetTxTimestampFor(markerAt);

        UwbRangingHeader final;
        final.SetMessage(UwbRangingMessage::FINAL);
        final.SetSession(m_initiator.id);
        final.SetMethod(m_initiator.method);
        final.SetInitiatorTimestamps(m_initiator.stamps.pollTx,
                                     m_initiator.stamps.responseRx,
                                     m_initiator.stamps.finalTx);
        auto payload = Create<Packet>();
        payload->AddHeader(final);

        if (!ScheduleFrame(payload, source, markerAt))
        {
            InitiatorTimeout();
            return;
        }
        if (!m_reportRange)
        {
            // nothing will come back, so the exchange is over for this device
            m_initiator.timeout.Cancel();
            m_initiator.active = false;
        }
        return;
    }

    case UwbRangingMessage::FINAL:
    {
        if (!m_responder.active || source != m_responder.peer ||
            ranging.GetSession() != m_responder.id)
        {
            return;
        }
        m_responder.timeout.Cancel();
        m_responder.stamps.pollTx = ranging.GetPollTx();
        m_responder.stamps.responseRx = ranging.GetResponseRx();
        m_responder.stamps.finalTx = ranging.GetFinalTx();
        m_responder.stamps.finalRx = info.rxTimestamp;

        UwbRangingResult result;
        result.peer = source;
        result.method = UwbRangingMethod::DS_TWR;
        result.session = m_responder.id;
        result.valid = true;
        result.rangeMetres = SolveDsTwrRange(m_responder.stamps);
        result.rxPowerDbm = info.rxPowerDbm;
        result.sinrDb = info.sinrDb;
        result.replyDelay = m_responder.replyDelay;
        result.when = Simulator::Now();
        result.measuredHere = true;
        m_responder.active = false;
        Report(result);

        if (m_reportRange)
        {
            UwbRangingHeader report;
            report.SetMessage(UwbRangingMessage::REPORT);
            report.SetSession(result.session);
            report.SetMethod(UwbRangingMethod::DS_TWR);
            report.SetRange(result.rangeMetres);
            auto payload = Create<Packet>();
            payload->AddHeader(report);
            Simulator::Schedule(m_responseDelay,
                                &UwbMac::SendFrame,
                                this,
                                payload,
                                source,
                                UwbFrameType::COMMAND,
                                m_sequence++,
                                false,
                                false);
        }
        return;
    }

    case UwbRangingMessage::REPORT:
    {
        if (!m_initiator.active || source != m_initiator.peer ||
            ranging.GetSession() != m_initiator.id)
        {
            return;
        }
        m_initiator.timeout.Cancel();
        UwbRangingResult result;
        result.peer = source;
        result.method = m_initiator.method;
        result.session = m_initiator.id;
        result.valid = true;
        result.rangeMetres = ranging.GetRange();
        result.rxPowerDbm = info.rxPowerDbm;
        result.sinrDb = info.sinrDb;
        result.when = Simulator::Now();
        result.measuredHere = false;
        m_initiator.active = false;
        Report(result);
        return;
    }

    default:
        return;
    }
}

} // namespace uwb
} // namespace ns3
