/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 * @ingroup wifi-benchmarks
 *
 * Link-level PER vs SNR of the ns-3 error models for every HE (802.11ax) or EHT (802.11be) MCS,
 * and comparison of the SNR required for a PER of 10% with 4096-byte PSDUs with the receiver
 * minimum input sensitivity of the standard (IEEE Std 802.11ax-2021 Table 27-51 for HE-MCS 0-11,
 * IEEE Std 802.11be-2024 Clause 36 for EHT-MCS 12-13), converted to SNR with the noise figure
 * (10 dB) and implementation margin (5 dB) assumed by the standard.
 *
 * The table-based model (default in ns-3) is derived from link simulations (Patidar et al. 2017)
 * and is available for LDPC up to MCS 11 and for BCC up to MCS 9; for higher MCSs it falls back
 * to the YANS model. The PASS/FAIL verdict is given for the table-based models only (the NIST
 * and YANS analytical models are reported for information). The PER curves are written to
 * <outputDir>/<name>-curves.csv.
 *
 * Example: ./ns3 run "wifi-bench-per-vs-snr --standard=be"
 */

#include "ns3/command-line.h"
#include "ns3/nist-error-rate-model.h"
#include "ns3/table-based-error-rate-model.h"
#include "ns3/wifi-benchmark-helper.h"
#include "ns3/wifi-literature-reference.h"
#include "ns3/wifi-tx-vector.h"
#include "ns3/yans-error-rate-model.h"

#include <cmath>
#include <fstream>

using namespace ns3;
using namespace ns3::wifibench;

/**
 * @param model the error model
 * @param mode the mode
 * @param txVector the TX vector
 * @param snrDb the SNR in dB
 * @param psduBytes the PSDU size
 * @return the PER
 */
double
Per(Ptr<ErrorRateModel> model,
    const WifiMode& mode,
    const WifiTxVector& txVector,
    double snrDb,
    uint32_t psduBytes)
{
    const double snr = std::pow(10.0, snrDb / 10.0);
    return 1.0 - model->GetChunkSuccessRate(mode, txVector, snr, psduBytes * 8);
}

/**
 * Find by bisection the SNR at which the PER equals a target.
 * @param model the error model
 * @param mode the mode
 * @param txVector the TX vector
 * @param psduBytes the PSDU size
 * @param targetPer the target PER
 * @return the SNR in dB
 */
double
SnrAtPer(Ptr<ErrorRateModel> model,
         const WifiMode& mode,
         const WifiTxVector& txVector,
         uint32_t psduBytes,
         double targetPer)
{
    double lo = -10;
    double hi = 70;
    for (int i = 0; i < 60; ++i)
    {
        const double mid = 0.5 * (lo + hi);
        if (Per(model, mode, txVector, mid, psduBytes) > targetPer)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    return 0.5 * (lo + hi);
}

int
main(int argc, char* argv[])
{
    CommonArgs common;
    std::string standardStr{"ax"};
    uint32_t psduBytes{STANDARD_SENSITIVITY_PSDU_BYTES};
    double toleranceDb{3.0};
    double curveStepDb{0.5};

    CommandLine cmd(__FILE__);
    AddCommonArgs(cmd, common);
    cmd.AddValue("standard", "ax (Wi-Fi 6) or be (Wi-Fi 7)", standardStr);
    cmd.AddValue("psduSize",
                 "PSDU size in bytes used for the PER (4096 in the standard)",
                 psduBytes);
    cmd.AddValue("toleranceDb",
                 "Maximum excess (dB) of the ns-3 required SNR over the SNR implied by the "
                 "standard sensitivity for a PASS verdict",
                 toleranceDb);
    cmd.AddValue("curveStep",
                 "SNR step (dB) of the PER curves written to the CSV file",
                 curveStepDb);
    cmd.Parse(argc, argv);
    ApplyCommonArgs(common);

    const WifiStandard standard = ParseStandard(standardStr);
    const bool eht = (standard == WIFI_STANDARD_80211be);
    const std::string name = eht ? "wifi7-per-vs-snr" : "wifi6-per-vs-snr";

    PrintBanner("Link-level PER vs SNR and receiver sensitivity: " + StandardName(standard),
                "SNR required by the ns-3 error models for 10% PER (4096-byte PSDU) vs the SNR "
                "implied by the minimum sensitivity of the standard (NF 10 dB, 5 dB margin).",
                {"ieee80211ax", "ieee80211be", "ieee80211-2020", "patidar2017"});

    ResultTable table(name, common.outputDir);
    std::ofstream curves(common.outputDir + "/" + name + "-curves.csv");
    curves << "model,mcs,snr_db,per\n";

    struct ModelEntry
    {
        std::string name;
        Ptr<ErrorRateModel> model;
        bool ldpc;
    };

    std::vector<ModelEntry> models{
        {"table-ldpc", CreateObject<TableBasedErrorRateModel>(), true},
        {"table-bcc", CreateObject<TableBasedErrorRateModel>(), false},
        {"nist", CreateObject<NistErrorRateModel>(), false},
        {"yans", CreateObject<YansErrorRateModel>(), false},
    };

    const double noise20 = GetNoisePowerDbm(MHz_u{20}, STANDARD_NOISE_FIGURE_DB);
    for (const auto& entry : models)
    {
        for (uint8_t mcs = 0; mcs <= MaxMcs(standard); ++mcs)
        {
            const WifiMode mode(DataModeName(standard, mcs));
            WifiTxVector txVector;
            txVector.SetMode(mode);
            txVector.SetChannelWidth(MHz_u{20});
            txVector.SetNss(1);
            txVector.SetGuardInterval(NanoSeconds(800));
            txVector.SetLdpc(entry.ldpc);
            txVector.SetPreambleType(eht ? WIFI_PREAMBLE_EHT_MU : WIFI_PREAMBLE_HE_SU);

            const double snrReq =
                SnrAtPer(entry.model, mode, txVector, psduBytes, STANDARD_SENSITIVITY_PER);
            const double snrStd = GetStandardRequiredSnrDb(mcs);
            const auto params = GetMcsParams(mcs);
            // table-based model availability (LDPC up to MCS 11, BCC up to MCS 9)
            std::string note;
            if (entry.name == "table-ldpc" && mcs > 11)
            {
                note = "fallback-to-yans";
            }
            else if (entry.name == "table-bcc" && mcs > 9)
            {
                note = "fallback-to-yans";
            }

            ResultRow row;
            row.Set("model", entry.name);
            row.Set("mcs", mcs);
            row.Set("modulation", params.modulation);
            row.Set("code_rate", params.codeRate, 3);
            row.Set("psdu_bytes", psduBytes);
            row.Set("snr_10pct_per_db", snrReq, 2);
            row.Set("std_sensitivity_20mhz_dbm", GetMinSensitivityDbm(mcs, MHz_u{20}), 1);
            row.Set("std_implied_snr_db", snrStd, 2);
            row.Set("ns3_sensitivity_nf10_dbm", noise20 + snrReq, 2);
            row.Set("abs_diff_db", std::abs(snrReq - snrStd), 2);
            row.Set("note", note);
            row.Set("measured", snrReq, 2);
            // The sensitivity of the standard is a maximum allowed level: an ideal AWGN model may
            // require less SNR (positive margin). Fail only if ns-3 is more pessimistic than the
            // standard by more than toleranceDb.
            row.Set("margin_vs_standard_db", snrStd - snrReq, 2);
            std::string verdict = "INFO";
            if (entry.name == "table-ldpc" || entry.name == "table-bcc")
            {
                verdict = ((snrReq - snrStd) <= toleranceDb) ? "PASS" : "FAIL";
            }
            row.Set("reference", snrStd, 2);
            row.Set("deviation_pct", (snrReq - snrStd) / snrStd * 100, 1);
            row.Set("verdict", verdict);
            row.Set("ref_source", eht && mcs > 11 ? "ieee80211be" : "ieee80211ax");
            table.AddRow(row);

            for (double snrDb = -5; snrDb <= 60; snrDb += curveStepDb)
            {
                curves << entry.name << "," << +mcs << "," << snrDb << ","
                       << Per(entry.model, mode, txVector, snrDb, psduBytes) << "\n";
            }
        }
    }
    curves.close();

    table.Print();
    table.Write();
    const auto fails = table.PrintSummary();
    return fails ? 1 : 0;
}
