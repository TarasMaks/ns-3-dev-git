# wifi-benchmarks: Wi-Fi 6 / 7 / 8 benchmark suite for ns-3

`wifi-benchmarks` is a contrib module that provides a set of experimental simulation scripts
exercising the IEEE 802.11ax (Wi-Fi 6), IEEE 802.11be (Wi-Fi 7) and IEEE 802.11bn (Wi-Fi 8,
Ultra High Reliability) capabilities modeled by the ns-3 `wifi` module, and comparing the results
with figures published in the standards and in the research literature.

Every script prints a result table and writes `<outputDir>/<benchmark>.csv` and `.json`. Each row
carries a `measured` value, a `reference` value, the `deviation_pct`, a `verdict` and the
`ref_source` key of the citation the reference comes from:

- `PASS` / `FAIL`: the measurement is checked against a reference with a tolerance (analytical
  bound, value of the standard, or an expected ordering between configurations);
- `INFO`: the reference is reported for comparison only (qualitative claims of the literature,
  baselines for features that ns-3 does not model).

A benchmark exits with a non-zero status when a row fails, so the scripts double as regression
tests (`test/examples-to-run.py` runs all of them with `--quick`).

## Building and running

```bash
./ns3 configure --enable-examples --enable-tests
./ns3 build
./ns3 run "wifi-bench-phy-rates --standard=be"           # one benchmark
./ns3 run "wifi-bench-mlo-throughput --quick --verbose"  # short run with details
./test.py -s wifi-benchmarks                             # unit tests of the reference models
python3 contrib/wifi-benchmarks/scripts/run-suite.py --profile full --jobs 2 --svg
```

`scripts/run-suite.py` runs the benchmarks of one or all generations (`--generation wifi6|wifi7|wifi8`,
`--only REGEX`), in `quick` (seconds) or `full` (minutes) profile, and writes `summary.md`,
`summary.json` and optional SVG charts (`charts/`) into the output directory (default
`wifi-benchmarks-results/`). Every script accepts `--help`, `--quick`, `--outputDir`, `--rngRun`
and `--verbose`, plus scenario-specific options.

## Benchmarks

The scripts live in `examples/`. Feature-level scripts take `--standard=ax|be` so that the same
experiment can be run for Wi-Fi 6 and Wi-Fi 7 devices; the suite driver runs both.

### Wi-Fi 6 (802.11ax)

| Script | What is measured | Reference (source key) | Verdict |
|---|---|---|---|
| `wifi-bench-phy-rates --standard=ax` | HE data rates of ns-3 for all MCS/width/GI/NSS; saturated single-user goodput | Formula and tables of IEEE 802.11ax-2021 Clause 27.5, peak 9607.8 Mb/s (`ieee80211ax`, `khorov2019`); analytical single-user bound (AIFS + backoff + A-MPDU + SIFS + BlockAck) | PASS/FAIL (0.1% / 6%) |
| `wifi-bench-per-vs-snr --standard=ax` | PER vs SNR of the table-based, NIST and YANS error models; SNR at 10% PER for 4096-byte PSDUs | Receiver minimum sensitivity of IEEE 802.11ax-2021 Table 27-51 converted to SNR with the 10 dB NF and 5 dB margin of the standard (`ieee80211ax`, `patidar2017`) | PASS/FAIL for the table-based models (not more than 3 dB pessimistic), INFO otherwise |
| `wifi-bench-aggregation --standard=ax` | Goodput with no aggregation, 64 KB A-MPDU, BA window 64/256 (extended block ack), A-MSDU | Analytical bound with the max A-MPDU (6.5 MB) and BA window (256) of 802.11ax | PASS/FAIL (6%) |
| `wifi-bench-edca-bianchi --standard=ax` | Saturation throughput of 5..50 stations with 1/8/64 MPDUs per A-MPDU | Bianchi model with aggregation, ported from the ns-3 reference script (`bianchi2000`) | PASS/FAIL (8%) |
| `wifi-bench-rate-adaptation --standard=ax` | Goodput vs distance with Ideal, MinstrelHt and ThompsonSampling | Genie bound (best MCS given the SNR and the error model) and the expected goodput of the Ideal selection rule | PASS/FAIL for Ideal (10%), INFO otherwise |
| `wifi-bench-dl-ofdma --standard=ax` | DL OFDMA (RR scheduler, aggregated MU-BAR) vs SU: saturated goodput, latency and airtime with small packets | Per-RU data rates of the standard (`ieee80211ax`); OFDMA latency gains (`khorov2019`, `deng2020`) | AT_MOST PHY bound; INFO |
| `wifi-bench-ul-ofdma --standard=ax` | UL OFDMA with Basic/BSRP trigger frames and MU EDCA vs EDCA contention | Bianchi model for EDCA; per-RU PHY bound for OFDMA (`khorov2019`) | PASS/FAIL (Bianchi, 10%); AT_MOST bound |
| `wifi-bench-spatial-reuse --standard=ax` | Two overlapping BSSs with BSS colors and OBSS-PD thresholds; aggregate throughput and CCA-busy fraction | Concurrent transmissions expected when the OBSS level is below the threshold (`wilhelmi2021`, `khorov2019`) | AT_LEAST 1.4x when SR is expected; INFO |
| `wifi-bench-power-save --standard=ax` | Energy and latency of legacy power save with sporadic traffic (TWT is not modeled) | Sleep/idle current ratio; beacon interval (`nurchis2019`) | AT_MOST |

### Wi-Fi 7 (802.11be)

| Script | What is measured | Reference (source key) | Verdict |
|---|---|---|---|
| `wifi-bench-phy-rates --standard=be` | EHT data rates including 320 MHz and 4096-QAM (MCS 12-13); single-user goodput up to 320 MHz / 2 SS | IEEE 802.11be-2024 Clause 36.5 tables, 2882.4 Mb/s per stream at 320 MHz, 20% 4096-QAM gain, 23 Gb/s (8 SS) and 46 Gb/s (16 SS) peak rates (`ieee80211be`, `lopezperez2019`, `deng2020`) | PASS/FAIL |
| `wifi-bench-per-vs-snr --standard=be` | PER vs SNR for EHT MCS 0-13 (MCS 12-13 fall back to the YANS model) | Sensitivity of the standard (`ieee80211be`) | PASS/FAIL / INFO |
| `wifi-bench-aggregation --standard=be` | A-MPDU up to 15.5 MB, BA window 1024 | Analytical bound (`ieee80211be`) | PASS/FAIL (6%) |
| `wifi-bench-edca-bianchi`, `wifi-bench-rate-adaptation`, `wifi-bench-dl-ofdma`, `wifi-bench-ul-ofdma`, `wifi-bench-spatial-reuse` with `--standard=be` | Same experiments with EHT devices (RU/MRU rates of 802.11be) | as above | as above |
| `wifi-bench-mlo-throughput` | Aggregate saturated goodput of an AP MLD with 1, 2, 3 links (5 GHz/80, 6 GHz/80, 2.4 GHz/40) | Sum of the single-link bounds; capacity aggregation of STR MLO (`lopezraventos2022`, `carrascosa2023`) | PASS/FAIL (8%) |
| `wifi-bench-mlo-latency` | Latency percentiles with Poisson traffic and an overlapping BSS saturating one link: SLD on the contended link, SLD on the idle link, MLD | MLO steers traffic to the available link (`carrascosa2023`) | AT_MOST (MLO P95 not above the contended SLD) |
| `wifi-bench-emlsr` | EMLSR client vs SLD vs dual-radio MLD: saturated throughput and latency under contention; padding/transition delays configurable | Single-link bound; EMLSR description of 802.11be Clause 35.3.17 (`chen2022`, `galati2024`) | PASS/FAIL for SLD; INFO |
| `wifi-bench-tid-to-link` | Voice (AC_VO) latency with saturated best-effort load: default mapping vs AC_VO mapped to its own link | Traffic-to-link allocation of 802.11be (`lopezraventos2022`, `khorov2020`) | AT_MOST |
| `wifi-bench-dynamic-bw` | 80 MHz BSS with a partial-band interferer: dynamic bandwidth operation vs the full-band, dynamic-BW and ideal preamble puncturing bounds | Puncturing gain (`deng2020`, `lopezperez2019`); ns-3 does not exploit puncturing in the MAC | PASS/FAIL without interference; INFO |

### Wi-Fi 8 (802.11bn UHR)

ns-3 does not model 802.11bn. The PAR of the amendment defines its objectives relative to an
802.11be baseline (`ieee80211bn-par`, `galati2024`, `reshef2022`): at least 25% higher throughput at
the 5th percentile, at least 25% lower latency at the 95th percentile and at least 25% lower
MPDU loss due to mobility. These scripts produce the baseline KPIs with the Wi-Fi 6/7 mechanisms
available in ns-3 and emulate the simplest forms of the candidate features:

| Script | What is measured | Reference (source key) | Verdict |
|---|---|---|---|
| `wifi-bench-uhr-kpi` | Dense co-channel deployment (grid of APs, stations with DL best-effort and UL voice traffic): 5th-percentile throughput, P95/P99 latency, loss for Wi-Fi 6, Wi-Fi 7 MLO and Wi-Fi 7 MLO + spatial reuse, with the distance to the UHR objectives | `ieee80211bn-par`, `galati2024`, `tgax-scenarios` | AT_LEAST (Wi-Fi 7 not worse than Wi-Fi 6); INFO |
| `wifi-bench-multi-ap` | Two overlapping BSSs: uncoordinated vs emulated coordinated TDMA (alternate blocking of the APs' queues) vs coordinated spatial reuse (OBSS-PD proxy); 5th-percentile throughput and P95 latency vs the +25%/-25% objectives | `ieee80211bn-par`, `nunez2022`, `galati2024` | INFO |
| `wifi-bench-roaming` | Station moving between two bridged APs: loss ratio and maximum reception gap with missed-beacon triggered vs ideal (forced) re-association | Mobility loss objective (`ieee80211bn-par`); 802.11r/k/v not modeled | INFO |
| `wifi-bench-mlo-reliability` | Hidden jammer on one link: deadline miss ratio, loss (with a queue delay budget), latency and retransmissions for SLD on the jammed link, SLD on the clean link and MLO | Multi-link reliability baseline (`galati2024`, `reshef2022`) | AT_MOST (MLO not worse than the jammed SLD) |

## Reference models

`model/wifi-literature-reference.{h,cc}` contains the reference data and analytical models
independent from the ns-3 wifi implementation:

- the MCS table (modulation, coding rate), the number of data subcarriers per channel width and
  per resource unit, and the data rate formula `N_SD * N_BPSCS * R * N_SS / T_SYM` of the
  HE/EHT MCS tables, with spot values published in the standards and the survey literature;
- the receiver minimum input sensitivity per MCS (Table 27-51 of 802.11ax-2021, Clause 36 of
  802.11be-2024) and its conversion to SNR with the noise figure and implementation margin
  assumed by the standard;
- the default EDCA parameters, PHY timing constants and frame overheads;
- the analytical single-user goodput bound and the Bianchi saturation model with A-MPDU
  aggregation (ported from `src/wifi/examples/reference/bianchi11ax.py`, validated by the unit
  test against the values tabulated in `wifi-bianchi.cc`);
- the objectives of the P802.11bn PAR and the list of citations.

`helper/wifi-benchmark-helper.{h,cc}` provides the scenario builder (`BssBuilder`: standard,
links, PHY, rate control, MAC, OFDMA scheduler, EMLSR, TID-to-link mapping, BSS color/OBSS-PD,
static association), the traffic and measurement tools (`FlowSet` with per-packet latency
percentiles, throughput percentiles, loss, deadline miss ratio and reception gaps; `TxAccounting`
of the transmitted PPDUs; energy monitor; channel occupancy helpers), the PPDU duration
functions used by the analytical models and the result tables.

## Modeling choices worth knowing

- LDPC is enabled (mandatory for EHT); the table-based error model then covers HE/EHT MCS 0-11
  and falls back to the YANS model for EHT MCS 12-13.
- With a fixed MCS, control frames use the non-HT reference rate in 2.4/5 GHz and the HE MCS
  with the same modulation and coding in 6 GHz.
- The Bianchi comparisons disable the explicit BlockAckReq that ns-3 sends after a missed
  BlockAck, so that a collision costs one data PPDU as in the model.
- Under a Block Ack agreement ns-3 does not increment the retry count of MPDUs by default
  (`IncrementRetryCountUnderBa`), so frames are retried until the MAC queue delay limit expires;
  the reliability benchmark therefore uses a queue delay budget and a deadline miss ratio.
- The Ideal rate manager derives its SNR thresholds from the BCC error tables even when LDPC is
  used; the rate adaptation benchmark replicates that rule for its reference.
- UL OFDMA needs a TXOP limit at the AP so that the Basic Trigger Frame follows the BSRP exchange
  in the same TXOP; the MU EDCA Parameter Set is only advertised when all four AC timers are set.
- Throughput is measured on packets received within the measurement window; latency includes
  the queueing delay in the MAC queue (5000 packets, 10 s by default).

## Coverage of the Wi-Fi 6/7/8 features in ns-3

| Feature | ns-3 support | Benchmark |
|---|---|---|
| HE/EHT PHY rates, 1024/4096-QAM, 20-320 MHz, GI 0.8/1.6/3.2 us, up to 8 SS | yes | phy-rates |
| Link-level error models (table-based, NIST, YANS) | yes | per-vs-snr |
| A-MPDU/A-MSDU, extended Block Ack (256/1024) | yes | aggregation |
| EDCA, TXOP | yes | edca-bianchi |
| Rate adaptation (Ideal, MinstrelHt, ThompsonSampling) | yes | rate-adaptation |
| DL OFDMA (RU allocation, MU-BAR/aggregated MU-BAR ack) | yes (round robin scheduler) | dl-ofdma |
| UL OFDMA (Basic/BSRP trigger frames, BSR, MU EDCA) | yes | ul-ofdma |
| BSS coloring, OBSS-PD spatial reuse | yes | spatial-reuse |
| Legacy power save | yes | power-save |
| Target Wake Time (TWT), restricted TWT | no | power-save (proxy) |
| DL/UL MU-MIMO | PHY only, no MAC scheduling | not covered |
| Beamforming | no | not covered |
| Multi-link operation (STR), TID-to-link mapping, EMLSR | yes (EMLSR experimental) | mlo-*, tid-to-link, emlsr |
| NSTR link pairs, multi-link probe/reconfiguration | no | not covered |
| Preamble puncturing / MRU | PHY only, not exploited by the MAC | dynamic-bw (gap quantified) |
| Multi-AP coordination (Co-TDMA, Co-SR, Co-BF, joint TX) | no | multi-ap (emulation) |
| Enhanced MLO / seamless roaming (802.11bn), 802.11r/k/v | no | roaming, mlo-reliability (baselines) |
| Unequal modulation, enhanced long range, dynamic sub-channel operation (802.11bn) | no | not covered |

## References

- `ieee80211-2020`: IEEE Std 802.11-2020.
- `ieee80211ax`: IEEE Std 802.11ax-2021, Enhancements for High-Efficiency WLAN.
- `ieee80211be`: IEEE Std 802.11be-2024, Enhancements for Extremely High Throughput.
- `ieee80211bn-par`: IEEE P802.11bn Project Authorization Request (Ultra High Reliability), 2023.
- `khorov2019`: E. Khorov, A. Kiryanov, A. Lyakhov, G. Bianchi, "A Tutorial on IEEE 802.11ax High Efficiency WLANs," IEEE Communications Surveys & Tutorials, 21(1), 2019.
- `lopezperez2019`: D. Lopez-Perez et al., "IEEE 802.11be Extremely High Throughput: The Next Generation of Wi-Fi Technology Beyond 802.11ax," IEEE Communications Magazine, 57(9), 2019.
- `deng2020`: C. Deng et al., "IEEE 802.11be Wi-Fi 7: New Challenges and Opportunities," IEEE Communications Surveys & Tutorials, 22(4), 2020.
- `khorov2020`: E. Khorov, I. Levitsky, I. F. Akyildiz, "Current Status and Directions of IEEE 802.11be, the Future Wi-Fi 7," IEEE Access, 8, 2020.
- `chen2022`: C. Chen et al., "Overview and Performance Evaluation of Wi-Fi 7," IEEE Communications Standards Magazine, 6(2), 2022.
- `lopezraventos2022`: A. Lopez-Raventos, B. Bellalta, "Multi-link Operation in IEEE 802.11be WLANs," IEEE Wireless Communications, 29(4), 2022.
- `carrascosa2023`: M. Carrascosa-Zamacois et al., "Understanding Multi-link Operation in Wi-Fi 7: Performance, Anomalies, and Solutions," IEEE PIMRC, 2023.
- `galati2024`: L. Galati-Giordano et al., "What Will Wi-Fi 8 Be? A Primer on IEEE 802.11bn Ultra High Reliability," IEEE Communications Magazine, 62(8), 2024.
- `reshef2022`: E. Reshef, C. Cordeiro, "Future Directions for Wi-Fi 8 and Beyond," IEEE Communications Magazine, 60(10), 2022.
- `bianchi2000`: G. Bianchi, "Performance Analysis of the IEEE 802.11 Distributed Coordination Function," IEEE JSAC, 18(3), 2000.
- `patidar2017`: R. Patidar et al., "Link-to-System Mapping for ns-3 Wi-Fi OFDM Error Models," WNS3, 2017.
- `wilhelmi2021`: F. Wilhelmi et al., "Spatial Reuse in IEEE 802.11ax WLANs," Computer Communications, 170, 2021.
- `nurchis2019`: M. Nurchis, B. Bellalta, "Target Wake Time: Scheduled Access in IEEE 802.11ax WLANs," IEEE Wireless Communications, 26(3), 2019.
- `nunez2022`: D. Nunez et al., "TXOP Sharing with Coordinated Spatial Reuse in Multi-AP Cooperative IEEE 802.11be WLANs," IEEE CCNC, 2022.
- `tgax-scenarios`: IEEE 802.11-14/0980r16 "TGax Simulation Scenarios"; IEEE 802.11-14/0571r12 "11ax Evaluation Methodology".
- `ns3-wifi`: ns-3 wifi module documentation (design, validation, scope and limitations).
