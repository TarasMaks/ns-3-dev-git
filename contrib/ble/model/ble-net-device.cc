/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-net-device.h"

#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleNetDevice");

namespace ble
{

NS_OBJECT_ENSURE_REGISTERED(BleNetDevice);

TypeId
BleNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::ble::BleNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Ble")
            .AddConstructor<BleNetDevice>()
            .AddAttribute("Mtu",
                          "The maximum transmission unit. RFC 7668 requires at least 1280 octets "
                          "to carry IPv6 over BLE.",
                          UintegerValue(1280),
                          MakeUintegerAccessor(&BleNetDevice::SetMtu, &BleNetDevice::GetMtu),
                          MakeUintegerChecker<uint16_t>(23, 65527))
            .AddAttribute("Phy",
                          "The physical layer of the device.",
                          PointerValue(),
                          MakePointerAccessor(&BleNetDevice::SetPhy, &BleNetDevice::GetPhy),
                          MakePointerChecker<BlePhy>())
            .AddAttribute(
                "LinkLayer",
                "The Link Layer of the device.",
                PointerValue(),
                MakePointerAccessor(&BleNetDevice::SetLinkLayer, &BleNetDevice::GetLinkLayer),
                MakePointerChecker<BleLinkLayer>())
            .AddTraceSource("MacTx",
                            "A packet was handed down by the upper layer.",
                            MakeTraceSourceAccessor(&BleNetDevice::m_macTxTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MacTxDrop",
                            "A packet could not be queued.",
                            MakeTraceSourceAccessor(&BleNetDevice::m_macTxDropTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource("MacRx",
                            "A packet was delivered to the upper layer.",
                            MakeTraceSourceAccessor(&BleNetDevice::m_macRxTrace),
                            "ns3::Packet::TracedCallback");
    return tid;
}

BleNetDevice::BleNetDevice()
    : m_ifIndex(0),
      m_mtu(1280),
      m_linkUp(false)
{
    NS_LOG_FUNCTION(this);
}

BleNetDevice::~BleNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
BleNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    if (m_phy)
    {
        m_phy->Initialize();
    }
    if (m_linkLayer)
    {
        m_linkLayer->SetReceiveCallback(MakeCallback(&BleNetDevice::OnReceive, this));
        m_linkLayer->Initialize();
    }
    NetDevice::DoInitialize();
}

void
BleNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_phy)
    {
        m_phy->Dispose();
        m_phy = nullptr;
    }
    if (m_linkLayer)
    {
        m_linkLayer->Dispose();
        m_linkLayer = nullptr;
    }
    m_node = nullptr;
    m_channel = nullptr;
    m_rxCallback.Nullify();
    m_promiscRxCallback.Nullify();
    NetDevice::DoDispose();
}

void
BleNetDevice::SetPhy(Ptr<BlePhy> phy)
{
    m_phy = phy;
    if (m_phy)
    {
        m_phy->SetDevice(this);
    }
    if (m_phy && m_linkLayer)
    {
        m_linkLayer->SetPhy(m_phy);
    }
}

Ptr<BlePhy>
BleNetDevice::GetPhy() const
{
    return m_phy;
}

void
BleNetDevice::SetLinkLayer(Ptr<BleLinkLayer> linkLayer)
{
    m_linkLayer = linkLayer;
    if (m_linkLayer)
    {
        m_linkLayer->SetReceiveCallback(MakeCallback(&BleNetDevice::OnReceive, this));
        if (m_phy)
        {
            m_linkLayer->SetPhy(m_phy);
        }
    }
}

Ptr<BleLinkLayer>
BleNetDevice::GetLinkLayer() const
{
    return m_linkLayer;
}

void
BleNetDevice::SetChannel(Ptr<SpectrumChannel> channel)
{
    m_channel = channel;
    if (m_phy)
    {
        m_phy->SetChannel(channel);
    }
}

void
BleNetDevice::SetIfIndex(const uint32_t index)
{
    m_ifIndex = index;
}

uint32_t
BleNetDevice::GetIfIndex() const
{
    return m_ifIndex;
}

Ptr<Channel>
BleNetDevice::GetChannel() const
{
    return m_channel;
}

void
BleNetDevice::SetAddress(Address address)
{
    NS_ABORT_MSG_IF(!Mac48Address::IsMatchingType(address),
                    "A BLE device address is a 48-bit address");
    if (m_linkLayer)
    {
        m_linkLayer->SetAddress(Mac48Address::ConvertFrom(address));
    }
}

Address
BleNetDevice::GetAddress() const
{
    return m_linkLayer ? Address(m_linkLayer->GetAddress()) : Address();
}

bool
BleNetDevice::SetMtu(const uint16_t mtu)
{
    m_mtu = mtu;
    return true;
}

uint16_t
BleNetDevice::GetMtu() const
{
    return m_mtu;
}

bool
BleNetDevice::IsLinkUp() const
{
    return m_linkLayer && m_linkLayer->IsConnected();
}

void
BleNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    m_linkChangeCallbacks.ConnectWithoutContext(callback);
}

void
BleNetDevice::NotifyLinkChange()
{
    const bool up = IsLinkUp();
    if (up != m_linkUp)
    {
        m_linkUp = up;
        m_linkChangeCallbacks();
    }
}

bool
BleNetDevice::IsBroadcast() const
{
    return true;
}

Address
BleNetDevice::GetBroadcast() const
{
    return Mac48Address::GetBroadcast();
}

bool
BleNetDevice::IsMulticast() const
{
    return true;
}

Address
BleNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    return Mac48Address::GetMulticast(multicastGroup);
}

Address
BleNetDevice::GetMulticast(Ipv6Address addr) const
{
    return Mac48Address::GetMulticast(addr);
}

bool
BleNetDevice::IsBridge() const
{
    return false;
}

bool
BleNetDevice::IsPointToPoint() const
{
    // a connection joins exactly two devices
    return true;
}

bool
BleNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet->GetSize() << dest << protocolNumber);
    if (!m_linkLayer || !m_linkLayer->IsConnected())
    {
        NS_LOG_WARN("The device is not connected, the packet is dropped");
        m_macTxDropTrace(packet);
        return false;
    }
    m_macTxTrace(packet);
    const auto destination = Mac48Address::IsMatchingType(dest) ? Mac48Address::ConvertFrom(dest)
                                                                : Mac48Address::GetBroadcast();
    if (!m_linkLayer->Enqueue(packet, destination))
    {
        m_macTxDropTrace(packet);
        return false;
    }
    NotifyLinkChange();
    return true;
}

bool
BleNetDevice::SendFrom(Ptr<Packet> packet,
                       const Address& source,
                       const Address& dest,
                       uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet->GetSize() << source << dest);
    // a connection carries the traffic of its own two devices only
    return Send(packet, dest, protocolNumber);
}

Ptr<Node>
BleNetDevice::GetNode() const
{
    return m_node;
}

void
BleNetDevice::SetNode(Ptr<Node> node)
{
    m_node = node;
}

bool
BleNetDevice::NeedsArp() const
{
    // the peer of a connection is known, so there is nothing to resolve
    return false;
}

void
BleNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
{
    m_rxCallback = cb;
}

void
BleNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb)
{
    m_promiscRxCallback = cb;
}

bool
BleNetDevice::SupportsSendFrom() const
{
    return false;
}

void
BleNetDevice::OnReceive(Ptr<Packet> packet, Mac48Address from)
{
    NS_LOG_FUNCTION(this << packet->GetSize() << from);
    m_macRxTrace(packet);
    NotifyLinkChange();
    // the model carries no protocol field, so IPv6 is assumed, as RFC 7668 does for BLE
    constexpr uint16_t IPV6_PROTOCOL_NUMBER = 0x86DD;
    if (!m_promiscRxCallback.IsNull())
    {
        m_promiscRxCallback(this,
                            packet->Copy(),
                            IPV6_PROTOCOL_NUMBER,
                            from,
                            GetAddress(),
                            NetDevice::PACKET_HOST);
    }
    if (!m_rxCallback.IsNull())
    {
        m_rxCallback(this, packet, IPV6_PROTOCOL_NUMBER, from);
    }
}

} // namespace ble
} // namespace ns3
