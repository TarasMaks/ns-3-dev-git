/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "uwb-helper.h"

#include "ns3/abort.h"
#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/names.h"
#include "ns3/pcap-file-wrapper.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/uwb-clock-model.h"
#include "ns3/uwb-utils.h"


namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UwbHelper");

namespace uwb
{

namespace
{

/**
 * The link type Wireshark uses for IEEE 802.15.4 frames that still carry their frame check
 * sequence, which is what this module puts on the air.
 */
constexpr uint16_t LINKTYPE_IEEE802_15_4_WITHFCS = 195;

/// @param file the capture file
/// @param packet the frame
void
PcapSinkTx(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet, uint8_t, double)
{
    file->Write(Simulator::Now(), packet);
}

/// @param file the capture file
/// @param packet the frame
void
PcapSinkRx(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet, const UwbRxInfo&)
{
    file->Write(Simulator::Now(), packet);
}

} // namespace

UwbHelper::UwbHelper()
    : m_preambleCode(9),
      m_panId(1),
      m_nextAddress(1)
{
    // the module cannot work at the default resolution of one nanosecond, which is thirty
    // centimetres of propagation, so the clock is raised before anything creates a time
    EnableUwbTimeResolution();

    m_phyFactory.SetTypeId(UwbPhy::GetTypeId());
    m_macFactory.SetTypeId(UwbMac::GetTypeId());
    m_deviceFactory.SetTypeId(UwbNetDevice::GetTypeId());
    m_errorModelFactory.SetTypeId(UwbErrorModel::GetTypeId());

    // every crystal is different, and a model in which they were not would make the two ranging
    // schemes look alike
    auto offsets = CreateObject<UniformRandomVariable>();
    offsets->SetAttribute("Min", DoubleValue(-20.0));
    offsets->SetAttribute("Max", DoubleValue(20.0));
    m_clockOffset = offsets;
}

UwbHelper::~UwbHelper()
{
}

Ptr<SpectrumChannel>
UwbHelper::CreateChannel(uint8_t channelNumber, const std::string& lossModel)
{
    EnableUwbTimeResolution();

    auto channel = CreateObject<MultiModelSpectrumChannel>();

    ObjectFactory factory;
    factory.SetTypeId(lossModel);
    auto loss = factory.Create<PropagationLossModel>();

    // ns-3 evaluates a propagation loss model at the frequency it holds, not at the frequency of
    // the signal, so a model that has one has to be told where the channel sits
    const double frequencyHz = ChannelToFrequencyMhz(channelNumber) * 1e6;
    TypeId::AttributeInformation info;
    if (loss->GetInstanceTypeId().LookupAttributeByName("Frequency", &info))
    {
        loss->SetAttribute("Frequency", DoubleValue(frequencyHz));
    }
    else if (loss->GetInstanceTypeId().LookupAttributeByName("ReferenceLoss", &info))
    {
        // a log distance model states its loss at one metre instead, which for an isotropic
        // antenna is the free space loss at that distance
        const double referenceLoss =
            20.0 * std::log10(4.0 * M_PI * frequencyHz / SPEED_OF_LIGHT);
        loss->SetAttribute("ReferenceLoss", DoubleValue(referenceLoss));
    }
    channel->AddPropagationLossModel(loss);
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    return channel;
}

void
UwbHelper::SetChannel(Ptr<SpectrumChannel> channel)
{
    m_channel = channel;
}

Ptr<SpectrumChannel>
UwbHelper::GetChannel() const
{
    return m_channel;
}

void
UwbHelper::SetPhyAttribute(const std::string& name, const AttributeValue& value)
{
    m_phyFactory.Set(name, value);
}

void
UwbHelper::SetMacAttribute(const std::string& name, const AttributeValue& value)
{
    m_macFactory.Set(name, value);
}

void
UwbHelper::SetDeviceAttribute(const std::string& name, const AttributeValue& value)
{
    m_deviceFactory.Set(name, value);
}

void
UwbHelper::SetErrorModelAttribute(const std::string& name, const AttributeValue& value)
{
    m_errorModelFactory.Set(name, value);
}

void
UwbHelper::SetChannelNumber(uint8_t channelNumber)
{
    m_config.channel = channelNumber;
}

void
UwbHelper::SetDataRate(UwbDataRate rate)
{
    m_config.dataRate = rate;
}

void
UwbHelper::SetPrf(UwbPrf prf)
{
    m_config.prf = prf;
}

void
UwbHelper::SetPreambleSymbols(uint32_t symbols)
{
    m_config.preambleSymbols = symbols;
}

void
UwbHelper::SetPreambleCode(uint8_t code)
{
    m_preambleCode = code;
}

void
UwbHelper::SetPanId(uint16_t panId)
{
    m_panId = panId;
}

const UwbPhyConfig&
UwbHelper::GetPhyConfig() const
{
    return m_config;
}

void
UwbHelper::SetClockOffsetModel(Ptr<RandomVariableStream> stream)
{
    m_clockOffset = stream;
}

Ptr<UwbNetDevice>
UwbHelper::Install(Ptr<Node> node)
{
    NS_ABORT_MSG_IF(!m_channel, "Call SetChannel before installing devices");
    auto mobility = node->GetObject<MobilityModel>();
    NS_ABORT_MSG_IF(!mobility,
                    "Node " << node->GetId()
                            << " has no mobility model, and a ranging model needs positions");

    auto phy = m_phyFactory.Create<UwbPhy>();
    phy->SetConfig(m_config);
    phy->SetPreambleCode(m_preambleCode);
    phy->SetTxPowerToRegulatoryLimit();
    phy->SetErrorModel(m_errorModelFactory.Create<UwbErrorModel>());
    phy->SetMobility(mobility);
    phy->SetChannel(m_channel);
    m_channel->AddRx(phy);

    auto clock = CreateObject<UwbClockModel>();
    clock->RandomiseFrequencyOffset(m_clockOffset);
    phy->SetClockModel(clock);

    auto mac = m_macFactory.Create<UwbMac>();
    mac->SetPhy(phy);
    mac->SetPanId(m_panId);
    const uint8_t address[2]{static_cast<uint8_t>(m_nextAddress >> 8),
                             static_cast<uint8_t>(m_nextAddress & 0xFF)};
    Mac16Address shortAddress;
    shortAddress.CopyFrom(address);
    mac->SetAddress(shortAddress);
    ++m_nextAddress;

    auto device = m_deviceFactory.Create<UwbNetDevice>();
    device->SetPhy(phy);
    device->SetMac(mac);
    device->SetNode(node);
    node->AddDevice(device);
    return device;
}

NetDeviceContainer
UwbHelper::Install(NodeContainer nodes)
{
    NetDeviceContainer devices;
    for (auto node = nodes.Begin(); node != nodes.End(); ++node)
    {
        devices.Add(Install(*node));
    }
    return devices;
}

Ptr<UwbTdoaEngine>
UwbHelper::CreateTdoaEngine(NetDeviceContainer anchors)
{
    auto engine = CreateObject<UwbTdoaEngine>();
    for (uint32_t i = 0; i < anchors.GetN(); ++i)
    {
        auto device = DynamicCast<UwbNetDevice>(anchors.Get(i));
        NS_ABORT_MSG_IF(!device, "An anchor that is not a UWB device was given to the engine");
        auto mobility = device->GetNode()->GetObject<MobilityModel>();
        NS_ABORT_MSG_IF(!mobility, "An anchor has no position");
        engine->AddAnchor(device->GetMac(), mobility->GetPosition());
    }
    return engine;
}

int64_t
UwbHelper::AssignStreams(NetDeviceContainer devices, int64_t stream)
{
    int64_t used = 0;
    for (uint32_t i = 0; i < devices.GetN(); ++i)
    {
        auto device = DynamicCast<UwbNetDevice>(devices.Get(i));
        if (!device)
        {
            continue;
        }
        used += device->GetPhy()->AssignStreams(stream + used);
        used += device->GetMac()->AssignStreams(stream + used);
    }
    return used;
}

void
UwbHelper::EnablePcapInternal(std::string prefix,
                              Ptr<NetDevice> nd,
                              bool promiscuous,
                              bool explicitFilename)
{
    auto device = DynamicCast<UwbNetDevice>(nd);
    if (!device)
    {
        NS_LOG_INFO("UwbHelper::EnablePcapInternal(): device " << nd << " is not a UWB device");
        return;
    }

    PcapHelper pcapHelper;
    const std::string filename = explicitFilename
                                     ? prefix
                                     : pcapHelper.GetFilenameFromDevice(prefix, device);
    auto file = pcapHelper.CreateFile(filename, std::ios::out, LINKTYPE_IEEE802_15_4_WITHFCS);

    auto phy = device->GetPhy();
    phy->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PcapSinkTx, file));
    phy->TraceConnectWithoutContext("PhyRxEnd", MakeBoundCallback(&PcapSinkRx, file));
}

} // namespace uwb
} // namespace ns3
