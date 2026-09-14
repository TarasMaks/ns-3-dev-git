/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef WIFI_BENCHMARK_HELPER_H
#define WIFI_BENCHMARK_HELPER_H

#include "ns3/application.h"
#include "ns3/command-line.h"
#include "ns3/data-rate.h"
#include "ns3/energy-source-container.h"
#include "ns3/event-id.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/net-device-container.h"
#include "ns3/node-container.h"
#include "ns3/nstime.h"
#include "ns3/packet-sink.h"
#include "ns3/ptr.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/spectrum-channel.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/wifi-co-trace-helper.h"
#include "ns3/wifi-helper.h"
#include "ns3/wifi-mac-helper.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-phy-band.h"
#include "ns3/wifi-psdu.h"
#include "ns3/wifi-radio-energy-model.h"
#include "ns3/wifi-ru.h"
#include "ns3/wifi-standards.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/wifi-types.h"
#include "ns3/wifi-units.h"

#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ns3
{

class ApWifiMac;
class Socket;

namespace wifibench
{

/**
 * @ingroup wifi-benchmarks
 * A row of a result table: an ordered list of (column, value) cells.
 */
class ResultRow
{
  public:
    /**
     * Set a string cell.
     * @param key the column name
     * @param value the value
     * @return this row
     */
    ResultRow& Set(const std::string& key, const std::string& value);
    /**
     * Set a floating point cell.
     * @param key the column name
     * @param value the value
     * @param precision the number of decimals
     * @return this row
     */
    ResultRow& Set(const std::string& key, double value, int precision = 3);
    /**
     * Set an integer cell.
     * @param key the column name
     * @param value the value
     * @return this row
     */
    ResultRow& Set(const std::string& key, int64_t value);
    /**
     * Set an unsigned integer cell.
     * @param key the column name
     * @param value the value
     * @return this row
     */
    ResultRow& Set(const std::string& key, uint64_t value);
    /**
     * Set an unsigned integer cell.
     * @param key the column name
     * @param value the value
     * @return this row
     */
    ResultRow& Set(const std::string& key, uint32_t value);
    /**
     * Set an integer cell.
     * @param key the column name
     * @param value the value
     * @return this row
     */
    ResultRow& Set(const std::string& key, int value);
    /**
     * @param key the column name
     * @return the cell value (empty string if unset)
     */
    std::string Get(const std::string& key) const;
    /// @return the cells in insertion order
    const std::vector<std::pair<std::string, std::string>>& GetCells() const;

  private:
    std::vector<std::pair<std::string, std::string>> m_cells; ///< cells in insertion order
};

/**
 * @ingroup wifi-benchmarks
 * Kind of comparison between a measured value and a reference value.
 */
enum class CheckKind
{
    WITHIN,   ///< PASS if |measured - reference| <= tolerance * |reference|
    AT_LEAST, ///< PASS if measured >= reference * (1 - tolerance)
    AT_MOST,  ///< PASS if measured <= reference * (1 + tolerance)
    INFO      ///< no verdict, only the deviation is reported
};

/**
 * Compare a measured value with a reference value and store the outcome in the row (columns
 * "reference", "deviation_pct", "verdict" and "ref_source").
 *
 * @param row the row to update
 * @param measured the measured value
 * @param reference the reference value
 * @param tolerance the relative tolerance (e.g., 0.05 for 5%)
 * @param source the citation key of the reference
 * @param kind the kind of comparison
 * @return the verdict ("PASS", "FAIL" or "INFO")
 */
std::string Compare(ResultRow& row,
                    double measured,
                    double reference,
                    double tolerance,
                    const std::string& source,
                    CheckKind kind = CheckKind::WITHIN);

/**
 * @ingroup wifi-benchmarks
 * A table of benchmark results, printed to the standard output and written as CSV and JSON.
 */
class ResultTable
{
  public:
    /**
     * @param benchmark the benchmark name (used as file name)
     * @param outputDir the output directory
     */
    ResultTable(const std::string& benchmark, const std::string& outputDir);
    /**
     * @param row the row to add
     */
    void AddRow(const ResultRow& row);
    /// Print the table to the standard output
    void Print() const;
    /// Write the CSV and JSON files
    void Write() const;
    /**
     * Print a summary of the verdicts.
     * @return the number of rows with a FAIL verdict
     */
    std::size_t PrintSummary() const;
    /// @return the rows
    const std::vector<ResultRow>& GetRows() const;
    /// @return the benchmark name
    const std::string& GetName() const;

  private:
    std::string m_benchmark;            ///< benchmark name
    std::string m_outputDir;            ///< output directory
    std::vector<std::string> m_columns; ///< column names in first-seen order
    std::vector<ResultRow> m_rows;      ///< rows
};

/**
 * Print a banner with the benchmark title, its purpose and the references used.
 *
 * @param title the benchmark title
 * @param description what the benchmark measures
 * @param citationKeys the keys of the citations used as references
 */
void PrintBanner(const std::string& title,
                 const std::string& description,
                 const std::vector<std::string>& citationKeys);

/**
 * @ingroup wifi-benchmarks
 * Command line options common to all the benchmark scripts.
 */
struct CommonArgs
{
    std::string outputDir{"wifi-benchmarks-results"}; ///< output directory
    bool quick{false};   ///< reduce the number of runs and their duration (regression tests)
    uint32_t rngRun{1};  ///< RNG run number
    bool verbose{false}; ///< print per-run details
};

/**
 * Add the common options to a command line parser.
 * @param cmd the command line parser
 * @param args the arguments to fill
 */
void AddCommonArgs(CommandLine& cmd, CommonArgs& args);

/**
 * Apply the common options: set the RNG run number and create the output directory.
 * @param args the arguments
 */
void ApplyCommonArgs(const CommonArgs& args);

/**
 * @ingroup wifi-benchmarks
 * Frequency band of a link.
 */
enum class Band
{
    GHZ_2_4,
    GHZ_5,
    GHZ_6
};

/**
 * @param band the band
 * @return the ns-3 PHY band
 */
WifiPhyBand ToPhyBand(Band band);
/**
 * @param band the band
 * @return the spectrum frequency range of the band
 */
FrequencyRange ToFrequencyRange(Band band);
/**
 * @param band the band
 * @return a printable name ("2.4GHz", "5GHz", "6GHz")
 */
std::string BandName(Band band);
/**
 * @param band the band
 * @return the ChannelSettings band token ("BAND_5GHZ", ...)
 */
std::string BandToken(Band band);
/**
 * @param ghz the band expressed as 2.4, 5 or 6
 * @return the band
 */
Band BandFromGhz(double ghz);

/**
 * @ingroup wifi-benchmarks
 * Configuration of one link (RF interface) of a device.
 */
struct LinkConfig
{
    Band band{Band::GHZ_5};    ///< frequency band
    MHz_u width{MHz_u{80}};    ///< channel width
    uint16_t channelNumber{0}; ///< channel number (0 = default channel for the width and band)
    uint8_t primary20{0};      ///< index of the primary 20 MHz channel
};

/**
 * @param link the link configuration
 * @return the WifiPhy ChannelSettings string, e.g. "{0, 80, BAND_5GHZ, 0}"
 */
std::string ChannelSettings(const LinkConfig& link);

/**
 * @ingroup wifi-benchmarks
 * PHY configuration shared by all links of a device.
 */
struct PhyConfig
{
    dBm_u txPower{dBm_u{20}};                   ///< transmit power
    double rxNoiseFigureDb{7};                  ///< receiver noise figure
    dBm_u ccaEdThreshold{dBm_u{-62}};           ///< CCA-ED threshold
    dBm_u rxSensitivity{dBm_u{-92}};            ///< receiver sensitivity (preamble detection floor)
    uint8_t nss{1};                             ///< spatial streams (and antennas)
    Time channelSwitchDelay{MicroSeconds(100)}; ///< channel switch delay
};

/**
 * @ingroup wifi-benchmarks
 * Propagation configuration of a spectrum channel.
 */
struct ChannelConfig
{
    std::string lossModel{"logdistance"};  ///< "logdistance", "friis" or "3gpp-inh"
    double logDistanceExponent{3.0};       ///< path loss exponent of the log-distance model
    std::optional<double> referenceLossDb; ///< reference loss at 1 m (default depends on band)
    bool shadowing{false};                 ///< enable shadowing (3GPP model only)
};

/**
 * Create a spectrum channel with the propagation loss model of the configuration and a
 * constant speed propagation delay model.
 *
 * @param band the band (selects the default reference loss)
 * @param cfg the channel configuration
 * @return the spectrum channel
 */
Ptr<SpectrumChannel> CreateSpectrumChannel(Band band, const ChannelConfig& cfg = ChannelConfig());

/**
 * @ingroup wifi-benchmarks
 * Rate control configuration.
 */
struct RateConfig
{
    int mcs{-1};                                  ///< fixed MCS (-1 = adaptive)
    std::string manager{"ns3::IdealWifiManager"}; ///< manager used when mcs is -1
    std::optional<std::string> controlMode6GHz;   ///< control mode used in 6 GHz (fixed MCS)
};

/**
 * @param standard the standard
 * @param mcs the MCS index
 * @return the WifiMode name ("HeMcs7" or "EhtMcs7")
 */
std::string DataModeName(WifiStandard standard, uint8_t mcs);

/**
 * Control (response) mode used together with a fixed data MCS: the non-HT reference rate in
 * 2.4/5 GHz (ERP-OFDM/OFDM), the HE MCS with the same modulation and coding in 6 GHz.
 *
 * @param standard the standard
 * @param band the band
 * @param mcs the data MCS index
 * @return the WifiMode name
 */
std::string ControlModeName(WifiStandard standard, Band band, uint8_t mcs);

/**
 * Configure the remote station manager of a link.
 *
 * @param wifi the wifi helper
 * @param linkId the link ID
 * @param standard the standard
 * @param band the band of the link
 * @param rate the rate configuration
 */
void ConfigureRateManager(WifiHelper& wifi,
                          uint8_t linkId,
                          WifiStandard standard,
                          Band band,
                          const RateConfig& rate);

/**
 * @ingroup wifi-benchmarks
 * MAC configuration.
 */
struct MacConfig
{
    std::optional<uint16_t> mpduBufferSize; ///< BlockAck buffer size (default: 256 HE, 1024 EHT)
    std::optional<uint32_t> maxAmpduSize;   ///< max A-MPDU size for all ACs (default: standard max)
    uint32_t maxAmsduSize{0};               ///< max A-MSDU size (0 = disabled)
    std::optional<Time> txopLimit;          ///< TXOP limit of AC_BE (default: ns-3 default, 0)
    bool rtsCts{false};                     ///< enable RTS/CTS (and MU-RTS)
    uint16_t guardIntervalNs{800};          ///< guard interval
    bool ldpc{true};                        ///< LDPC support
    uint32_t macQueueSize{5000};            ///< MAC queue size in packets
    Time macQueueMaxDelay{Seconds(10)};     ///< MAC queue max delay
    std::set<uint8_t> baTids{0};            ///< TIDs with static Block Ack agreements
    bool staticSetup{true};                 ///< static association and Block Ack setup
};

/**
 * @ingroup wifi-benchmarks
 * Multi-user (OFDMA) scheduler configuration for the AP.
 */
struct MuSchedulerConfig
{
    std::string dlAckType{"AGGR-MU-BAR"}; ///< NO-OFDMA, ACK-SU-FORMAT, MU-BAR or AGGR-MU-BAR
    bool enableUlOfdma{false};            ///< enable UL OFDMA (Basic Trigger Frames)
    bool enableBsrp{false};               ///< enable BSRP Trigger Frames
    uint8_t nStations{4};                 ///< max number of stations per MU PPDU
    Time accessReqInterval{Seconds(0)};   ///< periodic channel access requests (UL-only traffic)
    uint32_t ulPsduSize{500};             ///< UL PSDU size solicited by Basic TFs
    bool useCentral26TonesRus{false};     ///< use central 26-tone RUs
    bool enableTxopSharing{true};         ///< allow A-MPDUs of different TIDs in a DL MU PPDU
};

/**
 * @ingroup wifi-benchmarks
 * EMLSR configuration of the non-AP MLDs.
 */
struct EmlsrConfig
{
    std::string manager{"ns3::DefaultEmlsrManager"}; ///< EMLSR manager type
    std::string linkSet{"0,1"};                      ///< EMLSR links
    uint8_t mainPhyId{0};                            ///< main PHY (link) ID
    Time paddingDelay{MicroSeconds(32)};             ///< EMLSR padding delay
    Time transitionDelay{MicroSeconds(128)};         ///< EMLSR transition delay
    Time transitionTimeout{MicroSeconds(1024)};      ///< AP transition timeout
    Time mediumSyncDuration{MicroSeconds(3200)};     ///< medium sync duration
    bool switchAuxPhy{true};                         ///< aux PHY switches to the main PHY link
    bool auxPhyTxCapable{true};                      ///< aux PHYs can transmit
    uint16_t auxPhyChannelWidth{20};                 ///< max channel width of aux PHYs
};

/**
 * @ingroup wifi-benchmarks
 * The devices and addresses of one BSS.
 */
struct Bss
{
    NodeContainer apNode;          ///< AP node
    NodeContainer staNodes;        ///< STA nodes
    NetDeviceContainer apDevice;   ///< AP device
    NetDeviceContainer staDevices; ///< STA devices
    Ipv4InterfaceContainer apIf;   ///< AP interface
    Ipv4InterfaceContainer staIfs; ///< STA interfaces
    Ssid ssid;                     ///< SSID
    uint8_t nLinks{1};             ///< number of links

    /// @return the AP WifiNetDevice
    Ptr<WifiNetDevice> ApDev() const;
    /// @return the AP MAC
    Ptr<ApWifiMac> ApMac() const;
    /**
     * @param i the STA index
     * @return the STA WifiNetDevice
     */
    Ptr<WifiNetDevice> StaDev(uint32_t i) const;
    /// @return all devices (AP first)
    NetDeviceContainer AllDevices() const;
    /// @return all nodes (AP first)
    NodeContainer AllNodes() const;
};

/**
 * @ingroup wifi-benchmarks
 * Builder of an infrastructure BSS (possibly multi-link) with the benchmark defaults.
 */
class BssBuilder
{
  public:
    /// Customizer of the PHY helper, MAC helper or wifi helper
    using PhyCustomizer = std::function<void(SpectrumWifiPhyHelper&)>;
    using MacCustomizer = std::function<void(WifiMacHelper&)>;
    using WifiCustomizer = std::function<void(WifiHelper&)>;

    BssBuilder();

    BssBuilder& SetStandard(WifiStandard standard); //!< @param standard the standard @return this
    BssBuilder& AddLink(const LinkConfig& link);    //!< @param link a link to add @return this
    BssBuilder& SetLinks(
        const std::vector<LinkConfig>& links); //!< @param links the links @return this
    BssBuilder& SetPhy(const PhyConfig& phy);  //!< @param phy PHY configuration @return this
    /**
     * Set the spectrum channel used by the links operating in a band.
     * @param band the band
     * @param channel the channel
     * @return this
     */
    BssBuilder& SetChannel(Band band, Ptr<SpectrumChannel> channel);
    BssBuilder& SetRate(const RateConfig& rate);  //!< @param rate rate configuration @return this
    BssBuilder& SetMac(const MacConfig& mac);     //!< @param mac MAC configuration @return this
    BssBuilder& SetSsid(const std::string& ssid); //!< @param ssid the SSID @return this
    BssBuilder& SetBssColor(uint8_t color);       //!< @param color the BSS color @return this
    BssBuilder& SetObssPdLevel(dBm_u level);      //!< @param level OBSS-PD level @return this
    BssBuilder& SetMuScheduler(const MuSchedulerConfig& mu); //!< @param mu config @return this
    BssBuilder& SetEmlsr(const EmlsrConfig& emlsr); //!< @param emlsr EMLSR config @return this
    /**
     * Set the TID-to-link mapping advertised by the AP MLD and requested by the non-AP MLDs.
     * @param dl the DL mapping, e.g. "0,1,2,3 0; 4,5,6,7 1"
     * @param ul the UL mapping
     * @return this
     */
    BssBuilder& SetTidToLinkMapping(const std::string& dl, const std::string& ul);
    BssBuilder& SetPhyCustomizer(PhyCustomizer f);    //!< @param f customizer @return this
    BssBuilder& SetStaMacCustomizer(MacCustomizer f); //!< @param f customizer @return this
    BssBuilder& SetApMacCustomizer(MacCustomizer f);  //!< @param f customizer @return this
    BssBuilder& SetWifiCustomizer(WifiCustomizer f);  //!< @param f customizer @return this

    /**
     * Create the devices.
     * @param apNode the AP node
     * @param staNodes the STA nodes
     * @return the BSS
     */
    Bss Build(NodeContainer apNode, NodeContainer staNodes);

    /// @return the standard
    WifiStandard GetStandard() const;
    /// @return the links
    const std::vector<LinkConfig>& GetLinks() const;
    /// @return the MAC configuration
    const MacConfig& GetMac() const;

  private:
    WifiStandard m_standard{WIFI_STANDARD_80211ax};            ///< standard
    std::vector<LinkConfig> m_links;                           ///< links
    PhyConfig m_phy;                                           ///< PHY configuration
    std::map<Band, Ptr<SpectrumChannel>> m_channels;           ///< spectrum channels per band
    RateConfig m_rate;                                         ///< rate configuration
    MacConfig m_mac;                                           ///< MAC configuration
    std::string m_ssid{"wifi-bench"};                          ///< SSID
    std::optional<uint8_t> m_bssColor;                         ///< BSS color
    std::optional<dBm_u> m_obssPdLevel;                        ///< OBSS-PD level
    std::optional<MuSchedulerConfig> m_mu;                     ///< MU scheduler configuration
    std::optional<EmlsrConfig> m_emlsr;                        ///< EMLSR configuration
    std::optional<std::pair<std::string, std::string>> m_t2lm; ///< TID-to-link mapping
    PhyCustomizer m_phyCustomizer;                             ///< PHY customizer
    MacCustomizer m_staMacCustomizer;                          ///< STA MAC customizer
    MacCustomizer m_apMacCustomizer;                           ///< AP MAC customizer
    WifiCustomizer m_wifiCustomizer;                           ///< wifi helper customizer
};

/**
 * Install the Internet stack on the nodes of a BSS and assign IPv4 addresses in the subnet
 * 10.<subnet>.0.0/16.
 *
 * @param bss the BSS
 * @param subnet the subnet index
 */
void InstallInternet(Bss& bss, uint8_t subnet);

/**
 * Populate the ARP caches of all nodes (to be called after all addresses are assigned).
 */
void PopulateArpCaches();

/**
 * Place the AP at the origin and the STAs on a circle of given radius around it.
 *
 * @param bss the BSS
 * @param radius the distance between AP and STAs
 * @param apPosition the AP position
 */
void PlaceOnCircle(const Bss& bss, double radius, Vector apPosition = Vector(0, 0, 0));

/**
 * @ingroup wifi-benchmarks
 * Traffic pattern of a UDP source.
 */
enum class TrafficPattern
{
    CBR,     ///< constant bit rate
    POISSON, ///< Poisson arrivals (exponential inter-packet times)
    BURST    ///< periodic bursts of packets
};

/**
 * @ingroup wifi-benchmarks
 * UDP source that stamps each packet with a SeqTsSizeHeader so that a PacketSink with
 * EnableSeqTsSizeHeader can measure the one-way latency.
 */
class BenchUdpSource : public Application
{
  public:
    /// @return the TypeId
    static TypeId GetTypeId();
    BenchUdpSource();
    ~BenchUdpSource() override;

    /// @return the number of packets sent so far
    uint64_t GetSentPackets() const;

  private:
    void StartApplication() override;
    void StopApplication() override;
    /// Send a packet and schedule the next one
    void SendPacket();
    /// Schedule the next transmission
    void ScheduleNext();

    Address m_remote;                              ///< remote address
    uint32_t m_size{1000};                         ///< packet size (bytes, including the header)
    DataRate m_rate{DataRate("1Mbps")};            ///< data rate
    TrafficPattern m_pattern{TrafficPattern::CBR}; ///< traffic pattern
    uint8_t m_tos{0};                              ///< IP TOS
    uint32_t m_burstPackets{10};                   ///< packets per burst
    Time m_burstInterval{MilliSeconds(10)};        ///< burst interval
    Ptr<Socket> m_socket;                          ///< socket
    EventId m_sendEvent;                           ///< send event
    uint32_t m_seq{0};                             ///< sequence number
    uint64_t m_sent{0};                            ///< packets sent
    Ptr<RandomVariableStream> m_expVar;            ///< exponential inter-arrival (Poisson)
};

/**
 * @ingroup wifi-benchmarks
 * Statistics of a flow over the measurement window.
 */
struct FlowStats
{
    std::string label;        ///< flow label
    double throughputMbps{0}; ///< received goodput
    uint64_t txPackets{0};    ///< packets sent (UDP only)
    uint64_t rxPackets{0};    ///< packets received
    uint64_t rxBytes{0};      ///< bytes received
    double lossRatio{0};      ///< 1 - rx/tx (UDP only)
    double meanLatencyMs{0};  ///< mean one-way latency
    double p50LatencyMs{0};   ///< median latency
    double p95LatencyMs{0};   ///< 95th percentile latency
    double p99LatencyMs{0};   ///< 99th percentile latency
    double p999LatencyMs{0};  ///< 99.9th percentile latency
    double maxLatencyMs{0};   ///< maximum latency
    double maxRxGapMs{0};     ///< maximum interval between consecutive receptions
};

/**
 * @ingroup wifi-benchmarks
 * A set of measured application flows (UDP or TCP).
 */
class FlowSet
{
  public:
    /**
     * Set the measurement window (only packets received within the window are accounted).
     * Must be called before adding flows.
     * @param start the start of the window
     * @param stop the end of the window
     */
    void SetMeasurementWindow(Time start, Time stop);

    /**
     * Add a UDP flow.
     * @param label the flow label
     * @param src the source node
     * @param dst the destination node
     * @param dstAddr the destination address
     * @param rateMbps the offered load in Mb/s
     * @param packetSize the packet size (bytes, UDP payload including the 20-byte header)
     * @param start the application start time
     * @param stop the application stop time
     * @param tos the IP TOS (0x00 BE, 0x20 BK, 0xa0 VI, 0xc0 VO)
     * @param pattern the traffic pattern
     * @return the flow ID
     */
    uint32_t AddUdpFlow(const std::string& label,
                        Ptr<Node> src,
                        Ptr<Node> dst,
                        Ipv4Address dstAddr,
                        double rateMbps,
                        uint32_t packetSize,
                        Time start,
                        Time stop,
                        uint8_t tos = 0,
                        TrafficPattern pattern = TrafficPattern::CBR);

    /**
     * Add a TCP flow (OnOff at a given rate, or BulkSend if the rate is 0).
     * @param label the flow label
     * @param src the source node
     * @param dst the destination node
     * @param dstAddr the destination address
     * @param rateMbps the offered load in Mb/s (0 = unlimited)
     * @param packetSize the packet size
     * @param start the application start time
     * @param stop the application stop time
     * @param tos the IP TOS
     * @return the flow ID
     */
    uint32_t AddTcpFlow(const std::string& label,
                        Ptr<Node> src,
                        Ptr<Node> dst,
                        Ipv4Address dstAddr,
                        double rateMbps,
                        uint32_t packetSize,
                        Time start,
                        Time stop,
                        uint8_t tos = 0);

    /**
     * @param id the flow ID
     * @return the statistics of the flow
     */
    FlowStats GetStats(uint32_t id) const;
    /// @return the statistics of all flows aggregated
    FlowStats GetAggregateStats() const;
    /// @return the statistics of every flow
    std::vector<FlowStats> GetAllStats() const;
    /// @return the number of flows
    uint32_t GetNFlows() const;
    /**
     * Compute a percentile of the per-flow throughputs.
     * @param percentile the percentile (0-100)
     * @return the throughput percentile in Mb/s
     */
    double GetThroughputPercentile(double percentile) const;
    /**
     * Compute a percentile of the latency across all packets of all flows.
     * @param percentile the percentile (0-100)
     * @return the latency percentile in ms
     */
    double GetLatencyPercentile(double percentile) const;
    /**
     * Fraction of the packets sent (UDP flows) that were not delivered within a deadline (lost
     * packets count as missed).
     * @param deadlineMs the deadline in ms
     * @return the deadline miss ratio
     */
    double GetDeadlineMissRatio(double deadlineMs) const;

    /**
     * Sink trace callback.
     * @param id the flow ID
     * @param p the packet
     * @param from the source address
     * @param to the destination address
     * @param header the SeqTsSize header
     */
    void Rx(uint32_t id,
            Ptr<const Packet> p,
            const Address& from,
            const Address& to,
            const SeqTsSizeHeader& header);

  private:
    /// Per-flow bookkeeping
    struct Flow
    {
        std::string label;               ///< label
        bool tcp{false};                 ///< TCP flow
        Ptr<Node> src;                   ///< source node
        Ptr<Node> dst;                   ///< destination node
        uint16_t port{0};                ///< destination port
        Ptr<BenchUdpSource> udpSource;   ///< UDP source (if UDP)
        Ptr<PacketSink> sink;            ///< sink
        uint64_t rxPackets{0};           ///< packets received in the window
        uint64_t rxBytes{0};             ///< bytes received in the window
        uint64_t txAtStart{0};           ///< packets sent before the window
        uint64_t txAtStop{0};            ///< packets sent before the end of the window
        bool stopSampled{false};         ///< whether txAtStop has been sampled
        std::vector<double> latenciesMs; ///< latency samples
        Time lastRx{Seconds(0)};         ///< time of the last reception in the window
        double maxRxGapMs{0};            ///< maximum interval between consecutive receptions
    };

    /**
     * Install a sink for a flow.
     * @param flow the flow
     * @param id the flow ID
     * @param start the start time
     * @param stop the stop time
     */
    void InstallSink(Flow& flow, uint32_t id, Time start, Time stop);
    /**
     * Compute the statistics of a flow.
     * @param flow the flow
     * @return the statistics
     */
    FlowStats ComputeStats(const Flow& flow) const;

    std::vector<Flow> m_flows;  ///< flows
    Time m_start{Seconds(0)};   ///< measurement window start
    Time m_stop{Time::Max()};   ///< measurement window end
    static uint16_t s_nextPort; ///< next sink port (unique across all flow sets)
};

/**
 * Compute a percentile with linear interpolation.
 * @param samples the samples (copied and sorted)
 * @param percentile the percentile (0-100)
 * @return the percentile (0 if there are no samples)
 */
double Percentile(std::vector<double> samples, double percentile);

/**
 * Duration of a PPDU carrying a PSDU of given size, computed with WifiPhy::CalculateTxDuration.
 *
 * @param psduBytes the PSDU size
 * @param standard the standard
 * @param mcs the MCS index
 * @param width the channel width
 * @param guardIntervalNs the guard interval
 * @param nss the number of spatial streams
 * @param band the band
 * @param ldpc whether LDPC is used
 * @return the duration in microseconds
 */
double PpduDurationUs(uint32_t psduBytes,
                      WifiStandard standard,
                      uint8_t mcs,
                      MHz_u width,
                      uint16_t guardIntervalNs,
                      uint8_t nss,
                      Band band,
                      bool ldpc = true);

/**
 * Duration of a control PPDU (ACK/BlockAck/CTS) sent with a given mode.
 *
 * @param bytes the frame size
 * @param modeName the WifiMode name (e.g. "OfdmRate24Mbps", "HeMcs0")
 * @param band the band
 * @return the duration in microseconds
 */
double ControlPpduDurationUs(uint32_t bytes, const std::string& modeName, Band band);

/**
 * Preamble and PHY header duration of an HE/EHT single-user PPDU.
 *
 * @param standard the standard
 * @param mcs the MCS index
 * @param width the channel width
 * @param guardIntervalNs the guard interval
 * @param nss the number of spatial streams
 * @return the duration in microseconds
 */
double PreambleDurationUs(WifiStandard standard,
                          uint8_t mcs,
                          MHz_u width,
                          uint16_t guardIntervalNs,
                          uint8_t nss);

/**
 * Number of MPDUs per A-MPDU under saturation, given the Block Ack window, the maximum A-MPDU
 * size and the maximum PPDU duration (or the TXOP limit).
 *
 * @param mpduBytes the MPDU size
 * @param baWindow the Block Ack window
 * @param maxAmpduBytes the maximum A-MPDU size
 * @param maxPpduUs the maximum PPDU duration
 * @param standard the standard
 * @param mcs the MCS index
 * @param width the channel width
 * @param guardIntervalNs the guard interval
 * @param nss the number of spatial streams
 * @param band the band
 * @return the number of MPDUs and the resulting PPDU duration in microseconds
 */
std::pair<uint32_t, double> SaturatedAmpdu(uint32_t mpduBytes,
                                           uint16_t baWindow,
                                           uint32_t maxAmpduBytes,
                                           double maxPpduUs,
                                           WifiStandard standard,
                                           uint8_t mcs,
                                           MHz_u width,
                                           uint16_t guardIntervalNs,
                                           uint8_t nss,
                                           Band band);

/**
 * @ingroup wifi-benchmarks
 * Energy consumption monitor based on WifiRadioEnergyModel.
 */
struct EnergyMonitor
{
    std::vector<Ptr<energy::DeviceEnergyModel>> models; ///< energy models (one per device)
    energy::EnergySourceContainer sources;              ///< energy sources

    /**
     * @param i the device index
     * @return the energy consumed by the device in joules
     */
    double GetConsumedJoules(uint32_t i) const;
};

/**
 * Install energy sources and radio energy models on the devices (single-link devices only).
 *
 * @param devices the devices
 * @param idleA idle current
 * @param txA transmit current
 * @param rxA receive current
 * @param sleepA sleep current
 * @param ccaBusyA CCA busy current
 * @return the monitor
 */
EnergyMonitor InstallEnergy(const NetDeviceContainer& devices,
                            double idleA = 0.273,
                            double txA = 0.380,
                            double rxA = 0.313,
                            double sleepA = 0.033,
                            double ccaBusyA = 0.273);

/**
 * Fraction of time a PHY link spent in a set of states, from a WifiCoTraceHelper.
 *
 * @param helper the channel occupancy trace helper
 * @param nodeId the node ID
 * @param deviceId the device ID
 * @param linkId the link ID
 * @param states the states to sum
 * @return the fraction of the measurement duration
 */
double StateFraction(const WifiCoTraceHelper& helper,
                     uint32_t nodeId,
                     uint32_t deviceId,
                     uint8_t linkId,
                     const std::set<WifiPhyState>& states);

/**
 * @ingroup wifi-benchmarks
 * Accounting of the PPDUs transmitted by a set of devices (PhyTxPsduBegin trace).
 */
class TxAccounting
{
  public:
    /// Record of one PPDU
    struct PpduRecord
    {
        Time time;                          ///< transmission start
        uint32_t nodeId;                    ///< transmitter node
        uint8_t linkId;                     ///< link ID
        uint32_t psduBytes;                 ///< total PSDU bytes
        uint32_t nMpdus;                    ///< number of MPDUs (all users)
        uint32_t nUsers;                    ///< number of PSDUs (users) in the PPDU
        double durationUs;                  ///< PPDU duration
        std::string preamble;               ///< preamble type
        std::string mode;                   ///< data mode of the first user
        double widthMhz;                    ///< channel width
        bool mu{false};                     ///< whether the PPDU is a MU (OFDMA) PPDU
        RuType ruType{RuType::RU_TYPE_MAX}; ///< RU type of the first user (MU PPDUs)
    };

    /**
     * Hook the PHYs of a device.
     * @param device the device
     */
    void Enable(Ptr<WifiNetDevice> device);
    /**
     * Hook the PHYs of the devices of a container.
     * @param devices the devices
     */
    void Enable(const NetDeviceContainer& devices);
    /**
     * Set the accounting window.
     * @param start the start time
     * @param stop the stop time
     */
    void SetWindow(Time start, Time stop);
    /// @return the records
    const std::vector<PpduRecord>& GetRecords() const;
    /**
     * @param preambleFilter if not empty, only count PPDUs with this preamble (e.g. "HE_MU")
     * @return the number of PPDUs
     */
    uint64_t GetNPpdus(const std::string& preambleFilter = "") const;
    /// @return the mean number of MPDUs per PPDU
    double GetMeanMpdusPerPpdu() const;
    /// @return the mean PPDU duration in microseconds
    double GetMeanDurationUs() const;
    /// @return the total transmission airtime in microseconds
    double GetTotalAirtimeUs() const;
    /// @return a one-line summary
    std::string Summary() const;
    /**
     * @param preamble the preamble filter (e.g. "HE_MU", "HE_TB")
     * @return the most frequent RU type of the MU PPDUs with that preamble
     */
    RuType GetDominantRuType(const std::string& preamble) const;
    /**
     * @param preamble the preamble filter (e.g. "HE_SU")
     * @return the most frequent data mode name of the PPDUs with that preamble
     */
    std::string GetDominantMode(const std::string& preamble) const;

    /**
     * Trace callback.
     * @param nodeId the node ID
     * @param linkId the link ID
     * @param band the PHY band
     * @param psduMap the PSDU map
     * @param txVector the TX vector
     * @param txPower the TX power
     */
    void Notify(uint32_t nodeId,
                uint8_t linkId,
                WifiPhyBand band,
                WifiConstPsduMap psduMap,
                WifiTxVector txVector,
                Watt_u txPower);

  private:
    std::vector<PpduRecord> m_records; ///< records
    Time m_start{Seconds(0)};          ///< window start
    Time m_stop{Time::Max()};          ///< window end
};

/**
 * @param standard the standard
 * @return a printable name ("802.11ax (Wi-Fi 6)" or "802.11be (Wi-Fi 7)")
 */
std::string StandardName(WifiStandard standard);

/**
 * @param name "ax", "be", "11ax", "11be", "wifi6" or "wifi7"
 * @return the standard
 */
WifiStandard ParseStandard(const std::string& name);

/**
 * @param standard the standard
 * @return the maximum MCS index of the standard (11 for HE, 13 for EHT)
 */
uint8_t MaxMcs(WifiStandard standard);

} // namespace wifibench
} // namespace ns3

#endif /* WIFI_BENCHMARK_HELPER_H */
