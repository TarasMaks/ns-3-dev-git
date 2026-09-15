/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Reliability through multi-link diversity: the multi-link baseline on which the 802.11bn
 * (Wi-Fi 8) reliability enhancements build (Galati-Giordano et al. 2024, Reshef and Cordeiro
 * 2022).
 *
 * A hidden interferer close to the station periodically jams the first link (its energy is below
 * the CCA threshold at the AP, so the AP keeps transmitting on that link). Downlink Poisson
 * traffic is delivered to a single-link device on the jammed link, to a single-link device on the
 * clean link, and to a multi-link device using both links, which can retransmit failed MPDUs on
 * the other link. Loss (with a MAC queue delay budget), the fraction of packets not delivered
 * within a deadline, latency and MAC retransmissions are compared. Note that ns-3 does not
 * increment the retry count of MPDUs under a Block Ack agreement by default, so frames are
 * retried until the queue delay budget expires.
 *
 * Example: ./ns3 run "wifi-bench-mlo-reliability --dutyCycle=0.5 --interfererPower=0"
 */

#include "ns3/command-line.h"
#include "ns3/double.h"
#include "ns3/mobility-helper.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/simulator.h"
#include "ns3/waveform-generator-helper.h"
#include "ns3/waveform-generator.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-spectrum-value-helper.h"
#include "ns3/wifi-tx-stats-helper.h"
#include "ns3/wifi-utils.h"

#include <sstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * Parse a link list such as "5:80,6:80".
 * @param s the string
 * @return the links
 */
std::vector<LinkConfig>
ParseLinks(const std::string& s)
{
    std::vector<LinkConfig> links;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ','))
    {
        const auto colon = item.find(':');
        NS_ABORT_MSG_IF(colon == std::string::npos, "Invalid link spec " << item);
        LinkConfig link;
        link.band = BandFromGhz(std::stod(item.substr(0, colon)));
        link.width = MHz_u{std::stod(item.substr(colon + 1))};
        links.push_back(link);
    }
    return links;
}

/// Outcome of one run
struct RunResult
{
    FlowStats flow;                 ///< flow statistics
    double deadlineMissRatio{0};    ///< packets not delivered within the deadline
    uint64_t macSuccesses{0};       ///< MPDUs acknowledged
    uint64_t macFailures{0};        ///< MPDUs dropped after retries
    uint64_t macRetransmissions{0}; ///< MPDU retransmissions
};

/**
 * Run one configuration.
 * @param mode "sld-jammed-link", "sld-clean-link" or "mlo"
 * @param links the two links (the first one is jammed)
 * @param mcs the MCS
 * @param loadMbps the DL load
 * @param packetSize the packet size
 * @param interfererPowerDbm the interferer power
 * @param dutyCycle the interferer duty cycle
 * @param period the interferer period
 * @param queueMaxDelay the MAC queue delay limit (latency budget)
 * @param deadlineMs the delivery deadline used for the deadline miss ratio
 * @param simTime the measurement duration
 * @return the run result
 */
RunResult
Run(const std::string& mode,
    const std::vector<LinkConfig>& links,
    uint8_t mcs,
    double loadMbps,
    uint32_t packetSize,
    double interfererPowerDbm,
    double dutyCycle,
    Time period,
    Time queueMaxDelay,
    double deadlineMs,
    Time simTime)
{
    std::vector<LinkConfig> devLinks;
    if (mode == "sld-jammed-link")
    {
        devLinks = {links[0]};
    }
    else if (mode == "sld-clean-link")
    {
        devLinks = {links[1]};
    }
    else
    {
        devLinks = links;
    }
    std::map<Band, Ptr<SpectrumChannel>> channels;
    for (const auto& link : links)
    {
        if (!channels.count(link.band))
        {
            channels[link.band] = CreateSpectrumChannel(link.band);
        }
    }
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(1);
    BssBuilder builder;
    RateConfig rate;
    rate.mcs = mcs;
    MacConfig mac;
    // latency budget of a reliability-sensitive application: frames older than this are dropped
    mac.macQueueMaxDelay = queueMaxDelay;
    builder.SetStandard(WIFI_STANDARD_80211be).SetLinks(devLinks).SetRate(rate).SetMac(mac);
    for (const auto& [band, ch] : channels)
    {
        builder.SetChannel(band, ch);
    }
    Bss bss = builder.Build(apNode, staNodes);
    InstallInternet(bss, 1);
    PopulateArpCaches();

    NodeContainer interfererNode;
    interfererNode.Create(1);
    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(10, 0, 0));
    positions->Add(Vector(12, 0, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);
    mobility.Install(interfererNode);

    const Time start = MilliSeconds(100);
    const Time mStart = start + MilliSeconds(300);
    // jammer on the first link: full-band waveform centered on the default channel of the band
    const double centerMhz =
        (links[0].band == Band::GHZ_5) ? 5210 : (links[0].band == Band::GHZ_6 ? 5985 : 2437);
    auto psd = WifiSpectrumValueHelper::CreateHeOfdmTxPowerSpectralDensity(
        MHz_u{centerMhz},
        links[0].width,
        DbmToW(dBm_u{interfererPowerDbm}),
        MHz_u{20});
    WaveformGeneratorHelper wgHelper;
    wgHelper.SetChannel(channels[links[0].band]);
    wgHelper.SetTxPowerSpectralDensity(psd);
    wgHelper.SetPhyAttribute("Period", TimeValue(period));
    wgHelper.SetPhyAttribute("DutyCycle", DoubleValue(dutyCycle));
    auto wgDevices = wgHelper.Install(interfererNode);
    Simulator::Schedule(start,
                        &WaveformGenerator::Start,
                        wgDevices.Get(0)
                            ->GetObject<NonCommunicatingNetDevice>()
                            ->GetPhy()
                            ->GetObject<WaveformGenerator>());

    WifiTxStatsHelper txStats;
    txStats.Enable(bss.apDevice);
    txStats.Start(mStart);
    txStats.Stop(mStart + simTime);
    FlowSet flows;
    flows.SetMeasurementWindow(mStart, mStart + simTime);
    flows.AddUdpFlow("dl",
                     apNode.Get(0),
                     staNodes.Get(0),
                     bss.staIfs.GetAddress(0),
                     loadMbps,
                     packetSize,
                     start,
                     mStart + simTime + MilliSeconds(1),
                     0,
                     TrafficPattern::POISSON);
    Simulator::Stop(mStart + simTime + MilliSeconds(1));
    Simulator::Run();
    RunResult res;
    res.flow = flows.GetStats(0);
    res.deadlineMissRatio = flows.GetDeadlineMissRatio(deadlineMs);
    res.macSuccesses = txStats.GetSuccesses();
    res.macFailures = txStats.GetFailures();
    res.macRetransmissions = txStats.GetRetransmissions();
    Simulator::Destroy();
    return res;
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string linksStr{"5:80,6:80"};
    uint32_t mcs{7};
    double loadMbps{10};
    uint32_t packetSize{500};
    double interfererPowerDbm{0};
    double dutyCycle{0.5};
    Time period{MilliSeconds(100)};
    Time queueMaxDelay{MilliSeconds(100)};
    double deadlineMs{10};
    Time simTime{Seconds(2)};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("links", "Two band:width links (the first one is jammed)", linksStr);
    cmd.AddValue("mcs", "EHT MCS index", mcs);
    cmd.AddValue("loadMbps", "DL Poisson load (Mb/s)", loadMbps);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.AddValue("interfererPower", "Interferer transmit power (dBm)", interfererPowerDbm);
    cmd.AddValue("dutyCycle", "Interferer duty cycle", dutyCycle);
    cmd.AddValue("interfererPeriod", "Interferer on/off period", period);
    cmd.AddValue("queueMaxDelay", "MAC queue delay limit (latency budget)", queueMaxDelay);
    cmd.AddValue("deadline", "Delivery deadline (ms) for the deadline miss ratio", deadlineMs);
    cmd.AddValue("simulationTime", "Measurement duration of each simulation", simTime);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const auto links = ParseLinks(linksStr);
    NS_ABORT_MSG_IF(links.size() != 2, "Exactly two links are expected");
    if (common.quick)
    {
        simTime = std::min(simTime, MilliSeconds(500));
    }

    PrintBanner("Multi-link reliability under hidden interference (802.11bn baseline)",
                "DL Poisson traffic with a hidden jammer on link 0: single-link on the jammed link "
                "vs single-link on the clean link vs MLO (retransmission on the other link).",
                {"ieee80211bn-par", "galati2024", "reshef2022", "ieee80211be"});

    ResultTable table("wifi8-mlo-reliability", common.outputDir);
    RunResult jammed;
    for (const std::string mode : {"sld-jammed-link", "sld-clean-link", "mlo"})
    {
        const auto r = Run(mode,
                           links,
                           mcs,
                           loadMbps,
                           packetSize,
                           interfererPowerDbm,
                           dutyCycle,
                           period,
                           queueMaxDelay,
                           deadlineMs,
                           simTime);
        if (mode == "sld-jammed-link")
        {
            jammed = r;
        }
        ResultRow row;
        row.Set("mode", mode);
        row.Set("mcs", mcs);
        row.Set("interferer_dbm", interfererPowerDbm, 1);
        row.Set("duty_cycle", dutyCycle, 2);
        row.Set("throughput_mbps", r.flow.throughputMbps, 3);
        row.Set("loss_ratio", r.flow.lossRatio, 4);
        row.Set("deadline_ms", deadlineMs, 1);
        row.Set("deadline_miss_ratio", r.deadlineMissRatio, 4);
        row.Set("latency_p50_ms", r.flow.p50LatencyMs, 3);
        row.Set("latency_p95_ms", r.flow.p95LatencyMs, 3);
        row.Set("latency_p99_ms", r.flow.p99LatencyMs, 3);
        row.Set("mac_successes", r.macSuccesses);
        row.Set("mac_failures", r.macFailures);
        row.Set("mac_retransmissions", r.macRetransmissions);
        row.Set("miss_reduction_vs_jammed_sld",
                jammed.deadlineMissRatio > 0 ? 1 - r.deadlineMissRatio / jammed.deadlineMissRatio
                                             : 0,
                3);
        row.Set("measured", r.deadlineMissRatio, 4);
        Compare(row,
                r.deadlineMissRatio,
                jammed.deadlineMissRatio,
                0.0,
                "galati2024",
                mode == "mlo" ? CheckKind::AT_MOST : CheckKind::INFO);
        table.AddRow(row);
    }

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
