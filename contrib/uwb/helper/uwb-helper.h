/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef UWB_HELPER_H
#define UWB_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/random-variable-stream.h"
#include "ns3/spectrum-channel.h"
#include "ns3/trace-helper.h"
#include "ns3/uwb-net-device.h"
#include "ns3/uwb-tdoa-engine.h"

#include <string>

namespace ns3
{
namespace uwb
{

/**
 * @ingroup uwb
 * Install UWB devices on nodes and attach them to a shared spectrum channel.
 *
 * The helper builds, for every node, a radio, a crystal, a medium access layer and a network
 * device, wires them together and connects the radio to the channel. It also raises the
 * resolution of the simulator clock to femtoseconds, which this module needs and which has to
 * happen before anything else creates a time.
 *
 * Two things the helper does are worth knowing about, because scripts that build the pieces by
 * hand get them wrong.
 *
 * The first is the frequency. ns-3 evaluates a propagation loss model at the frequency the
 * model is configured with rather than at the frequency of the signal, so a Friis model left at
 * its default would compute the loss of a 5.15 GHz link for a channel sitting at 6.5 or 8 GHz,
 * and every received power would be two or three dB out. CreateChannel sets the frequency from
 * the channel being used.
 *
 * The second is the crystals. Two devices whose clocks agree perfectly would make single-sided
 * ranging look as good as double-sided, which is the one thing a UWB model must not do. The
 * helper therefore gives every device a frequency offset drawn from a random variable, twenty
 * parts per million wide by default, which is what IEEE Std 802.15.4 permits.
 *
 * @code
 *   UwbHelper uwb;
 *   uwb.SetChannelNumber(5);
 *   uwb.SetDataRate(UwbDataRate::RATE_6M81);
 *   uwb.SetChannel(UwbHelper::CreateChannel(5));
 *   NetDeviceContainer devices = uwb.Install(nodes);
 * @endcode
 */
class UwbHelper : public PcapHelperForDevice
{
  public:
    UwbHelper();
    ~UwbHelper() override;

    /**
     * Build a spectrum channel suitable for UWB, with a propagation loss model configured for
     * the frequency of the channel in use and a constant speed propagation delay model.
     *
     * The delay model matters more here than anywhere else in ns-3: the whole module rests on
     * the propagation delay being right to a fraction of a nanosecond, which is why the module
     * runs the simulator clock at femtoseconds.
     *
     * @param channelNumber the UWB channel the devices will use, which sets the frequency the
     *                      loss model is evaluated at
     * @param lossModel the TypeId name of the propagation loss model
     * @return the channel
     */
    static Ptr<SpectrumChannel> CreateChannel(
        uint8_t channelNumber = 5,
        const std::string& lossModel = "ns3::FriisPropagationLossModel");

    /// @param channel the spectrum channel the installed devices transmit on
    void SetChannel(Ptr<SpectrumChannel> channel);
    /// @return the spectrum channel
    Ptr<SpectrumChannel> GetChannel() const;

    /**
     * Set an attribute of the radios created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetPhyAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an attribute of the medium access layers created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetMacAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an attribute of the network devices created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetDeviceAttribute(const std::string& name, const AttributeValue& value);

    /**
     * Set an attribute of the error models created afterwards.
     * @param name the attribute name
     * @param value the attribute value
     */
    void SetErrorModelAttribute(const std::string& name, const AttributeValue& value);

    /// @param channelNumber the UWB channel the installed radios are tuned to
    void SetChannelNumber(uint8_t channelNumber);
    /// @param rate the data rate the installed radios use
    void SetDataRate(UwbDataRate rate);
    /// @param prf the mean pulse repetition frequency the installed radios use
    void SetPrf(UwbPrf prf);
    /// @param symbols the number of preamble symbols the installed radios use
    void SetPreambleSymbols(uint32_t symbols);
    /// @param code the preamble code the installed radios use
    void SetPreambleCode(uint8_t code);
    /// @param panId the personal area network the installed devices belong to
    void SetPanId(uint16_t panId);

    /// @return the configuration the installed radios are given
    const UwbPhyConfig& GetPhyConfig() const;

    /**
     * Set the random variable the frequency offset of each crystal is drawn from, in parts per
     * million.
     *
     * @param stream the random variable
     */
    void SetClockOffsetModel(Ptr<RandomVariableStream> stream);

    /**
     * Install a UWB device on every node of a container.
     *
     * @param nodes the nodes
     * @return the devices that were installed
     */
    NetDeviceContainer Install(NodeContainer nodes);

    /**
     * Install a UWB device on one node.
     *
     * @param node the node
     * @return the device that was installed
     */
    Ptr<UwbNetDevice> Install(Ptr<Node> node);

    /**
     * Build a time difference of arrival engine and register a set of installed devices with it
     * as anchors, taking each anchor position from the mobility model of its node.
     *
     * @param anchors the devices that act as anchors
     * @return the engine
     */
    static Ptr<UwbTdoaEngine> CreateTdoaEngine(NetDeviceContainer anchors);

    /**
     * Assign the streams of the random variables of a set of installed devices.
     *
     * @param devices the devices
     * @param stream the first stream index to use
     * @return the number of streams used
     */
    int64_t AssignStreams(NetDeviceContainer devices, int64_t stream);

  private:
    void EnablePcapInternal(std::string prefix,
                            Ptr<NetDevice> nd,
                            bool promiscuous,
                            bool explicitFilename) override;

    Ptr<SpectrumChannel> m_channel;      //!< the channel the devices share
    ObjectFactory m_phyFactory;          //!< builds the radios
    ObjectFactory m_macFactory;          //!< builds the medium access layers
    ObjectFactory m_deviceFactory;       //!< builds the network devices
    ObjectFactory m_errorModelFactory;   //!< builds the error models
    UwbPhyConfig m_config;               //!< the configuration the radios are given
    uint8_t m_preambleCode;              //!< the preamble code the radios use
    uint16_t m_panId;                    //!< the personal area network
    uint16_t m_nextAddress;              //!< the next short address to hand out
    Ptr<RandomVariableStream> m_clockOffset; //!< draws the frequency offset of each crystal
};

} // namespace uwb
} // namespace ns3

#endif /* UWB_HELPER_H */
