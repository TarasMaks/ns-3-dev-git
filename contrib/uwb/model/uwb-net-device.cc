/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-net-device.h"

#include "ns3/abort.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbNetDevice");

namespace uwb
{

namespace
{
/// The payload a 127 octet frame leaves once the MAC header and the check sequence are taken.
constexpr uint16_t DEFAULT_MTU = MAX_PSDU_OCTETS - 9 - FCS_OCTETS;
} // namespace

NS_OBJECT_ENSURE_REGISTERED(UwbNetDevice);

TypeId
UwbNetDevice::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::uwb::UwbNetDevice")
            .SetParent<NetDevice>()
            .SetGroupName("Uwb")
            .AddConstructor<UwbNetDevice>()
            .AddAttribute("Mtu",
                          "The largest payload a frame can carry. A frame of IEEE Std 802.15.4 "
                          "is 127 octets, of which the MAC header takes nine and the frame check "
                          "sequence two.",
                          UintegerValue(DEFAULT_MTU),
                          MakeUintegerAccessor(&UwbNetDevice::SetMtu, &UwbNetDevice::GetMtu),
                          MakeUintegerChecker<uint16_t>(1, MAX_PSDU_OCTETS_EXTENDED))
            .AddAttribute("Phy",
                          "The radio of this device.",
                          PointerValue(),
                          MakePointerAccessor(&UwbNetDevice::SetPhy, &UwbNetDevice::GetPhy),
                          MakePointerChecker<UwbPhy>())
            .AddAttribute("Mac",
                          "The medium access layer of this device.",
                          PointerValue(),
                          MakePointerAccessor(&UwbNetDevice::SetMac, &UwbNetDevice::GetMac),
                          MakePointerChecker<UwbMac>());
    return tid;
}

UwbNetDevice::UwbNetDevice()
    : m_ifIndex(0),
      m_mtu(DEFAULT_MTU),
      m_linkUp(false)
{
    NS_LOG_FUNCTION(this);
}

UwbNetDevice::~UwbNetDevice()
{
    NS_LOG_FUNCTION(this);
}

void
UwbNetDevice::DoDispose()
{
    NS_LOG_FUNCTION(this);
    if (m_mac)
    {
        m_mac->Dispose();
    }
    if (m_phy)
    {
        m_phy->Dispose();
    }
    m_mac = nullptr;
    m_phy = nullptr;
    m_node = nullptr;
    m_receiveCallback.Nullify();
    NetDevice::DoDispose();
}

void
UwbNetDevice::DoInitialize()
{
    NS_LOG_FUNCTION(this);
    if (m_phy)
    {
        m_phy->Initialize();
    }
    if (m_mac)
    {
        m_mac->Initialize();
    }
    m_linkUp = true;
    m_linkChangeCallbacks();
    NetDevice::DoInitialize();
}

void
UwbNetDevice::SetPhy(Ptr<UwbPhy> phy)
{
    m_phy = phy;
    if (m_phy)
    {
        m_phy->SetDevice(this);
    }
}

Ptr<UwbPhy>
UwbNetDevice::GetPhy() const
{
    return m_phy;
}

void
UwbNetDevice::SetMac(Ptr<UwbMac> mac)
{
    m_mac = mac;
    if (m_mac)
    {
        m_mac->SetReceiveCallback(MakeCallback(&UwbNetDevice::Receive, this));
    }
}

Ptr<UwbMac>
UwbNetDevice::GetMac() const
{
    return m_mac;
}

bool
UwbNetDevice::StartRanging(Mac16Address peer, UwbRangingMethod method)
{
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    return m_mac->StartRanging(peer, method);
}

bool
UwbNetDevice::SendBlink()
{
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    return m_mac->SendBlink();
}

void
UwbNetDevice::SetRangingResultCallback(UwbMac::RangingResultCallback callback)
{
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    m_mac->SetRangingResultCallback(callback);
}

void
UwbNetDevice::Receive(Ptr<Packet> packet, Mac16Address source)
{
    NS_LOG_FUNCTION(this << packet << source);
    if (!m_receiveCallback.IsNull())
    {
        m_receiveCallback(this, packet, 0, source);
    }
}

/* -------------------------------------------------------------------------------------------- */
/* NetDevice                                                                                      */
/* -------------------------------------------------------------------------------------------- */

void
UwbNetDevice::SetIfIndex(const uint32_t index)
{
    m_ifIndex = index;
}

uint32_t
UwbNetDevice::GetIfIndex() const
{
    return m_ifIndex;
}

Ptr<Channel>
UwbNetDevice::GetChannel() const
{
    // the spectrum channel is not a Channel, so there is nothing meaningful to hand back
    return nullptr;
}

void
UwbNetDevice::SetAddress(Address address)
{
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    m_mac->SetAddress(Mac16Address::ConvertFrom(address));
}

Address
UwbNetDevice::GetAddress() const
{
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    return m_mac->GetAddress();
}

bool
UwbNetDevice::SetMtu(const uint16_t mtu)
{
    m_mtu = mtu;
    return true;
}

uint16_t
UwbNetDevice::GetMtu() const
{
    return m_mtu;
}

bool
UwbNetDevice::IsLinkUp() const
{
    return m_linkUp;
}

void
UwbNetDevice::AddLinkChangeCallback(Callback<void> callback)
{
    m_linkChangeCallbacks.ConnectWithoutContext(callback);
}

bool
UwbNetDevice::IsBroadcast() const
{
    return true;
}

Address
UwbNetDevice::GetBroadcast() const
{
    return Mac16Address::GetBroadcast();
}

bool
UwbNetDevice::IsMulticast() const
{
    return true;
}

Address
UwbNetDevice::GetMulticast(Ipv4Address multicastGroup) const
{
    // a sixteen bit address space has no room for a mapping of IPv4 multicast groups, so a
    // group is reached the only way it can be, by broadcasting to the network
    return Mac16Address::GetBroadcast();
}

Address
UwbNetDevice::GetMulticast(Ipv6Address addr) const
{
    return Mac16Address::GetMulticast(addr);
}

bool
UwbNetDevice::IsBridge() const
{
    return false;
}

bool
UwbNetDevice::IsPointToPoint() const
{
    return false;
}

bool
UwbNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet << dest << protocolNumber);
    NS_ABORT_MSG_IF(!m_mac, "The device has no medium access layer");
    if (packet->GetSize() > m_mtu)
    {
        NS_LOG_WARN("A payload of " << packet->GetSize() << " octets does not fit in a frame");
        return false;
    }
    return m_mac->Enqueue(packet, Mac16Address::ConvertFrom(dest));
}

bool
UwbNetDevice::SendFrom(Ptr<Packet> packet,
                       const Address& source,
                       const Address& dest,
                       uint16_t protocolNumber)
{
    NS_LOG_FUNCTION(this << packet << source << dest);
    return false;
}

Ptr<Node>
UwbNetDevice::GetNode() const
{
    return m_node;
}

void
UwbNetDevice::SetNode(Ptr<Node> node)
{
    m_node = node;
}

bool
UwbNetDevice::NeedsArp() const
{
    return false;
}

void
UwbNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback callback)
{
    m_receiveCallback = callback;
}

void
UwbNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback callback)
{
}

bool
UwbNetDevice::SupportsSendFrom() const
{
    return false;
}

} // namespace uwb
} // namespace ns3
