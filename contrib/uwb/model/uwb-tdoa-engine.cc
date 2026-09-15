/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-tdoa-engine.h"

#include "uwb-clock-model.h"
#include "uwb-ranging.h"

#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbTdoaEngine");

namespace uwb
{

NS_OBJECT_ENSURE_REGISTERED(UwbTdoaEngine);

TypeId
UwbTdoaEngine::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbTdoaEngine")
            .SetParent<Object>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbTdoaEngine>()
            .AddAttribute("CollectionWindow",
                          "How long to wait after the first anchor reports before working out "
                          "where the tag was.",
                          TimeValue(MilliSeconds(1)),
                          MakeTimeAccessor(&UwbTdoaEngine::m_collectionWindow),
                          MakeTimeChecker())
            .AddAttribute("SyncError",
                          "The standard deviation of what each anchor clock is left out by once "
                          "the infrastructure has synchronised them. It is drawn once per anchor "
                          "and stays, because that is what a residual calibration error does. "
                          "Zero, the default, is perfect synchronisation; a nanosecond is about "
                          "thirty centimetres of position error and is what a good wired "
                          "installation achieves.",
                          TimeValue(Time(0)),
                          MakeTimeAccessor(&UwbTdoaEngine::m_syncError),
                          MakeTimeChecker())
            .AddAttribute("SolveHeight",
                          "Whether the vertical coordinate is estimated as well, which needs a "
                          "fifth anchor and anchors that are not all at the same height.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&UwbTdoaEngine::m_solveHeight),
                          MakeBooleanChecker())
            .AddTraceSource("Position",
                            "A tag has been located.",
                            MakeTraceSourceAccessor(&UwbTdoaEngine::m_positionTrace),
                            "ns3::uwb::UwbTdoaEngine::PositionTracedCallback");
    return tid;
}

UwbTdoaEngine::UwbTdoaEngine()
    : m_collectionWindow(MilliSeconds(1)),
      m_syncError(Time(0)),
      m_solveHeight(false)
{
    NS_LOG_FUNCTION(this);
    m_syncNoise = CreateObject<NormalRandomVariable>();
    m_syncNoise->SetAttribute("Mean", DoubleValue(0.0));
    m_syncNoise->SetAttribute("Variance", DoubleValue(1.0));
}

UwbTdoaEngine::~UwbTdoaEngine()
{
    NS_LOG_FUNCTION(this);
}

void
UwbTdoaEngine::DoDispose()
{
    for (auto& [key, collection] : m_pending)
    {
        collection.solve.Cancel();
    }
    m_pending.clear();
    m_anchors.clear();
    m_positionCallback.Nullify();
    Object::DoDispose();
}

void
UwbTdoaEngine::AddAnchor(Ptr<UwbMac> anchor, Vector position)
{
    NS_LOG_FUNCTION(this << anchor << position);
    const std::size_t index = m_anchors.size();
    Anchor entry;
    entry.mac = anchor;
    entry.position = position;
    entry.syncOffset = Seconds(m_syncError.GetSeconds() * m_syncNoise->GetValue());
    m_anchors.push_back(entry);

    anchor->SetBlinkCallback(
        MakeCallback(&UwbTdoaEngine::OnBlink, this).Bind(index));
}

std::size_t
UwbTdoaEngine::GetNAnchors() const
{
    return m_anchors.size();
}

void
UwbTdoaEngine::SetPositionCallback(PositionCallback callback)
{
    m_positionCallback = callback;
}

void
UwbTdoaEngine::SetSolveHeight(bool solveHeight)
{
    m_solveHeight = solveHeight;
}

int64_t
UwbTdoaEngine::AssignStreams(int64_t stream)
{
    m_syncNoise->SetStream(stream);
    return 1;
}

void
UwbTdoaEngine::OnBlink(std::size_t index, Mac16Address tag, uint8_t session, const UwbRxInfo& info)
{
    NS_LOG_FUNCTION(this << index << tag << +session);
    NS_ASSERT_MSG(index < m_anchors.size(), "A report came from an anchor that is not registered");
    const auto& anchor = m_anchors[index];

    // the anchor read its own counter, so the reading has to be brought back onto the common
    // time base the engine works in, and then displaced by whatever its synchronisation is out
    // by. With no synchronisation error the conversion is exact, which is the ideal a wired
    // installation is trying to approach
    auto clock = anchor.mac->GetPhy()->GetClockModel();
    const Time local = UwbClockModel::TicksToTime(info.rxTimestamp);
    const Time arrival = clock->LocalToGlobal(local) + anchor.syncOffset;

    const BlinkKey key{tag, session};
    auto& collection = m_pending[key];
    collection.arrivals[index] = arrival.GetSeconds();
    if (!collection.solve.IsPending())
    {
        collection.solve =
            Simulator::Schedule(m_collectionWindow, &UwbTdoaEngine::Solve, this, key);
    }
}

void
UwbTdoaEngine::Solve(BlinkKey key)
{
    NS_LOG_FUNCTION(this << key.first << +key.second);
    auto entry = m_pending.find(key);
    if (entry == m_pending.end())
    {
        return;
    }
    const auto arrivals = entry->second.arrivals;
    m_pending.erase(entry);

    std::vector<Vector> anchors;
    std::vector<double> offsets;
    anchors.reserve(arrivals.size());
    offsets.reserve(arrivals.size());

    // the first anchor to have heard the blink is the reference the others are measured against
    double reference = 0.0;
    bool first = true;
    for (const auto& [index, arrival] : arrivals)
    {
        if (first)
        {
            reference = arrival;
            first = false;
        }
        anchors.push_back(m_anchors[index].position);
        offsets.push_back(arrival - reference);
    }

    Vector position;
    const bool solved = SolveTdoa(anchors, offsets, m_solveHeight, position);
    m_positionTrace(key.first, position, solved);
    if (!m_positionCallback.IsNull())
    {
        m_positionCallback(key.first, position, solved);
    }
}

} // namespace uwb
} // namespace ns3
