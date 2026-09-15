/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_NET_DEVICE_H
#define UWB_NET_DEVICE_H

#include "uwb-mac.h"
#include "uwb-phy.h"

#include "ns3/mac16-address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/traced-callback.h"

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * A UWB device, as the rest of ns-3 sees it.
 *
 * The device carries ordinary traffic, so the applications and the routing of ns-3 work over it
 * unchanged, and it also exposes the part that makes UWB what it is: StartRanging measures the
 * distance to a peer, and the result arrives through a callback with the range in metres.
 *
 * The maximum transmission unit is small. A frame of IEEE Std 802.15.4 holds 127 octets, of
 * which the MAC header takes nine and the frame check sequence two, leaving 116 for the
 * payload. That is what UWB is: a technology for knowing where something is, which happens to
 * be able to carry a little data as well.
 */
class UwbNetDevice : public NetDevice
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();

    UwbNetDevice();
    ~UwbNetDevice() override;

    /// @param phy the radio
    void SetPhy(Ptr<UwbPhy> phy);
    /// @return the radio
    Ptr<UwbPhy> GetPhy() const;

    /// @param mac the medium access layer
    void SetMac(Ptr<UwbMac> mac);
    /// @return the medium access layer
    Ptr<UwbMac> GetMac() const;

    /**
     * Measure the distance to a peer.
     *
     * @param peer the short address of the peer
     * @param method the ranging scheme to use
     * @return true if the exchange was started
     */
    bool StartRanging(Mac16Address peer, UwbRangingMethod method);

    /**
     * Send a blink for the anchors of a time difference of arrival system to timestamp.
     *
     * @return true if the blink was sent
     */
    bool SendBlink();

    /// @param callback invoked whenever a ranging exchange finishes
    void SetRangingResultCallback(UwbMac::RangingResultCallback callback);

    // inherited from NetDevice
    void SetIfIndex(const uint32_t index) override;
    uint32_t GetIfIndex() const override;
    Ptr<Channel> GetChannel() const override;
    void SetAddress(Address address) override;
    Address GetAddress() const override;
    bool SetMtu(const uint16_t mtu) override;
    uint16_t GetMtu() const override;
    bool IsLinkUp() const override;
    void AddLinkChangeCallback(Callback<void> callback) override;
    bool IsBroadcast() const override;
    Address GetBroadcast() const override;
    bool IsMulticast() const override;
    Address GetMulticast(Ipv4Address multicastGroup) const override;
    Address GetMulticast(Ipv6Address addr) const override;
    bool IsBridge() const override;
    bool IsPointToPoint() const override;
    bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber) override;
    bool SendFrom(Ptr<Packet> packet,
                  const Address& source,
                  const Address& dest,
                  uint16_t protocolNumber) override;
    Ptr<Node> GetNode() const override;
    void SetNode(Ptr<Node> node) override;
    bool NeedsArp() const override;
    void SetReceiveCallback(NetDevice::ReceiveCallback callback) override;
    void SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback callback) override;
    bool SupportsSendFrom() const override;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    /**
     * A data frame arrived.
     *
     * @param packet the payload
     * @param source who sent it
     */
    void Receive(Ptr<Packet> packet, Mac16Address source);

    Ptr<Node> m_node;   //!< the node this device belongs to
    Ptr<UwbPhy> m_phy;  //!< the radio
    Ptr<UwbMac> m_mac;  //!< the medium access layer
    uint32_t m_ifIndex; //!< the interface index
    uint16_t m_mtu;     //!< the largest payload a frame can carry
    bool m_linkUp;      //!< whether the link is up

    NetDevice::ReceiveCallback m_receiveCallback; //!< delivers a payload upwards
    TracedCallback<> m_linkChangeCallbacks;       //!< fired when the link comes up
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_NET_DEVICE_H */
