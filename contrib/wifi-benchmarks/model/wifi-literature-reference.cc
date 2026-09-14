/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "wifi-literature-reference.h"

#include "ns3/abort.h"

#include <cmath>
#include <map>

namespace ns3
{
namespace wifibench
{

McsParams
GetMcsParams(uint8_t mcs)
{
    static const std::vector<McsParams> table{
        {1, 1.0 / 2, "BPSK"},
        {2, 1.0 / 2, "QPSK"},
        {2, 3.0 / 4, "QPSK"},
        {4, 1.0 / 2, "16-QAM"},
        {4, 3.0 / 4, "16-QAM"},
        {6, 2.0 / 3, "64-QAM"},
        {6, 3.0 / 4, "64-QAM"},
        {6, 5.0 / 6, "64-QAM"},
        {8, 3.0 / 4, "256-QAM"},
        {8, 5.0 / 6, "256-QAM"},
        {10, 3.0 / 4, "1024-QAM"},
        {10, 5.0 / 6, "1024-QAM"},
        {12, 3.0 / 4, "4096-QAM"},
        {12, 5.0 / 6, "4096-QAM"},
    };
    NS_ABORT_MSG_IF(mcs >= table.size(), "Invalid MCS " << +mcs);
    return table.at(mcs);
}

uint16_t
GetDataSubcarriers(RuType ruType)
{
    switch (ruType)
    {
    case RuType::RU_26_TONE:
        return 24;
    case RuType::RU_52_TONE:
        return 48;
    case RuType::RU_106_TONE:
        return 102;
    case RuType::RU_242_TONE:
        return 234;
    case RuType::RU_484_TONE:
        return 468;
    case RuType::RU_996_TONE:
        return 980;
    case RuType::RU_2x996_TONE:
        return 1960;
    case RuType::RU_4x996_TONE:
        return 3920;
    default:
        NS_ABORT_MSG("Unsupported RU type");
    }
    return 0;
}

uint16_t
GetDataSubcarriers(MHz_u width)
{
    if (width == MHz_u{20})
    {
        return 234;
    }
    if (width == MHz_u{40})
    {
        return 468;
    }
    if (width == MHz_u{80})
    {
        return 980;
    }
    if (width == MHz_u{160})
    {
        return 1960;
    }
    if (width == MHz_u{320})
    {
        return 3920;
    }
    NS_ABORT_MSG("Unsupported channel width " << width);
    return 0;
}

double
GetSymbolDurationUs(uint16_t guardIntervalNs)
{
    NS_ABORT_MSG_IF(guardIntervalNs != 800 && guardIntervalNs != 1600 && guardIntervalNs != 3200,
                    "Invalid guard interval " << guardIntervalNs);
    return 12.8 + guardIntervalNs / 1000.0;
}

double
GetStandardDataRateMbps(uint8_t mcs, MHz_u width, uint16_t guardIntervalNs, uint8_t nss)
{
    const auto params = GetMcsParams(mcs);
    const auto nsd = GetDataSubcarriers(width);
    return nsd * params.bitsPerSubcarrier * params.codeRate * nss /
           GetSymbolDurationUs(guardIntervalNs);
}

double
GetStandardRuDataRateMbps(uint8_t mcs, RuType ruType, uint16_t guardIntervalNs, uint8_t nss)
{
    const auto params = GetMcsParams(mcs);
    const auto nsd = GetDataSubcarriers(ruType);
    return nsd * params.bitsPerSubcarrier * params.codeRate * nss /
           GetSymbolDurationUs(guardIntervalNs);
}

std::vector<PublishedRate>
GetPublishedRates()
{
    const std::string ax = "ieee80211ax";
    const std::string be = "ieee80211be";
    return {
        {WIFI_STANDARD_80211ax, 0, MHz_u{20}, 800, 1, 8.6, ax},
        {WIFI_STANDARD_80211ax, 7, MHz_u{20}, 800, 1, 86.0, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{20}, 800, 1, 143.4, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{20}, 3200, 1, 121.9, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{40}, 800, 1, 286.8, ax},
        {WIFI_STANDARD_80211ax, 9, MHz_u{80}, 800, 1, 480.4, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{80}, 800, 1, 600.5, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{160}, 800, 1, 1201.0, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{160}, 800, 2, 2402.0, ax},
        {WIFI_STANDARD_80211ax, 11, MHz_u{160}, 800, 8, HE_PEAK_RATE_MBPS, "khorov2019"},
        {WIFI_STANDARD_80211be, 13, MHz_u{20}, 800, 1, 172.1, be},
        {WIFI_STANDARD_80211be, 11, MHz_u{320}, 800, 1, 2402.0, be},
        {WIFI_STANDARD_80211be, 12, MHz_u{320}, 800, 1, 2594.1, be},
        {WIFI_STANDARD_80211be, 13, MHz_u{320}, 800, 1, 2882.4, be},
        {WIFI_STANDARD_80211be, 13, MHz_u{160}, 800, 1, 1441.2, be},
        {WIFI_STANDARD_80211be, 13, MHz_u{320}, 800, 8, EHT_PEAK_RATE_8SS_MBPS, "deng2020"},
    };
}

double
GetMinSensitivityDbm(uint8_t mcs, MHz_u width)
{
    // 20 MHz values; HE-MCS 0-11 from IEEE Std 802.11ax-2021 Table 27-51, EHT-MCS 12-13 from
    // IEEE Std 802.11be-2024 Clause 36
    static const std::vector<double>
        sens20{-82, -79, -77, -74, -70, -66, -65, -64, -59, -57, -54, -52, -49, -47};
    NS_ABORT_MSG_IF(mcs >= sens20.size(), "Invalid MCS " << +mcs);
    const double widthFactor = std::log2(width / MHz_u{20});
    return sens20.at(mcs) + 3 * widthFactor;
}

double
GetNoisePowerDbm(MHz_u width, double noiseFigureDb)
{
    return -174 + 10 * std::log10(width * 1e6) + noiseFigureDb;
}

double
GetStandardRequiredSnrDb(uint8_t mcs)
{
    return GetMinSensitivityDbm(mcs, MHz_u{20}) -
           GetNoisePowerDbm(MHz_u{20}, STANDARD_NOISE_FIGURE_DB) -
           STANDARD_IMPLEMENTATION_MARGIN_DB;
}

EdcaParams
GetStandardEdcaParams(AcIndex ac)
{
    switch (ac)
    {
    case AC_BK:
        return {7, 15, 1023, 2528};
    case AC_BE:
        return {3, 15, 1023, 2528};
    case AC_VI:
        return {2, 7, 15, 4096};
    case AC_VO:
        return {2, 3, 7, 2080};
    default:
        return {2, 15, 1023, 0};
    }
}

uint32_t
GetMpduBytes(uint32_t udpPayloadBytes)
{
    return QOS_MAC_HEADER_BYTES + LLC_SNAP_BYTES + IPV4_HEADER_BYTES + UDP_HEADER_BYTES +
           udpPayloadBytes + FCS_BYTES;
}

uint32_t
GetAmpduSubframeBytes(uint32_t mpduBytes)
{
    const uint32_t padded = ((mpduBytes + 3) / 4) * 4;
    return AMPDU_DELIMITER_BYTES + padded;
}

uint32_t
GetBlockAckBytes(uint16_t baWindow)
{
    // FC(2) + Duration(2) + RA(6) + TA(6) + BA Control(2) + SSC(2) + bitmap + FCS(4)
    return 24 + baWindow / 8;
}

double
ComputeSingleUserGoodputMbps(const MacModelTiming& timing, uint32_t kMpdus, uint32_t payloadBytes)
{
    const double meanBackoffUs = timing.cwMin / 2.0 * timing.slotUs;
    const double cycleUs = timing.aifsUs + meanBackoffUs + timing.dataPpduUs + timing.sifsUs +
                           timing.ackPpduUs + 2 * timing.propagationDelayUs;
    return kMpdus * payloadBytes * 8.0 / cycleUs;
}

double
SolveBianchiTau(uint32_t nStations, uint32_t cwMin, uint32_t cwMax)
{
    const double w = cwMin + 1;
    const int m = static_cast<int>(std::round(std::log2((cwMax + 1.0) / (cwMin + 1.0))));
    auto f = [&](double tau) {
        const double p = 1 - std::pow(1 - tau, static_cast<int>(nStations) - 1);
        double ps = 0;
        for (int i = 0; i < m; ++i)
        {
            ps += std::pow(2 * p, i);
        }
        return tau - 2.0 / (1 + w + p * w * ps);
    };
    // f is increasing in tau on (0, 1): bisection
    double lo = 1e-9;
    double hi = 1.0 - 1e-9;
    for (int it = 0; it < 200; ++it)
    {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) > 0)
        {
            hi = mid;
        }
        else
        {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

double
ComputeBianchiThroughputMbps(uint32_t nStations,
                             const MacModelTiming& timing,
                             uint32_t kMpdus,
                             uint32_t payloadBytes,
                             bool eifs)
{
    const double delta = timing.propagationDelayUs;
    double ts;
    double tc;
    if (!eifs)
    {
        ts = timing.dataPpduUs + timing.sifsUs + timing.ackPpduUs + timing.aifsUs;
        tc = timing.dataPpduUs + timing.aifsUs;
    }
    else
    {
        ts = timing.dataPpduUs + timing.sifsUs + timing.ackPpduUs + timing.aifsUs + delta;
        tc = timing.dataPpduUs + timing.aifsUs + timing.sifsUs + timing.ackPpduUs + delta;
    }
    // the ns-3 reference script adds one slot to the successful transmission time
    const double tS = ts + timing.slotUs;

    const double tau = SolveBianchiTau(nStations, timing.cwMin, timing.cwMax);
    const double ptr = 1 - std::pow(1 - tau, static_cast<int>(nStations));
    const double psucc = nStations * tau * std::pow(1 - tau, static_cast<int>(nStations) - 1) / ptr;
    const double payloadBits = payloadBytes * 8.0;
    const double denom = (1 - ptr) * timing.slotUs + ptr * psucc * tS + ptr * (1 - psucc) * tc;
    return kMpdus * psucc * ptr * payloadBits / denom;
}

const std::vector<Citation>&
GetCitations()
{
    static const std::vector<Citation> citations{
        {"ieee80211-2020",
         "IEEE Std 802.11-2020, IEEE Standard for Information Technology - Telecommunications and "
         "Information Exchange between Systems - Local and Metropolitan Area Networks - Specific "
         "Requirements - Part 11: Wireless LAN MAC and PHY Specifications, 2021."},
        {"ieee80211ax",
         "IEEE Std 802.11ax-2021, Amendment 1: Enhancements for High-Efficiency WLAN, 2021 "
         "(Clause 27.5 HE-MCS tables, Clause 27.3.19 receiver minimum input sensitivity)."},
        {"ieee80211be",
         "IEEE Std 802.11be-2024, Amendment 8: Enhancements for Extremely High Throughput (EHT), "
         "2024 (Clause 36.5 EHT-MCS tables, Clause 35.3 multi-link operation, EMLSR, TID-to-link "
         "mapping)."},
        {"ieee80211bn-par",
         "IEEE 802.11 Working Group, Project Authorization Request P802.11bn (Ultra High "
         "Reliability), 2023: +25% throughput at the 5th percentile, -25% latency at the 95th "
         "percentile, -25% MPDU loss due to mobility, relative to an 802.11be baseline."},
        {"khorov2019",
         "E. Khorov, A. Kiryanov, A. Lyakhov and G. Bianchi, \"A Tutorial on IEEE 802.11ax High "
         "Efficiency WLANs,\" IEEE Communications Surveys & Tutorials, vol. 21, no. 1, pp. "
         "197-216, 2019."},
        {"lopezperez2019",
         "D. Lopez-Perez, A. Garcia-Rodriguez, L. Galati-Giordano, M. Kasslin and K. Doppler, "
         "\"IEEE 802.11be Extremely High Throughput: The Next Generation of Wi-Fi Technology "
         "Beyond 802.11ax,\" IEEE Communications Magazine, vol. 57, no. 9, pp. 113-119, 2019."},
        {"deng2020",
         "C. Deng et al., \"IEEE 802.11be Wi-Fi 7: New Challenges and Opportunities,\" IEEE "
         "Communications Surveys & Tutorials, vol. 22, no. 4, pp. 2136-2166, 2020."},
        {"khorov2020",
         "E. Khorov, I. Levitsky and I. F. Akyildiz, \"Current Status and Directions of IEEE "
         "802.11be, the Future Wi-Fi 7,\" IEEE Access, vol. 8, pp. 88664-88688, 2020."},
        {"chen2022",
         "C. Chen, X. Chen, D. Das, D. Akhmetov and C. Cordeiro, \"Overview and Performance "
         "Evaluation of Wi-Fi 7,\" IEEE Communications Standards Magazine, vol. 6, no. 2, pp. "
         "12-18, 2022."},
        {"lopezraventos2022",
         "A. Lopez-Raventos and B. Bellalta, \"Multi-link Operation in IEEE 802.11be WLANs,\" "
         "IEEE Wireless Communications, vol. 29, no. 4, pp. 94-100, 2022."},
        {"carrascosa2023",
         "M. Carrascosa-Zamacois, G. Geraci, L. Galati-Giordano, A. Jonsson and B. Bellalta, "
         "\"Understanding Multi-link Operation in Wi-Fi 7: Performance, Anomalies, and "
         "Solutions,\" Proc. IEEE PIMRC, 2023."},
        {"galati2024",
         "L. Galati-Giordano, G. Geraci, M. Carrascosa and B. Bellalta, \"What Will Wi-Fi 8 Be? "
         "A Primer on IEEE 802.11bn Ultra High Reliability,\" IEEE Communications Magazine, "
         "vol. 62, no. 8, 2024."},
        {"reshef2022",
         "E. Reshef and C. Cordeiro, \"Future Directions for Wi-Fi 8 and Beyond,\" IEEE "
         "Communications Magazine, vol. 60, no. 10, pp. 50-55, 2022."},
        {"bianchi2000",
         "G. Bianchi, \"Performance Analysis of the IEEE 802.11 Distributed Coordination "
         "Function,\" IEEE Journal on Selected Areas in Communications, vol. 18, no. 3, pp. "
         "535-547, 2000."},
        {"patidar2017",
         "R. Patidar, S. Roy, T. R. Henderson and A. Chandramohan, \"Link-to-System Mapping for "
         "ns-3 Wi-Fi OFDM Error Models,\" Proc. Workshop on ns-3 (WNS3), 2017."},
        {"wilhelmi2021",
         "F. Wilhelmi, S. Barrachina-Munoz, B. Bellalta, C. Cano, A. Jonsson and V. Ram, "
         "\"Spatial Reuse in IEEE 802.11ax WLANs,\" Computer Communications, vol. 170, pp. "
         "65-83, 2021."},
        {"nurchis2019",
         "M. Nurchis and B. Bellalta, \"Target Wake Time: Scheduled Access in IEEE 802.11ax "
         "WLANs,\" IEEE Wireless Communications, vol. 26, no. 3, pp. 142-150, 2019."},
        {"nunez2022",
         "D. Nunez, F. Wilhelmi, S. Avallone, M. Smith and B. Bellalta, \"TXOP Sharing with "
         "Coordinated Spatial Reuse in Multi-AP Cooperative IEEE 802.11be WLANs,\" Proc. IEEE "
         "CCNC, 2022."},
        {"tgax-scenarios",
         "IEEE 802.11-14/0980r16, \"TGax Simulation Scenarios,\" and IEEE 802.11-14/0571r12, "
         "\"11ax Evaluation Methodology\" (traffic models and dense deployment scenarios)."},
        {"ns3-wifi",
         "ns-3 wifi module documentation, Model Library, \"Wi-Fi Module\" (design, validation and "
         "scope/limitations sections), https://www.nsnam.org/docs/models/html/wifi.html."},
    };
    return citations;
}

} // namespace wifibench
} // namespace ns3
