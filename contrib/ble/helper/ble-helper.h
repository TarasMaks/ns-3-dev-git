/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_HELPER_H
#define BLE_HELPER_H

#include "ns3/ble-constants.h"
#include "ns3/ble-link-layer.h"
#include "ns3/ble-net-device.h"
#include "ns3/ble-phy.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/spectrum-channel.h"
#include "ns3/trace-helper.h"

#include <string>

namespace ns3
{
namespace ble
{

/**
 * @ingroup ble
 * Install BLE devices on nodes and attach them to a shared spectrum channel.
 *
 * The helper creates, for every node, a physical layer, a Link Layer and a network device, wires
 * them together and connects the physical layer to the channel. Attributes of each of the three
 * objects can be set before installing.
 *
 * @code
 *   BleHelper ble;
 *   ble.SetChannel(BleHelper::CreateChannel("ns3::LogDistancePropagationLossModel"));
 *   ble.SetPhyAttribute("PhyMode", EnumValue(BlePhyMode::LE_CODED_S8));
 *   ble.SetLinkLayerAttribute("ConnInterval", TimeValue(MilliSeconds(30)));
 *   NetDeviceContainer devices = ble.Install(nodes);
 * @endcode
 */
class BleHelper : public PcapHelperForDevice
{
  public:
    BleHelper();
    ~BleHelper() override;

    /**
     * Build a spectrum channel suitable for BLE, with a propagation loss model and a constant
     * speed propagation delay model.
     *
     * @param lossModel the TypeId name of the propagation loss model
     * @return the channel
     */
    static Ptr<SpectrumChannel> CreateChannel(
        const std::string& lossModel = "ns3::LogDistancePropagationLossModel");

    /**
     * @param channel the spectrum channel the installed devices transmit on
     */
    void SetChannel(Ptr<SpectrumChannel> channel);
    /// @return the spectrum channel
    Ptr<SpectrumChannel> GetChannel() const;

    /**
     * Set an attribute of the physical layers created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetPhyAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an attribute of the Link Layers created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetLinkLayerAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an attribute of the network devices created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetDeviceAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Install a BLE device on every node of a container.
     * @param nodes the nodes
     * @return the devices that were installed
     */
    NetDeviceContainer Install(NodeContainer nodes);

    /**
     * Install a BLE device on a node.
     * @param node the node
     * @return the device that was installed
     */
    Ptr<BleNetDevice> Install(Ptr<Node> node);

    /**
     * Establish a connection between two devices without going through the advertising and
     * scanning procedures, so that a study of the connection does not have to wait for them.
     *
     * @param central the device that becomes the central
     * @param peripheral the device that becomes the peripheral
     * @param interval the connection interval
     * @param start when the first anchor point occurs
     */
    static void ConnectStatically(Ptr<NetDevice> central,
                                  Ptr<NetDevice> peripheral,
                                  Time interval,
                                  Time start = Seconds(1));

    /**
     * Assign the streams of the random variables of the devices of a container.
     * @param devices the devices
     * @param stream the first stream index to use
     * @return the number of streams used
     */
    static int64_t AssignStreams(NetDeviceContainer devices, int64_t stream);

    /**
     * Turn on the logging of every component of the BLE model.
     * @param level the log level
     */
    static void EnableLogComponents(LogLevel level = LOG_LEVEL_INFO);

  private:
    void EnablePcapInternal(std::string prefix,
                            Ptr<NetDevice> nd,
                            bool promiscuous,
                            bool explicitFilename) override;

    ObjectFactory m_phyFactory;       //!< creates the physical layers
    ObjectFactory m_linkLayerFactory; //!< creates the Link Layers
    ObjectFactory m_deviceFactory;    //!< creates the network devices
    Ptr<SpectrumChannel> m_channel;   //!< the shared spectrum channel
};

} // namespace ble
} // namespace ns3

#endif /* BLE_HELPER_H */
