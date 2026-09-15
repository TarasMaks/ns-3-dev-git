/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef WIFI_LITERATURE_REFERENCE_H
#define WIFI_LITERATURE_REFERENCE_H

#include "ns3/nstime.h"
#include "ns3/qos-utils.h"
#include "ns3/wifi-phy-band.h"
#include "ns3/wifi-standards.h"
#include "ns3/wifi-types.h"
#include "ns3/wifi-units.h"

#include <cstdint>
#include <string>
#include <vector>

/**
 * @defgroup wifi-benchmarks Wi-Fi 6/7/8 benchmark suite
 *
 * Experimental simulation scripts that exercise the IEEE 802.11ax (Wi-Fi 6), 802.11be (Wi-Fi 7)
 * and 802.11bn (Wi-Fi 8, Ultra High Reliability) capabilities modeled by ns-3 and compare the
 * results with figures published in the standards and in the research literature.
 */

namespace ns3
{
namespace wifibench
{

/**
 * @ingroup wifi-benchmarks
 *
 * Modulation and coding parameters of an HE/EHT MCS index (IEEE Std 802.11ax-2021 Clause 27.5,
 * IEEE Std 802.11be-2024 Clause 36.5).
 */
struct McsParams
{
    uint8_t bitsPerSubcarrier; ///< N_BPSCS: coded bits per subcarrier per spatial stream
    double codeRate;           ///< R: coding rate
    std::string modulation;    ///< Modulation name (BPSK, QPSK, ..., 4096-QAM)
};

/**
 * @param mcs the HE/EHT MCS index (0-13)
 * @return the modulation and coding parameters of the MCS
 */
McsParams GetMcsParams(uint8_t mcs);

/**
 * Number of data subcarriers (N_SD) of an OFDMA resource unit, per the HE/EHT MCS tables.
 *
 * @param ruType the resource unit type
 * @return the number of data subcarriers
 */
uint16_t GetDataSubcarriers(RuType ruType);

/**
 * Number of data subcarriers (N_SD) of a full-bandwidth HE/EHT PPDU.
 *
 * @param width the channel width (20, 40, 80, 160 or 320 MHz)
 * @return the number of data subcarriers
 */
uint16_t GetDataSubcarriers(MHz_u width);

/**
 * OFDM symbol duration of HE/EHT PPDUs: 12.8 us of useful symbol plus the guard interval.
 *
 * @param guardIntervalNs the guard interval in nanoseconds (800, 1600 or 3200)
 * @return the symbol duration in microseconds
 */
double GetSymbolDurationUs(uint16_t guardIntervalNs);

/**
 * PHY data rate computed with the formula of the standard MCS tables:
 * R = N_SD * N_BPSCS * R_code * N_SS / T_SYM.
 *
 * This is an implementation independent from the ns-3 wifi module and is used as the reference
 * against which HePhy::GetDataRate() and EhtPhy::GetDataRate() are compared.
 *
 * @param mcs the HE/EHT MCS index
 * @param width the channel width in MHz
 * @param guardIntervalNs the guard interval in nanoseconds
 * @param nss the number of spatial streams
 * @return the data rate in Mb/s
 */
double GetStandardDataRateMbps(uint8_t mcs, MHz_u width, uint16_t guardIntervalNs, uint8_t nss);

/**
 * PHY data rate of a single-user transmission in an OFDMA resource unit.
 *
 * @param mcs the HE/EHT MCS index
 * @param ruType the resource unit type
 * @param guardIntervalNs the guard interval in nanoseconds
 * @param nss the number of spatial streams
 * @return the data rate in Mb/s
 */
double GetStandardRuDataRateMbps(uint8_t mcs, RuType ruType, uint16_t guardIntervalNs, uint8_t nss);

/**
 * A data rate published in the standard tables, used as a spot check of the formula.
 */
struct PublishedRate
{
    WifiStandard standard;    ///< standard (802.11ax or 802.11be)
    uint8_t mcs;              ///< MCS index
    MHz_u width;              ///< channel width
    uint16_t guardIntervalNs; ///< guard interval in nanoseconds
    uint8_t nss;              ///< number of spatial streams
    double rateMbps;          ///< published data rate in Mb/s
    std::string source;       ///< citation
};

/**
 * @return data rates published in IEEE Std 802.11ax-2021 (Clause 27.5) and IEEE Std
 *         802.11be-2024 (Clause 36.5) MCS tables, plus the peak rates quoted in the survey
 *         literature (Khorov et al. 2019, Lopez-Perez et al. 2019, Deng et al. 2020).
 */
std::vector<PublishedRate> GetPublishedRates();

/**
 * Receiver minimum input level sensitivity of the standard (IEEE Std 802.11ax-2021 Table 27-51
 * for HE-MCS 0-11, IEEE Std 802.11be-2024 Clause 36 for EHT-MCS 12-13), measured for a PSDU of
 * 4096 octets at a PER of 10%. The 20 MHz values increase by 3 dB per doubling of the width.
 *
 * @param mcs the MCS index (0-13)
 * @param width the channel width in MHz
 * @return the minimum sensitivity in dBm
 */
double GetMinSensitivityDbm(uint8_t mcs, MHz_u width);

/**
 * Thermal noise power in a given bandwidth: -174 dBm/Hz + 10 log10(B) + NF.
 *
 * @param width the bandwidth in MHz
 * @param noiseFigureDb the receiver noise figure in dB
 * @return the noise power in dBm
 */
double GetNoisePowerDbm(MHz_u width, double noiseFigureDb);

/// Noise figure assumed by the sensitivity tables of the standard (Clause 17.3.10.1 note)
constexpr double STANDARD_NOISE_FIGURE_DB = 10.0;
/// Implementation margin assumed by the sensitivity tables of the standard
constexpr double STANDARD_IMPLEMENTATION_MARGIN_DB = 5.0;
/// PSDU length (octets) used by the standard to define the sensitivity (PER = 10%)
constexpr uint32_t STANDARD_SENSITIVITY_PSDU_BYTES = 4096;
/// PER at which the standard defines the receiver sensitivity
constexpr double STANDARD_SENSITIVITY_PER = 0.1;

/**
 * SNR implied by the sensitivity table of the standard: sensitivity minus the noise power
 * (with the 10 dB noise figure of the standard) minus the 5 dB implementation margin.
 *
 * @param mcs the MCS index
 * @return the SNR in dB at which an ideal receiver reaches 10% PER with 4096-byte PSDUs
 */
double GetStandardRequiredSnrDb(uint8_t mcs);

/// Peak PHY rate of 802.11ax: 160 MHz, HE-MCS 11, 8 spatial streams, 0.8 us GI (Mb/s)
constexpr double HE_PEAK_RATE_MBPS = 9607.8;
/// Peak PHY rate of 802.11be with 8 spatial streams: 320 MHz, EHT-MCS 13, 0.8 us GI (Mb/s)
constexpr double EHT_PEAK_RATE_8SS_MBPS = 23059.2;
/// Peak PHY rate of 802.11be with 16 spatial streams (the "46 Gb/s" figure of Wi-Fi 7 papers)
constexpr double EHT_PEAK_RATE_16SS_MBPS = 46118.4;
/// Rate gain of 4096-QAM 5/6 (EHT-MCS 13) over 1024-QAM 5/6 (HE-MCS 11): 12/10
constexpr double EHT_4096QAM_GAIN = 1.2;
/// Maximum PPDU duration (us) for HE/EHT PPDUs
constexpr double MAX_PPDU_DURATION_US = 5484;
/// Maximum A-MPDU length (octets) of an HE STA
constexpr uint32_t HE_MAX_AMPDU_BYTES = 6500631;
/// Maximum A-MPDU length (octets) of an EHT STA
constexpr uint32_t EHT_MAX_AMPDU_BYTES = 15523200;
/// Maximum MPDU length (octets) of HE/EHT STAs
constexpr uint32_t HE_MAX_MPDU_BYTES = 11454;
/// Block Ack window (MPDUs) of HE (extended block ack) and EHT
constexpr uint16_t HE_MAX_BA_WINDOW = 256;
constexpr uint16_t EHT_MAX_BA_WINDOW = 1024;

/**
 * Objectives of the IEEE 802.11bn (UHR, Wi-Fi 8) Project Authorization Request, relative to
 * an 802.11be baseline (see Galati-Giordano et al., "What Will Wi-Fi 8 Be? A Primer on IEEE
 * 802.11bn Ultra High Reliability", IEEE Communications Magazine, 2024).
 */
/// At least 25% improvement of the throughput at the 5th percentile
constexpr double UHR_5TH_PERCENTILE_THROUGHPUT_GAIN = 0.25;
/// At least 25% reduction of the latency at the 95th percentile
constexpr double UHR_P95_LATENCY_REDUCTION = 0.25;
/// At least 25% reduction of the MPDU loss due to mobility (BSS transitions)
constexpr double UHR_MOBILITY_LOSS_REDUCTION = 0.25;

/**
 * EDCA parameters of the standard (IEEE Std 802.11-2020 Table 9-155, non-DSSS PHYs).
 */
struct EdcaParams
{
    uint8_t aifsn;      ///< AIFSN
    uint32_t cwMin;     ///< CWmin
    uint32_t cwMax;     ///< CWmax
    double txopLimitUs; ///< TXOP limit in microseconds (0 = single PPDU exchange)
};

/**
 * @param ac the access category
 * @return the default EDCA parameters of the standard for the AC
 */
EdcaParams GetStandardEdcaParams(AcIndex ac);

/// Timing constants of OFDM PHYs in 5/6 GHz (and 2.4 GHz HT/HE without DSSS coexistence)
constexpr double SIFS_US = 16;
constexpr double SLOT_US = 9;
constexpr double DIFS_US = SIFS_US + 2 * SLOT_US;

/**
 * Overheads (bytes) of a QoS Data MPDU carrying a UDP/IPv4 datagram.
 */
constexpr uint32_t QOS_MAC_HEADER_BYTES = 26;
constexpr uint32_t LLC_SNAP_BYTES = 8;
constexpr uint32_t IPV4_HEADER_BYTES = 20;
constexpr uint32_t UDP_HEADER_BYTES = 8;
constexpr uint32_t FCS_BYTES = 4;
constexpr uint32_t AMPDU_DELIMITER_BYTES = 4;

/**
 * @param udpPayloadBytes the UDP payload size
 * @return the MPDU size in bytes (MAC header + LLC/SNAP + IP + UDP + payload + FCS)
 */
uint32_t GetMpduBytes(uint32_t udpPayloadBytes);

/**
 * @param mpduBytes the MPDU size
 * @return the A-MPDU subframe size in bytes (delimiter + MPDU + padding to 4 bytes)
 */
uint32_t GetAmpduSubframeBytes(uint32_t mpduBytes);

/**
 * @param baWindow the block ack window size (64, 256 or 1024)
 * @return the size in bytes of a compressed BlockAck frame with that bitmap length
 */
uint32_t GetBlockAckBytes(uint16_t baWindow);

/**
 * Timing inputs of the analytical MAC models, in microseconds. The PPDU durations are expected
 * to be computed with WifiPhy::CalculateTxDuration() so that the model shares the PHY timing
 * assumptions (preamble format, padding) of the simulator.
 */
struct MacModelTiming
{
    double aifsUs{DIFS_US};       ///< AIFS (DIFS for DCF)
    double slotUs{SLOT_US};       ///< slot time
    double sifsUs{SIFS_US};       ///< SIFS
    uint32_t cwMin{15};           ///< CWmin
    uint32_t cwMax{1023};         ///< CWmax
    double dataPpduUs{0};         ///< duration of the data PPDU (A-MPDU of k MPDUs)
    double ackPpduUs{0};          ///< duration of the ACK/BlockAck PPDU
    double propagationDelayUs{0}; ///< propagation delay
};

/**
 * Maximum goodput of a single transmitter without contention nor errors: in each cycle the
 * transmitter waits AIFS plus the mean post-transmission backoff (CWmin/2 slots), transmits an
 * A-MPDU of k MPDUs and receives a BlockAck after SIFS.
 *
 * @param timing the timing inputs
 * @param kMpdus the number of MPDUs per A-MPDU
 * @param payloadBytes the application payload per MPDU
 * @return the goodput in Mb/s
 */
double ComputeSingleUserGoodputMbps(const MacModelTiming& timing,
                                    uint32_t kMpdus,
                                    uint32_t payloadBytes);

/**
 * Saturation throughput of n contending stations according to the Bianchi model
 * (G. Bianchi, "Performance Analysis of the IEEE 802.11 Distributed Coordination Function",
 * IEEE JSAC, vol. 18, no. 3, 2000), extended with frame aggregation as in the ns-3
 * reference script src/wifi/examples/reference/bianchi11ax.py.
 *
 * @param nStations the number of saturated stations
 * @param timing the timing inputs (data PPDU carrying k MPDUs, ACK/BA PPDU)
 * @param kMpdus the number of MPDUs per A-MPDU
 * @param payloadBytes the application payload per MPDU
 * @param eifs whether a collision is followed by EIFS (true, as in ns-3) or DIFS
 * @return the saturation throughput in Mb/s (payload bits delivered per second)
 */
double ComputeBianchiThroughputMbps(uint32_t nStations,
                                    const MacModelTiming& timing,
                                    uint32_t kMpdus,
                                    uint32_t payloadBytes,
                                    bool eifs);

/**
 * Solve the Bianchi fixed point for the per-slot transmission probability tau.
 *
 * @param nStations the number of stations
 * @param cwMin CWmin
 * @param cwMax CWmax
 * @return the transmission probability tau
 */
double SolveBianchiTau(uint32_t nStations, uint32_t cwMin, uint32_t cwMax);

/**
 * A citation used in the tables written by the benchmark scripts.
 */
struct Citation
{
    std::string key;  ///< short key used in the CSV files (e.g., "ieee80211ax")
    std::string text; ///< full reference
};

/**
 * @return the list of references cited by the suite
 */
const std::vector<Citation>& GetCitations();

} // namespace wifibench
} // namespace ns3

#endif /* WIFI_LITERATURE_REFERENCE_H */
