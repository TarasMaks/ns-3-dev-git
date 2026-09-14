/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "wifi-benchmark-helper.h"

#include "ns3/abort.h"
#include "ns3/address.h"
#include "ns3/ap-wifi-mac.h"
#include "ns3/basic-energy-source-helper.h"
#include "ns3/boolean.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/channel-condition-model.h"
#include "ns3/config.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/double.h"
#include "ns3/eht-configuration.h"
#include "ns3/eht-phy.h"
#include "ns3/enum.h"
#include "ns3/he-phy.h"
#include "ns3/inet-socket-address.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/neighbor-cache-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/pointer.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/queue-size.h"
#include "ns3/random-variable-stream.h"
#include "ns3/rng-seed-manager.h"
#include "ns3/simulator.h"
#include "ns3/socket.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/system-path.h"
#include "ns3/three-gpp-propagation-loss-model.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "ns3/wifi-acknowledgment.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-mode.h"
#include "ns3/wifi-phy.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/wifi-remote-station-manager.h"
#include "ns3/wifi-static-setup-helper.h"
#include "ns3/wifi-tx-vector.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("WifiBenchmarkHelper");

namespace wifibench
{

/* ------------------------------------------------------------------------------------------ */
/* Result tables                                                                              */
/* ------------------------------------------------------------------------------------------ */

ResultRow&
ResultRow::Set(const std::string& key, const std::string& value)
{
    for (auto& cell : m_cells)
    {
        if (cell.first == key)
        {
            cell.second = value;
            return *this;
        }
    }
    m_cells.emplace_back(key, value);
    return *this;
}

ResultRow&
ResultRow::Set(const std::string& key, double value, int precision)
{
    std::ostringstream oss;
    if (std::isnan(value))
    {
        oss << "nan";
    }
    else if (std::isinf(value))
    {
        oss << (value > 0 ? "inf" : "-inf");
    }
    else
    {
        oss << std::fixed << std::setprecision(precision) << value;
    }
    return Set(key, oss.str());
}

ResultRow&
ResultRow::Set(const std::string& key, int64_t value)
{
    return Set(key, std::to_string(value));
}

ResultRow&
ResultRow::Set(const std::string& key, uint64_t value)
{
    return Set(key, std::to_string(value));
}

ResultRow&
ResultRow::Set(const std::string& key, uint32_t value)
{
    return Set(key, std::to_string(value));
}

ResultRow&
ResultRow::Set(const std::string& key, int value)
{
    return Set(key, std::to_string(value));
}

std::string
ResultRow::Get(const std::string& key) const
{
    for (const auto& cell : m_cells)
    {
        if (cell.first == key)
        {
            return cell.second;
        }
    }
    return "";
}

const std::vector<std::pair<std::string, std::string>>&
ResultRow::GetCells() const
{
    return m_cells;
}

std::string
Compare(ResultRow& row,
        double measured,
        double reference,
        double tolerance,
        const std::string& source,
        CheckKind kind)
{
    double deviation = 0;
    if (reference != 0)
    {
        deviation = (measured - reference) / std::abs(reference) * 100;
    }
    else if (measured != 0)
    {
        deviation = std::numeric_limits<double>::infinity();
    }
    std::string verdict = "INFO";
    switch (kind)
    {
    case CheckKind::WITHIN:
        verdict =
            (std::abs(measured - reference) <= tolerance * std::abs(reference)) ? "PASS" : "FAIL";
        break;
    case CheckKind::AT_LEAST:
        verdict = (measured >= reference * (1 - tolerance)) ? "PASS" : "FAIL";
        break;
    case CheckKind::AT_MOST:
        verdict = (measured <= reference * (1 + tolerance)) ? "PASS" : "FAIL";
        break;
    case CheckKind::INFO:
        break;
    }
    row.Set("reference", reference, 3);
    row.Set("deviation_pct", deviation, 1);
    row.Set("verdict", verdict);
    row.Set("ref_source", source);
    return verdict;
}

ResultTable::ResultTable(const std::string& benchmark, const std::string& outputDir)
    : m_benchmark(benchmark),
      m_outputDir(outputDir)
{
}

void
ResultTable::AddRow(const ResultRow& row)
{
    for (const auto& cell : row.GetCells())
    {
        if (std::find(m_columns.begin(), m_columns.end(), cell.first) == m_columns.end())
        {
            m_columns.push_back(cell.first);
        }
    }
    m_rows.push_back(row);
    // print the row immediately so that long runs show progress
    std::ostringstream oss;
    for (const auto& cell : row.GetCells())
    {
        oss << cell.first << "=" << cell.second << "  ";
    }
    std::cout << "[" << m_benchmark << "] " << oss.str() << std::endl;
}

void
ResultTable::Print() const
{
    if (m_rows.empty())
    {
        return;
    }
    std::vector<std::size_t> widths(m_columns.size());
    for (std::size_t c = 0; c < m_columns.size(); ++c)
    {
        widths[c] = m_columns[c].size();
        for (const auto& row : m_rows)
        {
            widths[c] = std::max(widths[c], row.Get(m_columns[c]).size());
        }
    }
    std::cout << "\n";
    for (std::size_t c = 0; c < m_columns.size(); ++c)
    {
        std::cout << std::left << std::setw(widths[c] + 2) << m_columns[c];
    }
    std::cout << "\n";
    for (const auto& row : m_rows)
    {
        for (std::size_t c = 0; c < m_columns.size(); ++c)
        {
            std::cout << std::left << std::setw(widths[c] + 2) << row.Get(m_columns[c]);
        }
        std::cout << "\n";
    }
    std::cout << std::endl;
}

namespace
{

std::string
CsvEscape(const std::string& s)
{
    if (s.find_first_of(",\"\n") == std::string::npos)
    {
        return s;
    }
    std::string out = "\"";
    for (char ch : s)
    {
        if (ch == '"')
        {
            out += "\"\"";
        }
        else
        {
            out += ch;
        }
    }
    return out + "\"";
}

std::string
JsonEscape(const std::string& s)
{
    std::string out;
    for (char ch : s)
    {
        switch (ch)
        {
        case '"':
            out += "\\\"";
            break;
        case '\\':
            out += "\\\\";
            break;
        case '\n':
            out += "\\n";
            break;
        default:
            out += ch;
        }
    }
    return out;
}

bool
IsNumber(const std::string& s)
{
    if (s.empty())
    {
        return false;
    }
    char* end = nullptr;
    std::strtod(s.c_str(), &end);
    return end != nullptr && *end == '\0' && s != "nan" && s != "inf" && s != "-inf";
}

} // namespace

void
ResultTable::Write() const
{
    const auto base = m_outputDir + "/" + m_benchmark;
    std::ofstream csv(base + ".csv");
    for (std::size_t c = 0; c < m_columns.size(); ++c)
    {
        csv << (c ? "," : "") << CsvEscape(m_columns[c]);
    }
    csv << "\n";
    for (const auto& row : m_rows)
    {
        for (std::size_t c = 0; c < m_columns.size(); ++c)
        {
            csv << (c ? "," : "") << CsvEscape(row.Get(m_columns[c]));
        }
        csv << "\n";
    }
    csv.close();

    std::ofstream json(base + ".json");
    json << "{\n  \"benchmark\": \"" << JsonEscape(m_benchmark) << "\",\n  \"rows\": [\n";
    for (std::size_t r = 0; r < m_rows.size(); ++r)
    {
        json << "    {";
        const auto& cells = m_rows[r].GetCells();
        for (std::size_t i = 0; i < cells.size(); ++i)
        {
            json << (i ? ", " : "") << "\"" << JsonEscape(cells[i].first) << "\": ";
            if (IsNumber(cells[i].second))
            {
                json << cells[i].second;
            }
            else
            {
                json << "\"" << JsonEscape(cells[i].second) << "\"";
            }
        }
        json << "}" << (r + 1 < m_rows.size() ? "," : "") << "\n";
    }
    json << "  ]\n}\n";
    json.close();
    std::cout << "Results written to " << base << ".csv and " << base << ".json" << std::endl;
}

std::size_t
ResultTable::PrintSummary() const
{
    std::size_t pass = 0;
    std::size_t fail = 0;
    std::size_t info = 0;
    for (const auto& row : m_rows)
    {
        const auto v = row.Get("verdict");
        if (v == "PASS")
        {
            ++pass;
        }
        else if (v == "FAIL")
        {
            ++fail;
        }
        else
        {
            ++info;
        }
    }
    std::cout << "[" << m_benchmark << "] SUMMARY: " << m_rows.size() << " rows, PASS=" << pass
              << " FAIL=" << fail << " INFO=" << info << std::endl;
    return fail;
}

const std::vector<ResultRow>&
ResultTable::GetRows() const
{
    return m_rows;
}

const std::string&
ResultTable::GetName() const
{
    return m_benchmark;
}

void
PrintBanner(const std::string& title,
            const std::string& description,
            const std::vector<std::string>& citationKeys)
{
    std::cout << "\n==================================================================\n"
              << title << "\n"
              << "==================================================================\n"
              << description << "\n";
    if (!citationKeys.empty())
    {
        std::cout << "References:\n";
        for (const auto& key : citationKeys)
        {
            for (const auto& c : GetCitations())
            {
                if (c.key == key)
                {
                    std::cout << "  [" << key << "] " << c.text << "\n";
                }
            }
        }
    }
    std::cout << std::endl;
}

void
AddCommonArgs(CommandLine& cmd, CommonArgs& args)
{
    cmd.AddValue("outputDir", "Directory where CSV/JSON results are written", args.outputDir);
    cmd.AddValue("quick",
                 "Reduce the number and duration of the runs (used by regression tests)",
                 args.quick);
    cmd.AddValue("rngRun", "RNG run number", args.rngRun);
    cmd.AddValue("verbose", "Print per-run details", args.verbose);
}

void
ApplyCommonArgs(const CommonArgs& args)
{
    RngSeedManager::SetRun(args.rngRun);
    SystemPath::MakeDirectories(args.outputDir);
}

/* ------------------------------------------------------------------------------------------ */
/* Bands, links, channels                                                                     */
/* ------------------------------------------------------------------------------------------ */

WifiPhyBand
ToPhyBand(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return WIFI_PHY_BAND_2_4GHZ;
    case Band::GHZ_5:
        return WIFI_PHY_BAND_5GHZ;
    case Band::GHZ_6:
        return WIFI_PHY_BAND_6GHZ;
    }
    return WIFI_PHY_BAND_5GHZ;
}

FrequencyRange
ToFrequencyRange(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return WIFI_SPECTRUM_2_4_GHZ;
    case Band::GHZ_5:
        return WIFI_SPECTRUM_5_GHZ;
    case Band::GHZ_6:
        return WIFI_SPECTRUM_6_GHZ;
    }
    return WIFI_SPECTRUM_5_GHZ;
}

std::string
BandName(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return "2.4GHz";
    case Band::GHZ_5:
        return "5GHz";
    case Band::GHZ_6:
        return "6GHz";
    }
    return "?";
}

std::string
BandToken(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return "BAND_2_4GHZ";
    case Band::GHZ_5:
        return "BAND_5GHZ";
    case Band::GHZ_6:
        return "BAND_6GHZ";
    }
    return "BAND_5GHZ";
}

Band
BandFromGhz(double ghz)
{
    if (std::abs(ghz - 2.4) < 0.01)
    {
        return Band::GHZ_2_4;
    }
    if (std::abs(ghz - 5) < 0.01)
    {
        return Band::GHZ_5;
    }
    if (std::abs(ghz - 6) < 0.01)
    {
        return Band::GHZ_6;
    }
    NS_ABORT_MSG("Unsupported band " << ghz << " GHz (use 2.4, 5 or 6)");
    return Band::GHZ_5;
}

std::string
ChannelSettings(const LinkConfig& link)
{
    std::ostringstream oss;
    oss << "{" << link.channelNumber << ", " << static_cast<int>(link.width) << ", "
        << BandToken(link.band) << ", " << +link.primary20 << "}";
    return oss.str();
}

namespace
{

double
BandCenterFrequencyHz(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return 2.437e9;
    case Band::GHZ_5:
        return 5.5e9;
    case Band::GHZ_6:
        return 6.5e9;
    }
    return 5.5e9;
}

double
DefaultReferenceLossDb(Band band)
{
    switch (band)
    {
    case Band::GHZ_2_4:
        return 40.05;
    case Band::GHZ_5:
        return 46.6777;
    case Band::GHZ_6:
        return 48.0;
    }
    return 46.6777;
}

} // namespace

Ptr<SpectrumChannel>
CreateSpectrumChannel(Band band, const ChannelConfig& cfg)
{
    auto channel = CreateObject<MultiModelSpectrumChannel>();
    if (cfg.lossModel == "logdistance")
    {
        auto loss = CreateObject<LogDistancePropagationLossModel>();
        loss->SetAttribute("Exponent", DoubleValue(cfg.logDistanceExponent));
        loss->SetAttribute("ReferenceLoss",
                           DoubleValue(cfg.referenceLossDb.value_or(DefaultReferenceLossDb(band))));
        channel->AddPropagationLossModel(loss);
    }
    else if (cfg.lossModel == "friis")
    {
        auto loss = CreateObject<FriisPropagationLossModel>();
        loss->SetAttribute("Frequency", DoubleValue(BandCenterFrequencyHz(band)));
        channel->AddPropagationLossModel(loss);
    }
    else if (cfg.lossModel == "3gpp-inh")
    {
        auto loss = CreateObject<ThreeGppIndoorOfficePropagationLossModel>();
        loss->SetAttribute("Frequency", DoubleValue(BandCenterFrequencyHz(band)));
        loss->SetAttribute("ShadowingEnabled", BooleanValue(cfg.shadowing));
        auto condition = CreateObject<AlwaysLosChannelConditionModel>();
        loss->SetAttribute("ChannelConditionModel", PointerValue(condition));
        channel->AddPropagationLossModel(loss);
    }
    else
    {
        NS_ABORT_MSG("Unknown loss model " << cfg.lossModel);
    }
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    return channel;
}

/* ------------------------------------------------------------------------------------------ */
/* Rate control                                                                               */
/* ------------------------------------------------------------------------------------------ */

std::string
DataModeName(WifiStandard standard, uint8_t mcs)
{
    return (standard == WIFI_STANDARD_80211be ? "EhtMcs" : "HeMcs") + std::to_string(mcs);
}

std::string
ControlModeName(WifiStandard standard, Band band, uint8_t mcs)
{
    const uint64_t refMbps =
        (standard == WIFI_STANDARD_80211be ? EhtPhy::GetNonHtReferenceRate(mcs)
                                           : HePhy::GetNonHtReferenceRate(mcs)) /
        1000000;
    switch (band)
    {
    case Band::GHZ_2_4:
        return "ErpOfdmRate" + std::to_string(refMbps) + "Mbps";
    case Band::GHZ_5:
        return "OfdmRate" + std::to_string(refMbps) + "Mbps";
    case Band::GHZ_6: {
        // HE MCS with the same modulation and coding as the non-HT reference rate
        static const std::map<uint64_t, uint8_t>
            map{{6, 0}, {9, 1}, {12, 1}, {18, 2}, {24, 3}, {36, 4}, {48, 5}, {54, 7}};
        auto it = map.find(refMbps);
        return "HeMcs" + std::to_string(it != map.end() ? it->second : 0);
    }
    }
    return "OfdmRate6Mbps";
}

void
ConfigureRateManager(WifiHelper& wifi,
                     uint8_t linkId,
                     WifiStandard standard,
                     Band band,
                     const RateConfig& rate)
{
    if (rate.mcs < 0)
    {
        wifi.SetRemoteStationManager(linkId, rate.manager);
        return;
    }
    std::string control = ControlModeName(standard, band, rate.mcs);
    if (band == Band::GHZ_6 && rate.controlMode6GHz)
    {
        control = *rate.controlMode6GHz;
    }
    wifi.SetRemoteStationManager(linkId,
                                 "ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(DataModeName(standard, rate.mcs)),
                                 "ControlMode",
                                 StringValue(control));
}

/* ------------------------------------------------------------------------------------------ */
/* BSS                                                                                        */
/* ------------------------------------------------------------------------------------------ */

Ptr<WifiNetDevice>
Bss::ApDev() const
{
    return DynamicCast<WifiNetDevice>(apDevice.Get(0));
}

Ptr<ApWifiMac>
Bss::ApMac() const
{
    return DynamicCast<ApWifiMac>(ApDev()->GetMac());
}

Ptr<WifiNetDevice>
Bss::StaDev(uint32_t i) const
{
    return DynamicCast<WifiNetDevice>(staDevices.Get(i));
}

NetDeviceContainer
Bss::AllDevices() const
{
    NetDeviceContainer all(apDevice);
    all.Add(staDevices);
    return all;
}

NodeContainer
Bss::AllNodes() const
{
    NodeContainer all(apNode);
    all.Add(staNodes);
    return all;
}

BssBuilder::BssBuilder()
{
}

BssBuilder&
BssBuilder::SetStandard(WifiStandard standard)
{
    m_standard = standard;
    return *this;
}

BssBuilder&
BssBuilder::AddLink(const LinkConfig& link)
{
    m_links.push_back(link);
    return *this;
}

BssBuilder&
BssBuilder::SetLinks(const std::vector<LinkConfig>& links)
{
    m_links = links;
    return *this;
}

BssBuilder&
BssBuilder::SetPhy(const PhyConfig& phy)
{
    m_phy = phy;
    return *this;
}

BssBuilder&
BssBuilder::SetChannel(Band band, Ptr<SpectrumChannel> channel)
{
    m_channels[band] = channel;
    return *this;
}

BssBuilder&
BssBuilder::SetRate(const RateConfig& rate)
{
    m_rate = rate;
    return *this;
}

BssBuilder&
BssBuilder::SetMac(const MacConfig& mac)
{
    m_mac = mac;
    return *this;
}

BssBuilder&
BssBuilder::SetSsid(const std::string& ssid)
{
    m_ssid = ssid;
    return *this;
}

BssBuilder&
BssBuilder::SetBssColor(uint8_t color)
{
    m_bssColor = color;
    return *this;
}

BssBuilder&
BssBuilder::SetObssPdLevel(dBm_u level)
{
    m_obssPdLevel = level;
    return *this;
}

BssBuilder&
BssBuilder::SetMuScheduler(const MuSchedulerConfig& mu)
{
    m_mu = mu;
    return *this;
}

BssBuilder&
BssBuilder::SetEmlsr(const EmlsrConfig& emlsr)
{
    m_emlsr = emlsr;
    return *this;
}

BssBuilder&
BssBuilder::SetTidToLinkMapping(const std::string& dl, const std::string& ul)
{
    m_t2lm = std::make_pair(dl, ul);
    return *this;
}

BssBuilder&
BssBuilder::SetPhyCustomizer(PhyCustomizer f)
{
    m_phyCustomizer = std::move(f);
    return *this;
}

BssBuilder&
BssBuilder::SetStaMacCustomizer(MacCustomizer f)
{
    m_staMacCustomizer = std::move(f);
    return *this;
}

BssBuilder&
BssBuilder::SetApMacCustomizer(MacCustomizer f)
{
    m_apMacCustomizer = std::move(f);
    return *this;
}

BssBuilder&
BssBuilder::SetWifiCustomizer(WifiCustomizer f)
{
    m_wifiCustomizer = std::move(f);
    return *this;
}

WifiStandard
BssBuilder::GetStandard() const
{
    return m_standard;
}

const std::vector<LinkConfig>&
BssBuilder::GetLinks() const
{
    return m_links;
}

const MacConfig&
BssBuilder::GetMac() const
{
    return m_mac;
}

namespace
{

void
SetMacAttributes(WifiMacHelper& mac,
                 const std::string& type,
                 const Ssid& ssid,
                 bool ap,
                 bool beacons,
                 uint16_t mpduBufferSize,
                 uint32_t maxAmpdu,
                 uint32_t maxAmsdu)
{
    if (ap)
    {
        mac.SetType(type,
                    "Ssid",
                    SsidValue(ssid),
                    "EnableBeaconJitter",
                    BooleanValue(false),
                    "BeaconGeneration",
                    BooleanValue(beacons),
                    "MpduBufferSize",
                    UintegerValue(mpduBufferSize),
                    "BE_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "BK_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "VI_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "VO_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "BE_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "BK_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "VI_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "VO_MaxAmsduSize",
                    UintegerValue(maxAmsdu));
    }
    else
    {
        mac.SetType(type,
                    "Ssid",
                    SsidValue(ssid),
                    "ActiveProbing",
                    BooleanValue(false),
                    "MpduBufferSize",
                    UintegerValue(mpduBufferSize),
                    "BE_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "BK_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "VI_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "VO_MaxAmpduSize",
                    UintegerValue(maxAmpdu),
                    "BE_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "BK_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "VI_MaxAmsduSize",
                    UintegerValue(maxAmsdu),
                    "VO_MaxAmsduSize",
                    UintegerValue(maxAmsdu));
    }
}

} // namespace

Bss
BssBuilder::Build(NodeContainer apNode, NodeContainer staNodes)
{
    NS_ABORT_MSG_IF(m_links.empty(), "No link configured");
    NS_ABORT_MSG_IF(apNode.GetN() != 1, "Exactly one AP node is expected");
    const auto nLinks = static_cast<uint8_t>(m_links.size());
    const bool eht = (m_standard == WIFI_STANDARD_80211be);
    NS_ABORT_MSG_IF(!eht && nLinks > 1, "Multi-link operation requires 802.11be");

    Config::SetDefault("ns3::WifiMacQueue::MaxSize",
                       QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, m_mac.macQueueSize)));
    Config::SetDefault("ns3::WifiMacQueue::MaxDelay", TimeValue(m_mac.macQueueMaxDelay));
    Config::SetDefault("ns3::WifiDefaultProtectionManager::EnableMuRts",
                       BooleanValue(m_mac.rtsCts));

    if (m_mu)
    {
        auto seq = WifiAcknowledgment::DL_MU_AGGREGATE_TF;
        if (m_mu->dlAckType == "ACK-SU-FORMAT")
        {
            seq = WifiAcknowledgment::DL_MU_BAR_BA_SEQUENCE;
        }
        else if (m_mu->dlAckType == "MU-BAR")
        {
            seq = WifiAcknowledgment::DL_MU_TF_MU_BAR;
        }
        else if (m_mu->dlAckType == "AGGR-MU-BAR")
        {
            seq = WifiAcknowledgment::DL_MU_AGGREGATE_TF;
        }
        else if (m_mu->dlAckType != "NO-OFDMA")
        {
            NS_ABORT_MSG("Invalid DL ack sequence type " << m_mu->dlAckType);
        }
        Config::SetDefault("ns3::WifiDefaultAckManager::DlMuAckSequenceType", EnumValue(seq));
    }

    WifiHelper wifi;
    wifi.SetStandard(m_standard);
    wifi.ConfigHtOptions("LdpcSupported", BooleanValue(m_mac.ldpc));
    wifi.ConfigHeOptions("GuardInterval", TimeValue(NanoSeconds(m_mac.guardIntervalNs)));
    if (m_bssColor)
    {
        wifi.ConfigHeOptions("BssColor", UintegerValue(*m_bssColor));
    }
    if (m_obssPdLevel)
    {
        wifi.SetObssPdAlgorithm("ns3::ConstantObssPdAlgorithm",
                                "ObssPdLevel",
                                DoubleValue(*m_obssPdLevel));
    }
    if (eht && m_emlsr)
    {
        wifi.ConfigEhtOptions("EmlsrActivated", BooleanValue(true));
        wifi.ConfigEhtOptions("TransitionTimeout", TimeValue(m_emlsr->transitionTimeout));
        wifi.ConfigEhtOptions("MediumSyncDuration", TimeValue(m_emlsr->mediumSyncDuration));
    }
    if (eht && m_t2lm)
    {
        wifi.ConfigEhtOptions("TidToLinkMappingNegSupport",
                              EnumValue(WifiTidToLinkMappingNegSupport::ANY_LINK_SET));
        wifi.ConfigEhtOptions("TidToLinkMappingDl", StringValue(m_t2lm->first));
        wifi.ConfigEhtOptions("TidToLinkMappingUl", StringValue(m_t2lm->second));
    }
    for (uint8_t linkId = 0; linkId < nLinks; ++linkId)
    {
        ConfigureRateManager(wifi, linkId, m_standard, m_links[linkId].band, m_rate);
    }
    if (m_wifiCustomizer)
    {
        m_wifiCustomizer(wifi);
    }

    SpectrumWifiPhyHelper phy(nLinks);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    std::set<Band> bandsAdded;
    for (uint8_t linkId = 0; linkId < nLinks; ++linkId)
    {
        const auto& link = m_links[linkId];
        phy.Set(linkId, "ChannelSettings", StringValue(ChannelSettings(link)));
        if (bandsAdded.insert(link.band).second)
        {
            auto it = m_channels.find(link.band);
            NS_ABORT_MSG_IF(it == m_channels.end(),
                            "No spectrum channel set for band " << BandName(link.band));
            phy.AddChannel(it->second, ToFrequencyRange(link.band));
        }
    }
    phy.Set("TxPowerStart", DoubleValue(m_phy.txPower));
    phy.Set("TxPowerEnd", DoubleValue(m_phy.txPower));
    phy.Set("RxNoiseFigure", DoubleValue(m_phy.rxNoiseFigureDb));
    phy.Set("CcaEdThreshold", DoubleValue(m_phy.ccaEdThreshold));
    phy.Set("RxSensitivity", DoubleValue(m_phy.rxSensitivity));
    phy.Set("Antennas", UintegerValue(m_phy.nss));
    phy.Set("MaxSupportedTxSpatialStreams", UintegerValue(m_phy.nss));
    phy.Set("MaxSupportedRxSpatialStreams", UintegerValue(m_phy.nss));
    phy.Set("ChannelSwitchDelay", TimeValue(m_phy.channelSwitchDelay));
    if (m_phyCustomizer)
    {
        m_phyCustomizer(phy);
    }

    const uint16_t mpduBufferSize = m_mac.mpduBufferSize.value_or(eht ? 1024 : 256);
    const uint32_t maxAmpdu =
        m_mac.maxAmpduSize.value_or(eht ? EHT_MAX_AMPDU_BYTES : HE_MAX_AMPDU_BYTES);
    std::string txopLimits;
    if (m_mac.txopLimit)
    {
        for (uint8_t linkId = 0; linkId < nLinks; ++linkId)
        {
            txopLimits +=
                (linkId ? "," : "") + std::to_string(m_mac.txopLimit->GetMicroSeconds()) + "us";
        }
    }
    Ssid ssid(m_ssid);

    WifiMacHelper staMac;
    SetMacAttributes(staMac,
                     "ns3::StaWifiMac",
                     ssid,
                     false,
                     false,
                     mpduBufferSize,
                     maxAmpdu,
                     m_mac.maxAmsduSize);
    if (!txopLimits.empty())
    {
        staMac.SetEdca(AC_BE, "TxopLimits", StringValue(txopLimits));
    }
    if (eht && m_emlsr)
    {
        staMac.SetEmlsrManager(m_emlsr->manager,
                               "EmlsrLinkSet",
                               StringValue(m_emlsr->linkSet),
                               "MainPhyId",
                               UintegerValue(m_emlsr->mainPhyId),
                               "EmlsrPaddingDelay",
                               TimeValue(m_emlsr->paddingDelay),
                               "EmlsrTransitionDelay",
                               TimeValue(m_emlsr->transitionDelay),
                               "SwitchAuxPhy",
                               BooleanValue(m_emlsr->switchAuxPhy),
                               "AuxPhyTxCapable",
                               BooleanValue(m_emlsr->auxPhyTxCapable),
                               "AuxPhyChannelWidth",
                               UintegerValue(m_emlsr->auxPhyChannelWidth));
    }
    if (m_staMacCustomizer)
    {
        m_staMacCustomizer(staMac);
    }
    NetDeviceContainer staDevices = wifi.Install(phy, staMac, staNodes);

    WifiMacHelper apMac;
    if (m_mu && m_mu->dlAckType != "NO-OFDMA")
    {
        apMac.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                    "EnableUlOfdma",
                                    BooleanValue(m_mu->enableUlOfdma),
                                    "EnableBsrp",
                                    BooleanValue(m_mu->enableBsrp),
                                    "NStations",
                                    UintegerValue(m_mu->nStations),
                                    "AccessReqInterval",
                                    TimeValue(m_mu->accessReqInterval),
                                    "UlPsduSize",
                                    UintegerValue(m_mu->ulPsduSize),
                                    "UseCentral26TonesRus",
                                    BooleanValue(m_mu->useCentral26TonesRus),
                                    "EnableTxopSharing",
                                    BooleanValue(m_mu->enableTxopSharing));
    }
    SetMacAttributes(apMac,
                     "ns3::ApWifiMac",
                     ssid,
                     true,
                     !m_mac.staticSetup,
                     mpduBufferSize,
                     maxAmpdu,
                     m_mac.maxAmsduSize);
    if (!txopLimits.empty())
    {
        apMac.SetEdca(AC_BE, "TxopLimits", StringValue(txopLimits));
    }
    if (m_apMacCustomizer)
    {
        m_apMacCustomizer(apMac);
    }
    NetDeviceContainer apDevice = wifi.Install(phy, apMac, apNode);

    NetDeviceContainer all(apDevice);
    all.Add(staDevices);
    if (m_mac.rtsCts)
    {
        for (auto it = all.Begin(); it != all.End(); ++it)
        {
            auto dev = DynamicCast<WifiNetDevice>(*it);
            for (uint8_t linkId = 0; linkId < nLinks; ++linkId)
            {
                dev->GetMac()->GetWifiRemoteStationManager(linkId)->SetAttribute("RtsCtsThreshold",
                                                                                 UintegerValue(0));
            }
        }
    }

    static int64_t streamNumber = 100;
    streamNumber += WifiHelper::AssignStreams(apDevice, streamNumber);
    streamNumber += WifiHelper::AssignStreams(staDevices, streamNumber);

    Bss bss;
    bss.apNode = apNode;
    bss.staNodes = staNodes;
    bss.apDevice = apDevice;
    bss.staDevices = staDevices;
    bss.ssid = ssid;
    bss.nLinks = nLinks;

    if (m_mac.staticSetup)
    {
        auto apDev = bss.ApDev();
        WifiStaticSetupHelper::SetStaticAssociation(apDev, staDevices);
        if (eht && m_emlsr)
        {
            WifiStaticSetupHelper::SetStaticEmlsr(apDev, staDevices);
        }
        if (!m_mac.baTids.empty())
        {
            WifiStaticSetupHelper::SetStaticBlockAck(apDev, staDevices, m_mac.baTids);
        }
    }
    return bss;
}

void
InstallInternet(Bss& bss, uint8_t subnet)
{
    InternetStackHelper stack;
    stack.Install(bss.apNode);
    stack.Install(bss.staNodes);
    Ipv4AddressHelper address;
    const std::string base = "10." + std::to_string(subnet) + ".0.0";
    address.SetBase(Ipv4Address(base.c_str()), Ipv4Mask("255.255.0.0"));
    bss.apIf = address.Assign(bss.apDevice);
    bss.staIfs = address.Assign(bss.staDevices);
}

void
PopulateArpCaches()
{
    NeighborCacheHelper nbCache;
    nbCache.PopulateNeighborCache();
}

void
PlaceOnCircle(const Bss& bss, double radius, Vector apPosition)
{
    MobilityHelper mobility;
    auto positions = CreateObject<ListPositionAllocator>();
    positions->Add(apPosition);
    const auto n = bss.staNodes.GetN();
    for (uint32_t i = 0; i < n; ++i)
    {
        const double angle = 2 * M_PI * i / n;
        positions->Add(Vector(apPosition.x + radius * std::cos(angle),
                              apPosition.y + radius * std::sin(angle),
                              apPosition.z));
    }
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(bss.apNode);
    mobility.Install(bss.staNodes);
}

/* ------------------------------------------------------------------------------------------ */
/* Traffic                                                                                    */
/* ------------------------------------------------------------------------------------------ */

NS_OBJECT_ENSURE_REGISTERED(BenchUdpSource);

TypeId
BenchUdpSource::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::wifibench::BenchUdpSource")
            .SetParent<Application>()
            .SetGroupName("WifiBenchmarks")
            .AddConstructor<BenchUdpSource>()
            .AddAttribute("Remote",
                          "The destination address (InetSocketAddress)",
                          AddressValue(),
                          MakeAddressAccessor(&BenchUdpSource::m_remote),
                          MakeAddressChecker())
            .AddAttribute("PacketSize",
                          "The packet size in bytes, including the 20-byte SeqTsSize header",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&BenchUdpSource::m_size),
                          MakeUintegerChecker<uint32_t>(21, 65000))
            .AddAttribute("DataRate",
                          "The offered load",
                          DataRateValue(DataRate("1Mbps")),
                          MakeDataRateAccessor(&BenchUdpSource::m_rate),
                          MakeDataRateChecker())
            .AddAttribute("Pattern",
                          "The traffic pattern",
                          EnumValue(TrafficPattern::CBR),
                          MakeEnumAccessor<TrafficPattern>(&BenchUdpSource::m_pattern),
                          MakeEnumChecker(TrafficPattern::CBR,
                                          "Cbr",
                                          TrafficPattern::POISSON,
                                          "Poisson",
                                          TrafficPattern::BURST,
                                          "Burst"))
            .AddAttribute("Tos",
                          "The IP TOS of the packets",
                          UintegerValue(0),
                          MakeUintegerAccessor(&BenchUdpSource::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("BurstPackets",
                          "Packets per burst (Burst pattern)",
                          UintegerValue(10),
                          MakeUintegerAccessor(&BenchUdpSource::m_burstPackets),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("BurstInterval",
                          "Interval between bursts (Burst pattern)",
                          TimeValue(MilliSeconds(10)),
                          MakeTimeAccessor(&BenchUdpSource::m_burstInterval),
                          MakeTimeChecker());
    return tid;
}

BenchUdpSource::BenchUdpSource()
{
    m_expVar = CreateObject<ExponentialRandomVariable>();
}

BenchUdpSource::~BenchUdpSource()
{
}

uint64_t
BenchUdpSource::GetSentPackets() const
{
    return m_sent;
}

void
BenchUdpSource::StartApplication()
{
    m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
    m_socket->SetIpTos(m_tos);
    m_socket->Bind();
    m_socket->Connect(m_remote);
    const double intervalS = m_size * 8.0 / m_rate.GetBitRate();
    m_expVar->SetAttribute("Mean", DoubleValue(intervalS));
    // random initial offset to avoid synchronization of the sources
    auto uniform = CreateObject<UniformRandomVariable>();
    const double offset = uniform->GetValue(0, intervalS);
    m_sendEvent = Simulator::Schedule(Seconds(offset), &BenchUdpSource::SendPacket, this);
}

void
BenchUdpSource::StopApplication()
{
    m_sendEvent.Cancel();
    if (m_socket)
    {
        m_socket->Close();
        m_socket = nullptr;
    }
}

void
BenchUdpSource::SendPacket()
{
    const uint32_t count = (m_pattern == TrafficPattern::BURST) ? m_burstPackets : 1;
    for (uint32_t i = 0; i < count; ++i)
    {
        SeqTsSizeHeader header;
        header.SetSeq(m_seq++);
        header.SetSize(m_size);
        auto packet = Create<Packet>(m_size - header.GetSerializedSize());
        packet->AddHeader(header);
        m_socket->Send(packet);
        ++m_sent;
    }
    ScheduleNext();
}

void
BenchUdpSource::ScheduleNext()
{
    const double intervalS = m_size * 8.0 / m_rate.GetBitRate();
    Time next;
    switch (m_pattern)
    {
    case TrafficPattern::CBR:
        next = Seconds(intervalS);
        break;
    case TrafficPattern::POISSON:
        next = Seconds(m_expVar->GetValue());
        break;
    case TrafficPattern::BURST:
        next = m_burstInterval;
        break;
    }
    m_sendEvent = Simulator::Schedule(next, &BenchUdpSource::SendPacket, this);
}

uint16_t FlowSet::s_nextPort = 5000;

void
FlowSet::SetMeasurementWindow(Time start, Time stop)
{
    m_start = start;
    m_stop = stop;
}

namespace
{

void
FlowSetRxStatic(FlowSet* self,
                uint32_t id,
                Ptr<const Packet> p,
                const Address& from,
                const Address& to,
                const SeqTsSizeHeader& header)
{
    self->Rx(id, p, from, to, header);
}

} // namespace

void
FlowSet::InstallSink(Flow& flow, uint32_t id, Time start, Time stop)
{
    PacketSinkHelper sinkHelper(flow.tcp ? "ns3::TcpSocketFactory" : "ns3::UdpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), flow.port));
    sinkHelper.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
    auto apps = sinkHelper.Install(flow.dst);
    apps.Start(Seconds(0));
    flow.sink = DynamicCast<PacketSink>(apps.Get(0));
    flow.sink->TraceConnectWithoutContext("RxWithSeqTsSize",
                                          MakeBoundCallback(&FlowSetRxStatic, this, id));
}

uint32_t
FlowSet::AddUdpFlow(const std::string& label,
                    Ptr<Node> src,
                    Ptr<Node> dst,
                    Ipv4Address dstAddr,
                    double rateMbps,
                    uint32_t packetSize,
                    Time start,
                    Time stop,
                    uint8_t tos,
                    TrafficPattern pattern)
{
    Flow flow;
    flow.label = label;
    flow.tcp = false;
    flow.src = src;
    flow.dst = dst;
    flow.port = s_nextPort++;
    const uint32_t id = m_flows.size();

    auto source = CreateObject<BenchUdpSource>();
    source->SetAttribute("Remote", AddressValue(InetSocketAddress(dstAddr, flow.port)));
    source->SetAttribute("PacketSize", UintegerValue(packetSize));
    source->SetAttribute("DataRate",
                         DataRateValue(DataRate(static_cast<uint64_t>(rateMbps * 1e6))));
    source->SetAttribute("Pattern", EnumValue(pattern));
    source->SetAttribute("Tos", UintegerValue(tos));
    src->AddApplication(source);
    source->SetStartTime(start);
    source->SetStopTime(stop);
    flow.udpSource = source;

    InstallSink(flow, id, start, stop);
    m_flows.push_back(flow);

    Simulator::Schedule(m_start, [this, id]() {
        m_flows[id].txAtStart = m_flows[id].udpSource->GetSentPackets();
    });
    if (m_stop != Time::Max())
    {
        Simulator::Schedule(m_stop, [this, id]() {
            m_flows[id].txAtStop = m_flows[id].udpSource->GetSentPackets();
            m_flows[id].stopSampled = true;
        });
    }
    return id;
}

uint32_t
FlowSet::AddTcpFlow(const std::string& label,
                    Ptr<Node> src,
                    Ptr<Node> dst,
                    Ipv4Address dstAddr,
                    double rateMbps,
                    uint32_t packetSize,
                    Time start,
                    Time stop,
                    uint8_t tos)
{
    Flow flow;
    flow.label = label;
    flow.tcp = true;
    flow.src = src;
    flow.dst = dst;
    flow.port = s_nextPort++;
    const uint32_t id = m_flows.size();

    ApplicationContainer apps;
    if (rateMbps > 0)
    {
        OnOffHelper onoff("ns3::TcpSocketFactory", InetSocketAddress(dstAddr, flow.port));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
        onoff.SetAttribute("DataRate",
                           DataRateValue(DataRate(static_cast<uint64_t>(rateMbps * 1e6))));
        onoff.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
        onoff.SetAttribute("Tos", UintegerValue(tos));
        apps = onoff.Install(src);
    }
    else
    {
        BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(dstAddr, flow.port));
        bulk.SetAttribute("SendSize", UintegerValue(packetSize));
        bulk.SetAttribute("EnableSeqTsSizeHeader", BooleanValue(true));
        bulk.SetAttribute("Tos", UintegerValue(tos));
        apps = bulk.Install(src);
    }
    apps.Start(start);
    apps.Stop(stop);

    InstallSink(flow, id, start, stop);
    m_flows.push_back(flow);
    return id;
}

void
FlowSet::Rx(uint32_t id,
            Ptr<const Packet> p,
            const Address& from,
            const Address& to,
            const SeqTsSizeHeader& header)
{
    const Time now = Simulator::Now();
    const Time sent = header.GetTs();
    if (now < m_start || now > m_stop)
    {
        return;
    }
    auto& flow = m_flows[id];
    if (flow.rxPackets > 0)
    {
        flow.maxRxGapMs = std::max(flow.maxRxGapMs, (now - flow.lastRx).GetNanoSeconds() / 1e6);
    }
    flow.lastRx = now;
    flow.rxPackets++;
    flow.rxBytes += header.GetSize();
    flow.latenciesMs.push_back((now - sent).GetNanoSeconds() / 1e6);
}

FlowStats
FlowSet::ComputeStats(const Flow& flow) const
{
    FlowStats s;
    s.label = flow.label;
    const Time end = std::min(m_stop, Simulator::Now());
    const double durationS = std::max((end - m_start).GetSeconds(), 1e-9);
    s.rxPackets = flow.rxPackets;
    s.rxBytes = flow.rxBytes;
    s.maxRxGapMs = flow.maxRxGapMs;
    s.throughputMbps = flow.rxBytes * 8.0 / durationS / 1e6;
    if (!flow.tcp && flow.udpSource)
    {
        const uint64_t txStop = flow.stopSampled ? flow.txAtStop : flow.udpSource->GetSentPackets();
        s.txPackets = txStop - flow.txAtStart;
        s.lossRatio =
            s.txPackets ? 1.0 - std::min<double>(1.0, double(s.rxPackets) / s.txPackets) : 0.0;
    }
    if (!flow.latenciesMs.empty())
    {
        s.meanLatencyMs = std::accumulate(flow.latenciesMs.begin(), flow.latenciesMs.end(), 0.0) /
                          flow.latenciesMs.size();
        s.p50LatencyMs = Percentile(flow.latenciesMs, 50);
        s.p95LatencyMs = Percentile(flow.latenciesMs, 95);
        s.p99LatencyMs = Percentile(flow.latenciesMs, 99);
        s.p999LatencyMs = Percentile(flow.latenciesMs, 99.9);
        s.maxLatencyMs = *std::max_element(flow.latenciesMs.begin(), flow.latenciesMs.end());
    }
    return s;
}

FlowStats
FlowSet::GetStats(uint32_t id) const
{
    return ComputeStats(m_flows.at(id));
}

FlowStats
FlowSet::GetAggregateStats() const
{
    FlowStats agg;
    agg.label = "all";
    std::vector<double> lat;
    double meanSum = 0;
    for (const auto& flow : m_flows)
    {
        const auto s = ComputeStats(flow);
        agg.throughputMbps += s.throughputMbps;
        agg.txPackets += s.txPackets;
        agg.rxPackets += s.rxPackets;
        agg.rxBytes += s.rxBytes;
        agg.maxRxGapMs = std::max(agg.maxRxGapMs, s.maxRxGapMs);
        meanSum += s.meanLatencyMs * flow.latenciesMs.size();
        lat.insert(lat.end(), flow.latenciesMs.begin(), flow.latenciesMs.end());
    }
    agg.lossRatio =
        agg.txPackets ? 1.0 - std::min<double>(1.0, double(agg.rxPackets) / agg.txPackets) : 0.0;
    if (!lat.empty())
    {
        agg.meanLatencyMs = meanSum / lat.size();
        agg.p50LatencyMs = Percentile(lat, 50);
        agg.p95LatencyMs = Percentile(lat, 95);
        agg.p99LatencyMs = Percentile(lat, 99);
        agg.p999LatencyMs = Percentile(lat, 99.9);
        agg.maxLatencyMs = *std::max_element(lat.begin(), lat.end());
    }
    return agg;
}

std::vector<FlowStats>
FlowSet::GetAllStats() const
{
    std::vector<FlowStats> all;
    for (const auto& flow : m_flows)
    {
        all.push_back(ComputeStats(flow));
    }
    return all;
}

uint32_t
FlowSet::GetNFlows() const
{
    return m_flows.size();
}

double
FlowSet::GetThroughputPercentile(double percentile) const
{
    std::vector<double> tputs;
    for (const auto& flow : m_flows)
    {
        tputs.push_back(ComputeStats(flow).throughputMbps);
    }
    return Percentile(tputs, percentile);
}

double
FlowSet::GetLatencyPercentile(double percentile) const
{
    std::vector<double> lat;
    for (const auto& flow : m_flows)
    {
        lat.insert(lat.end(), flow.latenciesMs.begin(), flow.latenciesMs.end());
    }
    return Percentile(lat, percentile);
}

double
FlowSet::GetDeadlineMissRatio(double deadlineMs) const
{
    uint64_t tx = 0;
    uint64_t onTime = 0;
    for (const auto& flow : m_flows)
    {
        const auto s = ComputeStats(flow);
        tx += s.txPackets;
        onTime += std::count_if(flow.latenciesMs.begin(), flow.latenciesMs.end(), [&](double l) {
            return l <= deadlineMs;
        });
    }
    if (tx == 0)
    {
        return 0;
    }
    return 1.0 - std::min<double>(1.0, static_cast<double>(onTime) / tx);
}

/* ------------------------------------------------------------------------------------------ */
/* Utilities                                                                                  */
/* ------------------------------------------------------------------------------------------ */

double
Percentile(std::vector<double> samples, double percentile)
{
    if (samples.empty())
    {
        return 0;
    }
    std::sort(samples.begin(), samples.end());
    const double rank = percentile / 100.0 * (samples.size() - 1);
    const auto lo = static_cast<std::size_t>(std::floor(rank));
    const auto hi = static_cast<std::size_t>(std::ceil(rank));
    const double frac = rank - lo;
    return samples[lo] + (samples[hi] - samples[lo]) * frac;
}

namespace
{

WifiTxVector
MakeDataTxVector(WifiStandard standard,
                 uint8_t mcs,
                 MHz_u width,
                 uint16_t guardIntervalNs,
                 uint8_t nss,
                 bool ldpc)
{
    WifiTxVector txVector;
    txVector.SetMode(WifiMode(DataModeName(standard, mcs)));
    txVector.SetPreambleType(standard == WIFI_STANDARD_80211be ? WIFI_PREAMBLE_EHT_MU
                                                               : WIFI_PREAMBLE_HE_SU);
    txVector.SetChannelWidth(width);
    txVector.SetGuardInterval(NanoSeconds(guardIntervalNs));
    txVector.SetNss(nss);
    txVector.SetNTx(nss);
    txVector.SetLdpc(ldpc);
    txVector.SetAggregation(true);
    return txVector;
}

} // namespace

double
PpduDurationUs(uint32_t psduBytes,
               WifiStandard standard,
               uint8_t mcs,
               MHz_u width,
               uint16_t guardIntervalNs,
               uint8_t nss,
               Band band,
               bool ldpc)
{
    const auto txVector = MakeDataTxVector(standard, mcs, width, guardIntervalNs, nss, ldpc);
    return WifiPhy::CalculateTxDuration(psduBytes, txVector, ToPhyBand(band)).GetNanoSeconds() /
           1e3;
}

double
ControlPpduDurationUs(uint32_t bytes, const std::string& modeName, Band band)
{
    WifiTxVector txVector;
    WifiMode mode(modeName);
    txVector.SetMode(mode);
    const auto modClass = mode.GetModulationClass();
    if (modClass == WIFI_MOD_CLASS_HE)
    {
        txVector.SetPreambleType(WIFI_PREAMBLE_HE_SU);
        txVector.SetGuardInterval(NanoSeconds(800));
    }
    else if (modClass == WIFI_MOD_CLASS_EHT)
    {
        txVector.SetPreambleType(WIFI_PREAMBLE_EHT_MU);
        txVector.SetGuardInterval(NanoSeconds(800));
    }
    else
    {
        txVector.SetPreambleType(WIFI_PREAMBLE_LONG);
    }
    txVector.SetChannelWidth(MHz_u{20});
    txVector.SetNss(1);
    return WifiPhy::CalculateTxDuration(bytes, txVector, ToPhyBand(band)).GetNanoSeconds() / 1e3;
}

double
PreambleDurationUs(WifiStandard standard,
                   uint8_t mcs,
                   MHz_u width,
                   uint16_t guardIntervalNs,
                   uint8_t nss)
{
    const auto txVector = MakeDataTxVector(standard, mcs, width, guardIntervalNs, nss, true);
    return WifiPhy::CalculatePhyPreambleAndHeaderDuration(txVector).GetNanoSeconds() / 1e3;
}

std::pair<uint32_t, double>
SaturatedAmpdu(uint32_t mpduBytes,
               uint16_t baWindow,
               uint32_t maxAmpduBytes,
               double maxPpduUs,
               WifiStandard standard,
               uint8_t mcs,
               MHz_u width,
               uint16_t guardIntervalNs,
               uint8_t nss,
               Band band)
{
    const uint32_t subframe = GetAmpduSubframeBytes(mpduBytes);
    uint32_t best = 0;
    double bestDuration = 0;
    for (uint32_t k = 1; k <= baWindow; ++k)
    {
        if (k * subframe > maxAmpduBytes)
        {
            break;
        }
        const double duration =
            PpduDurationUs(k * subframe, standard, mcs, width, guardIntervalNs, nss, band);
        if (duration > maxPpduUs)
        {
            break;
        }
        best = k;
        bestDuration = duration;
    }
    if (best == 0)
    {
        best = 1;
        bestDuration = PpduDurationUs(subframe, standard, mcs, width, guardIntervalNs, nss, band);
    }
    return {best, bestDuration};
}

double
EnergyMonitor::GetConsumedJoules(uint32_t i) const
{
    return models.at(i)->GetTotalEnergyConsumption();
}

EnergyMonitor
InstallEnergy(const NetDeviceContainer& devices,
              double idleA,
              double txA,
              double rxA,
              double sleepA,
              double ccaBusyA)
{
    EnergyMonitor monitor;
    BasicEnergySourceHelper sourceHelper;
    sourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1e6));
    sourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(3.0));
    WifiRadioEnergyModelHelper radioHelper;
    radioHelper.Set("IdleCurrentA", DoubleValue(idleA));
    radioHelper.Set("TxCurrentA", DoubleValue(txA));
    radioHelper.Set("RxCurrentA", DoubleValue(rxA));
    radioHelper.Set("SleepCurrentA", DoubleValue(sleepA));
    radioHelper.Set("CcaBusyCurrentA", DoubleValue(ccaBusyA));
    for (auto it = devices.Begin(); it != devices.End(); ++it)
    {
        auto sources = sourceHelper.Install((*it)->GetNode());
        monitor.sources.Add(sources);
        auto models = radioHelper.Install(*it, sources.Get(0));
        monitor.models.push_back(models.Get(0));
    }
    return monitor;
}

double
StateFraction(const WifiCoTraceHelper& helper,
              uint32_t nodeId,
              uint32_t deviceId,
              uint8_t linkId,
              const std::set<WifiPhyState>& states)
{
    for (const auto& record : helper.GetDeviceRecords())
    {
        if (record.m_nodeId != nodeId || record.m_ifIndex != deviceId)
        {
            continue;
        }
        auto it = record.m_linkStateDurations.find(linkId);
        if (it == record.m_linkStateDurations.end())
        {
            return 0;
        }
        Time total;
        Time selected;
        for (const auto& [state, duration] : it->second)
        {
            total += duration;
            if (states.count(state))
            {
                selected += duration;
            }
        }
        return total.IsStrictlyPositive() ? selected.GetDouble() / total.GetDouble() : 0;
    }
    return 0;
}

/* ------------------------------------------------------------------------------------------ */
/* TX accounting                                                                              */
/* ------------------------------------------------------------------------------------------ */

namespace
{

void
TxAccountingTrampoline(TxAccounting* self,
                       uint32_t nodeId,
                       uint8_t linkId,
                       WifiPhyBand band,
                       WifiConstPsduMap psduMap,
                       WifiTxVector txVector,
                       Watt_u txPower)
{
    self->Notify(nodeId, linkId, band, psduMap, txVector, txPower);
}

} // namespace

void
TxAccounting::Enable(Ptr<WifiNetDevice> device)
{
    const uint32_t nodeId = device->GetNode()->GetId();
    for (uint8_t linkId = 0; linkId < device->GetNPhys(); ++linkId)
    {
        auto phy = device->GetPhy(linkId);
        phy->TraceConnectWithoutContext(
            "PhyTxPsduBegin",
            MakeBoundCallback(&TxAccountingTrampoline, this, nodeId, linkId, phy->GetPhyBand()));
    }
}

void
TxAccounting::Enable(const NetDeviceContainer& devices)
{
    for (auto it = devices.Begin(); it != devices.End(); ++it)
    {
        Enable(DynamicCast<WifiNetDevice>(*it));
    }
}

void
TxAccounting::SetWindow(Time start, Time stop)
{
    m_start = start;
    m_stop = stop;
}

void
TxAccounting::Notify(uint32_t nodeId,
                     uint8_t linkId,
                     WifiPhyBand band,
                     WifiConstPsduMap psduMap,
                     WifiTxVector txVector,
                     Watt_u /* txPower */)
{
    const Time now = Simulator::Now();
    if (now < m_start || now > m_stop)
    {
        return;
    }
    PpduRecord rec;
    rec.time = now;
    rec.nodeId = nodeId;
    rec.linkId = linkId;
    rec.psduBytes = 0;
    rec.nMpdus = 0;
    rec.nUsers = psduMap.size();
    for (const auto& [staId, psdu] : psduMap)
    {
        rec.psduBytes += psdu->GetSize();
        rec.nMpdus += psdu->GetNMpdus();
    }
    rec.durationUs = WifiPhy::CalculateTxDuration(psduMap, txVector, band).GetNanoSeconds() / 1e3;
    std::ostringstream oss;
    oss << txVector.GetPreambleType();
    rec.preamble = oss.str();
    const auto firstStaId = psduMap.begin()->first;
    rec.mode = txVector.IsMu() ? txVector.GetMode(firstStaId).GetUniqueName()
                               : txVector.GetMode().GetUniqueName();
    rec.widthMhz = txVector.GetChannelWidth();
    rec.mu = txVector.IsMu();
    if (rec.mu)
    {
        rec.ruType = WifiRu::GetRuType(txVector.GetRu(firstStaId));
    }
    m_records.push_back(rec);
}

const std::vector<TxAccounting::PpduRecord>&
TxAccounting::GetRecords() const
{
    return m_records;
}

uint64_t
TxAccounting::GetNPpdus(const std::string& preambleFilter) const
{
    if (preambleFilter.empty())
    {
        return m_records.size();
    }
    return std::count_if(m_records.begin(), m_records.end(), [&](const PpduRecord& r) {
        return r.preamble == preambleFilter;
    });
}

double
TxAccounting::GetMeanMpdusPerPpdu() const
{
    if (m_records.empty())
    {
        return 0;
    }
    double sum = 0;
    for (const auto& r : m_records)
    {
        sum += r.nMpdus;
    }
    return sum / m_records.size();
}

double
TxAccounting::GetMeanDurationUs() const
{
    if (m_records.empty())
    {
        return 0;
    }
    return GetTotalAirtimeUs() / m_records.size();
}

double
TxAccounting::GetTotalAirtimeUs() const
{
    double sum = 0;
    for (const auto& r : m_records)
    {
        sum += r.durationUs;
    }
    return sum;
}

RuType
TxAccounting::GetDominantRuType(const std::string& preamble) const
{
    std::map<RuType, uint64_t> counts;
    for (const auto& r : m_records)
    {
        if (r.mu && r.preamble == preamble)
        {
            counts[r.ruType]++;
        }
    }
    RuType best = RuType::RU_TYPE_MAX;
    uint64_t bestCount = 0;
    for (const auto& [ru, n] : counts)
    {
        if (n > bestCount)
        {
            best = ru;
            bestCount = n;
        }
    }
    return best;
}

std::string
TxAccounting::GetDominantMode(const std::string& preamble) const
{
    std::map<std::string, uint64_t> counts;
    for (const auto& r : m_records)
    {
        if (r.preamble == preamble)
        {
            counts[r.mode]++;
        }
    }
    std::string best;
    uint64_t bestCount = 0;
    for (const auto& [mode, n] : counts)
    {
        if (n > bestCount)
        {
            best = mode;
            bestCount = n;
        }
    }
    return best;
}

std::string
TxAccounting::Summary() const
{
    std::ostringstream oss;
    std::map<std::string, uint64_t> byPreamble;
    for (const auto& r : m_records)
    {
        byPreamble[r.preamble]++;
    }
    oss << m_records.size() << " PPDUs, mean " << GetMeanMpdusPerPpdu() << " MPDUs/PPDU, mean "
        << GetMeanDurationUs() << " us/PPDU, airtime " << GetTotalAirtimeUs() / 1e3 << " ms";
    for (const auto& [p, n] : byPreamble)
    {
        oss << ", " << p << "=" << n;
    }
    return oss.str();
}

std::string
StandardName(WifiStandard standard)
{
    switch (standard)
    {
    case WIFI_STANDARD_80211ax:
        return "802.11ax (Wi-Fi 6)";
    case WIFI_STANDARD_80211be:
        return "802.11be (Wi-Fi 7)";
    default:
        return "unknown";
    }
}

WifiStandard
ParseStandard(const std::string& name)
{
    if (name == "ax" || name == "11ax" || name == "802.11ax" || name == "wifi6")
    {
        return WIFI_STANDARD_80211ax;
    }
    if (name == "be" || name == "11be" || name == "802.11be" || name == "wifi7")
    {
        return WIFI_STANDARD_80211be;
    }
    NS_ABORT_MSG("Unknown standard " << name << " (use ax or be)");
    return WIFI_STANDARD_80211ax;
}

uint8_t
MaxMcs(WifiStandard standard)
{
    return standard == WIFI_STANDARD_80211be ? 13 : 11;
}

} // namespace wifibench
} // namespace ns3
