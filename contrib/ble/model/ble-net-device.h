/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_NET_DEVICE_H
#define BLE_NET_DEVICE_H

#include "ble-link-layer.h"
#include "ble-phy.h"

#include "ns3/mac48-address.h"
#include "ns3/net-device.h"
#include "ns3/node.h"
#include "ns3/traced-callback.h"

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * A network device that carries traffic over a BLE connection.
 *
 * The device owns a physical layer and a Link Layer and presents them to the rest of ns-3 as a
 * normal NetDevice, so that applications, the Internet stack and 6LoWPAN can run on top of it.
 * The device address is the 48-bit device address of the Link Layer.
 *
 * The default maximum transmission unit is 1280 octets, the smallest an IPv6 link may have,
 * which is what RFC 7668 requires of the L2CAP channel used to carry IPv6 over BLE. Payloads
 * larger than the PDU of the connection are fragmented by the Link Layer.
 */
class BleNetDevice : public NetDevice
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();

    BleNetDevice();
    ~BleNetDevice() override;

    /**
     * @param phy the physical layer of the device
     */
    void SetPhy(Ptr<BlePhy> phy);
    /// @return the physical layer of the device
    Ptr<BlePhy> GetPhy() const;

    /**
     * @param linkLayer the Link Layer of the device
     */
    void SetLinkLayer(Ptr<BleLinkLayer> linkLayer);
    /// @return the Link Layer of the device
    Ptr<BleLinkLayer> GetLinkLayer() const;

    /**
     * @param channel the spectrum channel the physical layer transmits on
     */
    void SetChannel(Ptr<SpectrumChannel> channel);

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
    void SetReceiveCallback(NetDevice::ReceiveCallback cb) override;
    void SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb) override;
    bool SupportsSendFrom() const override;

  protected:
    void DoDispose() override;
    void DoInitialize() override;

  private:
    /**
     * Invoked by the Link Layer with every payload it reassembles.
     * @param packet the payload
     * @param from the address it came from
     */
    void OnReceive(Ptr<Packet> packet, Mac48Address from);

    /// Tell the upper layers that the link came up or went down.
    void NotifyLinkChange();

    Ptr<Node> m_node;               //!< the node the device is installed on
    Ptr<BlePhy> m_phy;              //!< the physical layer
    Ptr<BleLinkLayer> m_linkLayer;  //!< the Link Layer
    Ptr<SpectrumChannel> m_channel; //!< the spectrum channel
    uint32_t m_ifIndex;             //!< the interface index
    uint16_t m_mtu;                 //!< the maximum transmission unit
    bool m_linkUp;                  //!< whether a connection is established

    NetDevice::ReceiveCallback m_rxCallback;               //!< delivers packets upwards
    NetDevice::PromiscReceiveCallback m_promiscRxCallback; //!< delivers packets to a sniffer
    TracedCallback<> m_linkChangeCallbacks;                //!< invoked when the link changes

    /// Traced with every packet handed down by the upper layer.
    TracedCallback<Ptr<const Packet>> m_macTxTrace;
    /// Traced with every packet that could not be queued.
    TracedCallback<Ptr<const Packet>> m_macTxDropTrace;
    /// Traced with every packet delivered to the upper layer.
    TracedCallback<Ptr<const Packet>> m_macRxTrace;
};

} // namespace ble
} // namespace ns3

#endif /* BLE_NET_DEVICE_H */
