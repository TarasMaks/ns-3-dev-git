/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-helper.h"

#include "ns3/abort.h"
#include "ns3/ble-spectrum-value-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/names.h"
#include "ns3/pcap-file-wrapper.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/simulator.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BleHelper");

namespace ble
{

namespace
{

/// Link type of a Bluetooth Low Energy link layer capture, as Wireshark expects it.
constexpr uint16_t LINKTYPE_BLUETOOTH_LE_LL = 251;

/**
 * Write a packet to a capture file in the format of a Bluetooth Low Energy link layer capture,
 * which begins with the access address and is followed by the PDU and the CRC.
 *
 * @param file the capture file
 * @param phy the PHY the packet passed through, which holds the access address in use
 * @param packet the packet, made of the PDU and the CRC
 */
void
WriteLeLinkLayerPacket(Ptr<PcapFileWrapper> file, Ptr<BlePhy> phy, Ptr<const Packet> packet)
{
    if (!packet || !file || !phy)
    {
        return;
    }
    const uint32_t accessAddress = phy->GetAccessAddress();
    uint8_t header[ACCESS_ADDRESS_OCTETS];
    for (uint8_t i = 0; i < ACCESS_ADDRESS_OCTETS; ++i)
    {
        header[i] = static_cast<uint8_t>((accessAddress >> (8 * i)) & 0xFF);
    }
    auto record = Create<Packet>(header, ACCESS_ADDRESS_OCTETS);
    record->AddAtEnd(packet->Copy());
    file->Write(Simulator::Now(), record);
}

/**
 * Capture sink of the transmissions of a PHY.
 *
 * @param file the capture file
 * @param phy the PHY
 * @param packet the packet being transmitted
 * @param channel the channel it is transmitted on
 * @param powerDbm the power it is transmitted with
 */
void
PcapSinkTx(Ptr<PcapFileWrapper> file,
           Ptr<BlePhy> phy,
           Ptr<const Packet> packet,
           uint8_t channel,
           double powerDbm)
{
    WriteLeLinkLayerPacket(file, phy, packet);
}

/**
 * Capture sink of the successful receptions of a PHY.
 *
 * @param file the capture file
 * @param phy the PHY
 * @param packet the packet that was received
 * @param rssiDbm the power it was received with
 */
void
PcapSinkRx(Ptr<PcapFileWrapper> file, Ptr<BlePhy> phy, Ptr<const Packet> packet, double rssiDbm)
{
    WriteLeLinkLayerPacket(file, phy, packet);
}

} // namespace

BleHelper::BleHelper()
{
    m_phyFactory.SetTypeId(BlePhy::GetTypeId());
    m_linkLayerFactory.SetTypeId(BleLinkLayer::GetTypeId());
    m_deviceFactory.SetTypeId(BleNetDevice::GetTypeId());
}

BleHelper::~BleHelper()
{
}

Ptr<SpectrumChannel>
BleHelper::CreateChannel(const std::string& lossModel)
{
    auto channel = CreateObject<MultiModelSpectrumChannel>();
    ObjectFactory factory;
    factory.SetTypeId(lossModel);
    auto loss = factory.Create<PropagationLossModel>();
    channel->AddPropagationLossModel(loss);
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    return channel;
}

void
BleHelper::SetChannel(Ptr<SpectrumChannel> channel)
{
    m_channel = channel;
}

Ptr<SpectrumChannel>
BleHelper::GetChannel() const
{
    return m_channel;
}

void
BleHelper::SetPhyAttribute(const std::string& name, const AttributeValue& value)
{
    m_phyFactory.Set(name, value);
}

void
BleHelper::SetLinkLayerAttribute(const std::string& name, const AttributeValue& value)
{
    m_linkLayerFactory.Set(name, value);
}

void
BleHelper::SetDeviceAttribute(const std::string& name, const AttributeValue& value)
{
    m_deviceFactory.Set(name, value);
}

Ptr<BleNetDevice>
BleHelper::Install(Ptr<Node> node)
{
    NS_ABORT_MSG_IF(!m_channel, "A spectrum channel must be set before installing devices");

    auto phy = m_phyFactory.Create<BlePhy>();
    auto linkLayer = m_linkLayerFactory.Create<BleLinkLayer>();
    auto device = m_deviceFactory.Create<BleNetDevice>();

    // every device gets a device address derived from a global counter, as a real one gets it
    // from its manufacturer
    static uint64_t nextAddress = 1;
    Mac48Address address;
    uint8_t buffer[6];
    const uint64_t value = nextAddress++;
    for (uint8_t i = 0; i < 6; ++i)
    {
        buffer[5 - i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }
    address.CopyFrom(buffer);
    linkLayer->SetAddress(address);

    auto mobility = node->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(!mobility,
                    "Node " << node->GetId() << " needs a mobility model before a BLE device");
    phy->SetMobility(mobility);

    device->SetNode(node);
    device->SetPhy(phy);
    device->SetLinkLayer(linkLayer);
    device->SetChannel(m_channel);
    node->AddDevice(device);
    return device;
}

NetDeviceContainer
BleHelper::Install(NodeContainer nodes)
{
    NetDeviceContainer devices;
    for (auto it = nodes.Begin(); it != nodes.End(); ++it)
    {
        devices.Add(Install(*it));
    }
    return devices;
}

void
BleHelper::ConnectStatically(Ptr<NetDevice> central,
                             Ptr<NetDevice> peripheral,
                             Time interval,
                             Time start)
{
    auto centralDevice = DynamicCast<BleNetDevice>(central);
    auto peripheralDevice = DynamicCast<BleNetDevice>(peripheral);
    NS_ABORT_MSG_IF(!centralDevice || !peripheralDevice, "Two BLE devices are required");
    // the link layers must be initialized before their connection is set up
    centralDevice->Initialize();
    peripheralDevice->Initialize();
    BleLinkLayer::SetupStaticConnection(centralDevice->GetLinkLayer(),
                                        peripheralDevice->GetLinkLayer(),
                                        interval,
                                        start);
}

int64_t
BleHelper::AssignStreams(NetDeviceContainer devices, int64_t stream)
{
    int64_t used = 0;
    for (auto it = devices.Begin(); it != devices.End(); ++it)
    {
        auto device = DynamicCast<BleNetDevice>(*it);
        if (!device)
        {
            continue;
        }
        if (auto phy = device->GetPhy())
        {
            used += phy->AssignStreams(stream + used);
        }
        if (auto linkLayer = device->GetLinkLayer())
        {
            used += linkLayer->AssignStreams(stream + used);
        }
    }
    return used;
}

void
BleHelper::EnableLogComponents(LogLevel level)
{
    LogComponentEnable("BleErrorModel", level);
    LogComponentEnable("BleHelper", level);
    LogComponentEnable("BleInterferenceHelper", level);
    LogComponentEnable("BleLinkLayer", level);
    LogComponentEnable("BleNetDevice", level);
    LogComponentEnable("BlePhy", level);
    LogComponentEnable("BleSpectrumValueHelper", level);
}

void
BleHelper::EnablePcapInternal(std::string prefix,
                              Ptr<NetDevice> nd,
                              bool promiscuous,
                              bool explicitFilename)
{
    auto device = DynamicCast<BleNetDevice>(nd);
    if (!device)
    {
        NS_LOG_INFO("BleHelper::EnablePcapInternal(): device " << nd << " is not a BLE device");
        return;
    }

    PcapHelper pcapHelper;
    std::string filename =
        explicitFilename ? prefix : pcapHelper.GetFilenameFromDevice(prefix, device);
    auto file = pcapHelper.CreateFile(filename, std::ios::out, LINKTYPE_BLUETOOTH_LE_LL);

    auto phy = device->GetPhy();
    phy->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PcapSinkTx, file, phy));
    phy->TraceConnectWithoutContext("PhyRxEnd", MakeBoundCallback(&PcapSinkRx, file, phy));
}

} // namespace ble
} // namespace ns3
