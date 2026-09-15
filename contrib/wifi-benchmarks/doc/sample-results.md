# Sample results of the wifi-benchmarks suite

Full-profile run on the ns-3 3-dev tree of this repository (optimized build, one RNG run,
4-core Linux container). Regenerate with
`python3 contrib/wifi-benchmarks/scripts/run-suite.py --profile full`; the CSV/JSON files of
every benchmark carry all the columns summarized here.

# wifi-benchmarks summary (full profile, 2026-09-14 19:57:08)

Benchmarks: 26, with failures or errors: 0

| Benchmark | Generation | Status | PASS | FAIL | INFO | Duration (s) |
|---|---|---|---|---|---|---|
| wifi6-phy-rates | wifi6 | PASS | 19 | 0 | 0 |  |
| wifi6-per-vs-snr | wifi6 | PASS | 24 | 0 | 24 |  |
| wifi6-aggregation | wifi6 | PASS | 5 | 0 | 0 |  |
| wifi6-edca-bianchi | wifi6 | PASS | 15 | 0 | 0 |  |
| wifi6-rate-adaptation | wifi6 | PASS | 10 | 0 | 20 |  |
| wifi6-dl-ofdma | wifi6 | PASS | 6 | 0 | 12 |  |
| wifi6-ul-ofdma | wifi6 | PASS | 8 | 0 | 0 |  |
| wifi6-spatial-reuse | wifi6 | PASS | 4 | 0 | 14 |  |
| wifi6-power-save | wifi6 | PASS | 3 | 0 | 0 |  |
| wifi7-phy-rates | wifi7 | PASS | 20 | 0 | 0 |  |
| wifi7-per-vs-snr | wifi7 | PASS | 28 | 0 | 28 |  |
| wifi7-aggregation | wifi7 | PASS | 7 | 0 | 0 |  |
| wifi7-edca-bianchi | wifi7 | PASS | 15 | 0 | 0 |  |
| wifi7-rate-adaptation | wifi7 | PASS | 10 | 0 | 20 |  |
| wifi7-dl-ofdma | wifi7 | PASS | 6 | 0 | 12 |  |
| wifi7-ul-ofdma | wifi7 | PASS | 8 | 0 | 0 |  |
| wifi7-spatial-reuse | wifi7 | PASS | 4 | 0 | 14 |  |
| wifi7-mlo-throughput | wifi7 | PASS | 3 | 0 | 0 |  |
| wifi7-mlo-latency | wifi7 | PASS | 1 | 0 | 2 |  |
| wifi7-emlsr | wifi7 | PASS | 1 | 0 | 5 |  |
| wifi7-tid-to-link | wifi7 | PASS | 1 | 0 | 1 |  |
| wifi7-dynamic-bw | wifi7 | PASS | 1 | 0 | 4 |  |
| wifi8-uhr-kpi | wifi8 | PASS | 1 | 0 | 2 |  |
| wifi8-multi-ap | wifi8 | PASS | 0 | 0 | 3 |  |
| wifi8-roaming | wifi8 | PASS | 0 | 0 | 2 |  |
| wifi8-mlo-reliability | wifi8 | PASS | 1 | 0 | 2 |  |

## wifi6-phy-rates

HE PHY data rates vs IEEE 802.11ax-2021 tables; saturated single-user goodput vs the analytical bound

Command: `./ns3 run --no-build 'wifi-bench-phy-rates --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| test=formula_vs_ns3_all_mcs_width_gi_nss, combinations=1152, mismatches=0, max_abs_diff_mbps=0.0 | 0 | 0.0 | 0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=0, width_mhz=20, gi_ns=800 | 8.6 | 8.6 | 0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=7, width_mhz=20, gi_ns=800 | 86.03 | 86.0 | 0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=20, gi_ns=800 | 143.38 | 143.4 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=20, gi_ns=3200 | 121.88 | 121.9 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=40, gi_ns=800 | 286.76 | 286.8 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=9, width_mhz=80, gi_ns=800 | 480.39 | 480.4 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=80, gi_ns=800 | 600.49 | 600.5 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=160, gi_ns=800 | 1200.98 | 1201.0 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=160, gi_ns=800 | 2401.96 | 2402.0 | -0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=11, width_mhz=160, gi_ns=800 | 9607.84 | 9607.8 | 0.0 | PASS | khorov2019 |
| test=peak_rate_8ss | 9607.8 | 9607.8 | 0.0 | PASS | khorov2019 |
| test=single_user_goodput, mcs=0, width_mhz=20, gi_ns=800 | 7.7 | 7.711 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=7, width_mhz=80, gi_ns=800 | 329.99 | 330.546 | -0.2 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=20, gi_ns=800 | 131.29 | 131.482 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=80, gi_ns=800 | 549.5 | 550.19 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=80, gi_ns=800 | 1054.92 | 1058.182 | -0.3 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=160, gi_ns=800 | 1057.6 | 1061.162 | -0.3 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=160, gi_ns=800 | 1959.34 | 1970.75 | -0.6 | PASS | ieee80211-2020 |

## wifi6-per-vs-snr

PER vs SNR of the error models; SNR at 10% PER vs the receiver sensitivity of the standard

Command: `./ns3 run --no-build 'wifi-bench-per-vs-snr --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| model=table-ldpc, mcs=0, modulation=BPSK, code_rate=0.5 | -0.5 | 3.99 | -112.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=1, modulation=QPSK, code_rate=0.5 | 2.5 | 6.99 | -64.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=2, modulation=QPSK, code_rate=0.75 | 4.97 | 8.99 | -44.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=3, modulation=16-QAM, code_rate=0.5 | 8.07 | 11.99 | -32.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=4, modulation=16-QAM, code_rate=0.75 | 11.06 | 15.99 | -30.8 | PASS | ieee80211ax |
| model=table-ldpc, mcs=5, modulation=64-QAM, code_rate=0.667 | 15.25 | 19.99 | -23.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=6, modulation=64-QAM, code_rate=0.75 | 16.67 | 20.99 | -20.6 | PASS | ieee80211ax |
| model=table-ldpc, mcs=7, modulation=64-QAM, code_rate=0.833 | 18.2 | 21.99 | -17.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=8, modulation=256-QAM, code_rate=0.75 | 21.85 | 26.99 | -19.0 | PASS | ieee80211ax |
| model=table-ldpc, mcs=9, modulation=256-QAM, code_rate=0.833 | 23.7 | 28.99 | -18.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=10, modulation=1024-QAM, code_rate=0.75 | 26.99 | 31.99 | -15.6 | PASS | ieee80211ax |
| model=table-ldpc, mcs=11, modulation=1024-QAM, code_rate=0.833 | 29.09 | 33.99 | -14.4 | PASS | ieee80211ax |
| model=table-bcc, mcs=0, modulation=BPSK, code_rate=0.5 | 1.27 | 3.99 | -68.3 | PASS | ieee80211ax |
| model=table-bcc, mcs=1, modulation=QPSK, code_rate=0.5 | 4.29 | 6.99 | -38.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=2, modulation=QPSK, code_rate=0.75 | 6.77 | 8.99 | -24.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=3, modulation=16-QAM, code_rate=0.5 | 10.02 | 11.99 | -16.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=4, modulation=16-QAM, code_rate=0.75 | 13.14 | 15.99 | -17.9 | PASS | ieee80211ax |
| model=table-bcc, mcs=5, modulation=64-QAM, code_rate=0.667 | 17.48 | 19.99 | -12.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=6, modulation=64-QAM, code_rate=0.75 | 18.8 | 20.99 | -10.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=7, modulation=64-QAM, code_rate=0.833 | 19.99 | 21.99 | -9.1 | PASS | ieee80211ax |
| model=table-bcc, mcs=8, modulation=256-QAM, code_rate=0.75 | 24.16 | 26.99 | -10.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=9, modulation=256-QAM, code_rate=0.833 | 25.57 | 28.99 | -11.8 | PASS | ieee80211ax |
| model=table-bcc, mcs=10, modulation=1024-QAM, code_rate=0.75 | 32.84 | 31.99 | 2.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=11, modulation=1024-QAM, code_rate=0.833 | 34.75 | 33.99 | 2.2 | PASS | ieee80211ax |
| model=nist, mcs=0, modulation=BPSK, code_rate=0.5 | 4.24 | 3.99 | 6.2 | INFO | ieee80211ax |
| model=nist, mcs=1, modulation=QPSK, code_rate=0.5 | 7.25 | 6.99 | 3.7 | INFO | ieee80211ax |
| model=nist, mcs=2, modulation=QPSK, code_rate=0.75 | 10.16 | 8.99 | 13.0 | INFO | ieee80211ax |
| model=nist, mcs=3, modulation=16-QAM, code_rate=0.5 | 13.81 | 11.99 | 15.2 | INFO | ieee80211ax |
| model=nist, mcs=4, modulation=16-QAM, code_rate=0.75 | 16.92 | 15.99 | 5.8 | INFO | ieee80211ax |
| model=nist, mcs=5, modulation=64-QAM, code_rate=0.667 | 21.66 | 19.99 | 8.4 | INFO | ieee80211ax |
| model=nist, mcs=6, modulation=64-QAM, code_rate=0.75 | 22.94 | 20.99 | 9.3 | INFO | ieee80211ax |
| model=nist, mcs=7, modulation=64-QAM, code_rate=0.833 | 24.11 | 21.99 | 9.6 | INFO | ieee80211ax |
| model=nist, mcs=8, modulation=256-QAM, code_rate=0.75 | 28.82 | 26.99 | 6.8 | INFO | ieee80211ax |
| model=nist, mcs=9, modulation=256-QAM, code_rate=0.833 | 30.03 | 28.99 | 3.6 | INFO | ieee80211ax |
| model=nist, mcs=10, modulation=1024-QAM, code_rate=0.75 | 34.69 | 31.99 | 8.4 | INFO | ieee80211ax |
| model=nist, mcs=11, modulation=1024-QAM, code_rate=0.833 | 35.93 | 33.99 | 5.7 | INFO | ieee80211ax |
| model=yans, mcs=0, modulation=BPSK, code_rate=0.5 | 2.68 | 3.99 | -32.9 | INFO | ieee80211ax |
| model=yans, mcs=1, modulation=QPSK, code_rate=0.5 | 5.67 | 6.99 | -18.9 | INFO | ieee80211ax |
| model=yans, mcs=2, modulation=QPSK, code_rate=0.75 | 8.52 | 8.99 | -5.3 | INFO | ieee80211ax |
| model=yans, mcs=3, modulation=16-QAM, code_rate=0.5 | 12.12 | 11.99 | 1.1 | INFO | ieee80211ax |
| model=yans, mcs=4, modulation=16-QAM, code_rate=0.75 | 15.22 | 15.99 | -4.8 | INFO | ieee80211ax |
| model=yans, mcs=5, modulation=64-QAM, code_rate=0.667 | 19.8 | 19.99 | -1.0 | INFO | ieee80211ax |
| model=yans, mcs=6, modulation=64-QAM, code_rate=0.75 | 21.19 | 20.99 | 1.0 | INFO | ieee80211ax |
| model=yans, mcs=7, modulation=64-QAM, code_rate=0.833 | 22.95 | 21.99 | 4.4 | INFO | ieee80211ax |
| model=yans, mcs=8, modulation=256-QAM, code_rate=0.75 | 27.02 | 26.99 | 0.1 | INFO | ieee80211ax |
| model=yans, mcs=9, modulation=256-QAM, code_rate=0.833 | 28.86 | 28.99 | -0.4 | INFO | ieee80211ax |
| model=yans, mcs=10, modulation=1024-QAM, code_rate=0.75 | 32.84 | 31.99 | 2.6 | INFO | ieee80211ax |
| model=yans, mcs=11, modulation=1024-QAM, code_rate=0.833 | 34.75 | 33.99 | 2.2 | INFO | ieee80211ax |

## wifi6-aggregation

A-MPDU / A-MSDU / Block Ack window (256) goodput vs the analytical bound

Command: `./ns3 run --no-build 'wifi-bench-aggregation --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| config=no-aggregation, mcs=11, width_mhz=80, max_ampdu_bytes=0 | 52.33 | 53.117 | -1.5 | PASS | ieee80211-2020 |
| config=ampdu-64KB-ba64, mcs=11, width_mhz=80, max_ampdu_bytes=65535 | 460.7 | 462.711 | -0.4 | PASS | ieee80211-2020 |
| config=ampdu-max-ba64, mcs=11, width_mhz=80, max_ampdu_bytes=6500631 | 495.38 | 496.583 | -0.2 | PASS | ieee80211ax |
| config=ampdu-max-ba256, mcs=11, width_mhz=80, max_ampdu_bytes=6500631 | 549.5 | 550.19 | -0.1 | PASS | ieee80211ax |
| config=amsdu-8KB-ampdu-max-ba256, mcs=11, width_mhz=80, max_ampdu_bytes=6500631 | 555.62 | 555.739 | -0.0 | PASS | ieee80211ax |

## wifi6-edca-bianchi

EDCA saturation throughput vs the Bianchi model with A-MPDU aggregation

Command: `./ns3 run --no-build 'wifi-bench-edca-bianchi --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| n_stations=5, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 33.777 | 32.99 | 2.4 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 32.156 | 30.825 | 4.3 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 30.001 | 28.488 | 5.3 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 28.773 | 27.046 | 6.4 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 26.382 | 25.107 | 5.1 | PASS | bianchi2000 |
| n_stations=5, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 61.117 | 60.849 | 0.4 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 56.843 | 55.899 | 1.7 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 53.467 | 51.099 | 4.6 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 49.063 | 48.267 | 1.6 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 46.248 | 44.56 | 3.8 | PASS | bianchi2000 |
| n_stations=5, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 68.077 | 67.502 | 0.9 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 61.899 | 61.775 | 0.2 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 58.15 | 56.333 | 3.2 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 53.071 | 53.152 | -0.2 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 46.35 | 49.01 | -5.4 | PASS | bianchi2000 |

## wifi6-rate-adaptation

Goodput vs distance with Ideal / MinstrelHt / ThompsonSampling vs a genie bound

Command: `./ns3 run --no-build 'wifi-bench-rate-adaptation --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 549.26 | 550.19 | -0.2 | PASS | patidar2017 |
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 258.54 | 550.19 | -53.0 | INFO | patidar2017 |
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 549.33 | 550.19 | -0.2 | INFO | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 549.53 | 550.19 | -0.1 | PASS | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 65.36 | 550.19 | -88.1 | INFO | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 549.46 | 550.19 | -0.1 | INFO | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 440.75 | 441.425 | -0.2 | PASS | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 133.33 | 550.19 | -75.8 | INFO | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 439.56 | 550.19 | -20.1 | INFO | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 330.18 | 330.546 | -0.1 | PASS | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 58.74 | 395.523 | -85.1 | INFO | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 295.45 | 395.523 | -25.3 | INFO | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 198.12 | 198.126 | -0.0 | PASS | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 92.77 | 296.605 | -68.7 | INFO | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 149.43 | 296.605 | -49.6 | INFO | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 132.03 | 131.931 | 0.1 | PASS | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 180.81 | 198.126 | -8.7 | INFO | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 130.04 | 198.126 | -34.4 | INFO | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 19.92 | 0.0 | inf | INFO | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 88.54 | 0.0 | inf | INFO | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 15.1 | 0.0 | inf | INFO | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 37.95 | 0.0 | inf | INFO | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |

## wifi6-dl-ofdma

DL OFDMA vs SU: throughput vs the per-RU PHY bound, latency with small packets

Command: `./ns3 run --no-build 'wifi-bench-dl-ofdma --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| scenario=saturated, n_stations=2, mode=SU, mcs=7 | 78.79 | 86.029 | -8.4 | PASS | ieee80211ax |
| scenario=saturated, n_stations=2, mode=DL-OFDMA, mcs=7 | 67.98 | 75.0 | -9.4 | PASS | ieee80211ax |
| scenario=saturated, n_stations=2, mode=gain-OFDMA-vs-SU, mcs=7 | 0.863 | 0.872 | -1.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=SU, mcs=7 | 0.289 | 0.289 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=DL-OFDMA, mcs=7 | 0.318 | 0.289 | 10.2 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=gain-OFDMA-vs-SU, mcs=7 | 0.907 | 1.0 | -9.3 | INFO | khorov2019 |
| scenario=saturated, n_stations=4, mode=SU, mcs=7 | 78.85 | 86.029 | -8.3 | PASS | ieee80211ax |
| scenario=saturated, n_stations=4, mode=DL-OFDMA, mcs=7 | 63.52 | 70.588 | -10.0 | PASS | ieee80211ax |
| scenario=saturated, n_stations=4, mode=gain-OFDMA-vs-SU, mcs=7 | 0.806 | 0.821 | -1.8 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=SU, mcs=7 | 0.451 | 0.451 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=DL-OFDMA, mcs=7 | 0.384 | 0.451 | -14.9 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=gain-OFDMA-vs-SU, mcs=7 | 1.175 | 1.0 | 17.5 | INFO | khorov2019 |
| scenario=saturated, n_stations=8, mode=SU, mcs=7 | 78.86 | 86.029 | -8.3 | PASS | ieee80211ax |
| scenario=saturated, n_stations=8, mode=DL-OFDMA, mcs=7 | 63.52 | 70.588 | -10.0 | PASS | ieee80211ax |
| scenario=saturated, n_stations=8, mode=gain-OFDMA-vs-SU, mcs=7 | 0.805 | 0.821 | -1.8 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=SU, mcs=7 | 1.143 | 1.143 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=DL-OFDMA, mcs=7 | 0.629 | 1.143 | -45.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=gain-OFDMA-vs-SU, mcs=7 | 1.818 | 1.0 | 81.8 | INFO | khorov2019 |

## wifi6-ul-ofdma

UL OFDMA (Basic / BSRP trigger frames, MU EDCA) vs EDCA contention (Bianchi)

Command: `./ns3 run --no-build 'wifi-bench-ul-ofdma --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| n_stations=4, mode=edca, mcs=7, width_mhz=20 | 69.51 | 69.352 | 0.2 | PASS | bianchi2000 |
| n_stations=4, mode=ofdma, mcs=7, width_mhz=20 | 59.65 | 70.588 | -15.5 | PASS | ieee80211ax |
| n_stations=4, mode=ofdma-bsrp, mcs=7, width_mhz=20 | 57.53 | 69.826 | -17.6 | PASS | ieee80211ax |
| n_stations=4, mode=ofdma-bsrp-muedca, mcs=7, width_mhz=20 | 51.53 | 70.588 | -27.0 | PASS | ieee80211ax |
| n_stations=8, mode=edca, mcs=7, width_mhz=20 | 64.7 | 63.531 | 1.8 | PASS | bianchi2000 |
| n_stations=8, mode=ofdma, mcs=7, width_mhz=20 | 60.69 | 70.588 | -14.0 | PASS | ieee80211ax |
| n_stations=8, mode=ofdma-bsrp, mcs=7, width_mhz=20 | 58.93 | 69.085 | -14.7 | PASS | ieee80211ax |
| n_stations=8, mode=ofdma-bsrp-muedca, mcs=7, width_mhz=20 | 51.51 | 70.588 | -27.0 | PASS | ieee80211ax |

## wifi6-spatial-reuse

BSS coloring / OBSS-PD spatial reuse between two overlapping BSSs

Command: `./ns3 run --no-build 'wifi-bench-spatial-reuse --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| ap_distance_m=100, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 33.26 | 33.261 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 33.34 | 33.261 | 0.2 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 59.89 | 66.523 | -10.0 | PASS | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 59.95 | 66.523 | -9.9 | PASS | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 31.62 | 33.261 | -4.9 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 31.57 | 33.261 | -5.1 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 33.98 | 33.98 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 33.71 | 33.98 | -0.8 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 60.2 | 67.959 | -11.4 | PASS | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 60.19 | 67.959 | -11.4 | PASS | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 50.01 | 33.98 | 47.2 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 31.65 | 33.98 | -6.9 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 62.77 | 62.766 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 62.75 | 62.766 | -0.0 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 62.78 | 62.766 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 62.82 | 62.766 | 0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 62.68 | 62.766 | -0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 62.85 | 62.766 | 0.1 | INFO | wilhelmi2021 |

## wifi6-power-save

Legacy power save energy and latency (TWT proxy)

Command: `./ns3 run --no-build 'wifi-bench-power-save --outputDir=wifi-benchmarks-results --standard=ax'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=active, packet_interval_ms=100.0, beacon_interval_ms=102.4, rx_packets=50 | 0.8196 | 0.819 | 0.1 | PASS | ns3-wifi |
| mode=power-save, packet_interval_ms=100.0, beacon_interval_ms=102.4, rx_packets=50 | 0.1036 | 0.819 | -87.3 | PASS | nurchis2019 |
| mode=power-save-latency, packet_interval_ms=100.0, beacon_interval_ms=102.4, latency_mean_ms=55.6 | 95.138 | 102.4 | -7.1 | PASS | ieee80211-2020 |

## wifi7-phy-rates

EHT PHY rates (320 MHz, 4096-QAM) vs IEEE 802.11be-2024 tables; single-user goodput bound

Command: `./ns3 run --no-build 'wifi-bench-phy-rates --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| test=formula_vs_ns3_all_mcs_width_gi_nss, combinations=1680, mismatches=0, max_abs_diff_mbps=0.0 | 0 | 0.0 | 0.0 | PASS | ieee80211ax |
| test=published_rate, mcs=13, width_mhz=20, gi_ns=800 | 172.06 | 172.1 | -0.0 | PASS | ieee80211be |
| test=published_rate, mcs=11, width_mhz=320, gi_ns=800 | 2401.96 | 2402.0 | -0.0 | PASS | ieee80211be |
| test=published_rate, mcs=12, width_mhz=320, gi_ns=800 | 2594.12 | 2594.1 | 0.0 | PASS | ieee80211be |
| test=published_rate, mcs=13, width_mhz=320, gi_ns=800 | 2882.35 | 2882.4 | -0.0 | PASS | ieee80211be |
| test=published_rate, mcs=13, width_mhz=160, gi_ns=800 | 1441.18 | 1441.2 | -0.0 | PASS | ieee80211be |
| test=published_rate, mcs=13, width_mhz=320, gi_ns=800 | 23058.82 | 23059.2 | -0.0 | PASS | deng2020 |
| test=4096qam_gain_mcs13_vs_mcs11 | 1.2 | 1.2 | 0.0 | PASS | lopezperez2019 |
| test=320mhz_vs_160mhz_ratio | 2.0 | 2.0 | 0.0 | PASS | deng2020 |
| test=peak_rate_16ss_extrapolated | 46117.6 | 46118.4 | -0.0 | PASS | deng2020 |
| test=single_user_goodput, mcs=0, width_mhz=20, gi_ns=800 | 7.48 | 7.488 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=7, width_mhz=80, gi_ns=800 | 327.97 | 329.609 | -0.5 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=20, gi_ns=800 | 156.79 | 157.599 | -0.5 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=80, gi_ns=800 | 655.99 | 659.217 | -0.5 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=80, gi_ns=800 | 1311.88 | 1318.652 | -0.5 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=160, gi_ns=800 | 1313.72 | 1320.521 | -0.5 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=160, gi_ns=800 | 2592.6 | 2610.487 | -0.7 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=11, width_mhz=320, gi_ns=800 | 2182.26 | 2183.544 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=320, gi_ns=800 | 2597.73 | 2599.682 | -0.1 | PASS | ieee80211-2020 |
| test=single_user_goodput, mcs=13, width_mhz=320, gi_ns=800 | 4899.97 | 4908.065 | -0.2 | PASS | ieee80211-2020 |

## wifi7-per-vs-snr

PER vs SNR for EHT MCS 0-13 vs the receiver sensitivity of the standard

Command: `./ns3 run --no-build 'wifi-bench-per-vs-snr --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| model=table-ldpc, mcs=0, modulation=BPSK, code_rate=0.5 | -0.5 | 3.99 | -112.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=1, modulation=QPSK, code_rate=0.5 | 2.5 | 6.99 | -64.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=2, modulation=QPSK, code_rate=0.75 | 4.97 | 8.99 | -44.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=3, modulation=16-QAM, code_rate=0.5 | 8.07 | 11.99 | -32.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=4, modulation=16-QAM, code_rate=0.75 | 11.06 | 15.99 | -30.8 | PASS | ieee80211ax |
| model=table-ldpc, mcs=5, modulation=64-QAM, code_rate=0.667 | 15.25 | 19.99 | -23.7 | PASS | ieee80211ax |
| model=table-ldpc, mcs=6, modulation=64-QAM, code_rate=0.75 | 16.67 | 20.99 | -20.6 | PASS | ieee80211ax |
| model=table-ldpc, mcs=7, modulation=64-QAM, code_rate=0.833 | 18.2 | 21.99 | -17.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=8, modulation=256-QAM, code_rate=0.75 | 21.85 | 26.99 | -19.0 | PASS | ieee80211ax |
| model=table-ldpc, mcs=9, modulation=256-QAM, code_rate=0.833 | 23.7 | 28.99 | -18.3 | PASS | ieee80211ax |
| model=table-ldpc, mcs=10, modulation=1024-QAM, code_rate=0.75 | 26.99 | 31.99 | -15.6 | PASS | ieee80211ax |
| model=table-ldpc, mcs=11, modulation=1024-QAM, code_rate=0.833 | 29.09 | 33.99 | -14.4 | PASS | ieee80211ax |
| model=table-ldpc, mcs=12, modulation=4096-QAM, code_rate=0.75 | 38.66 | 36.99 | 4.5 | PASS | ieee80211be |
| model=table-ldpc, mcs=13, modulation=4096-QAM, code_rate=0.833 | 40.64 | 38.99 | 4.2 | PASS | ieee80211be |
| model=table-bcc, mcs=0, modulation=BPSK, code_rate=0.5 | 1.27 | 3.99 | -68.3 | PASS | ieee80211ax |
| model=table-bcc, mcs=1, modulation=QPSK, code_rate=0.5 | 4.29 | 6.99 | -38.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=2, modulation=QPSK, code_rate=0.75 | 6.77 | 8.99 | -24.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=3, modulation=16-QAM, code_rate=0.5 | 10.02 | 11.99 | -16.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=4, modulation=16-QAM, code_rate=0.75 | 13.14 | 15.99 | -17.9 | PASS | ieee80211ax |
| model=table-bcc, mcs=5, modulation=64-QAM, code_rate=0.667 | 17.48 | 19.99 | -12.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=6, modulation=64-QAM, code_rate=0.75 | 18.8 | 20.99 | -10.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=7, modulation=64-QAM, code_rate=0.833 | 19.99 | 21.99 | -9.1 | PASS | ieee80211ax |
| model=table-bcc, mcs=8, modulation=256-QAM, code_rate=0.75 | 24.16 | 26.99 | -10.5 | PASS | ieee80211ax |
| model=table-bcc, mcs=9, modulation=256-QAM, code_rate=0.833 | 25.57 | 28.99 | -11.8 | PASS | ieee80211ax |
| model=table-bcc, mcs=10, modulation=1024-QAM, code_rate=0.75 | 32.84 | 31.99 | 2.6 | PASS | ieee80211ax |
| model=table-bcc, mcs=11, modulation=1024-QAM, code_rate=0.833 | 34.75 | 33.99 | 2.2 | PASS | ieee80211ax |
| model=table-bcc, mcs=12, modulation=4096-QAM, code_rate=0.75 | 38.66 | 36.99 | 4.5 | PASS | ieee80211be |
| model=table-bcc, mcs=13, modulation=4096-QAM, code_rate=0.833 | 40.64 | 38.99 | 4.2 | PASS | ieee80211be |
| model=nist, mcs=0, modulation=BPSK, code_rate=0.5 | 4.24 | 3.99 | 6.2 | INFO | ieee80211ax |
| model=nist, mcs=1, modulation=QPSK, code_rate=0.5 | 7.25 | 6.99 | 3.7 | INFO | ieee80211ax |
| model=nist, mcs=2, modulation=QPSK, code_rate=0.75 | 10.16 | 8.99 | 13.0 | INFO | ieee80211ax |
| model=nist, mcs=3, modulation=16-QAM, code_rate=0.5 | 13.81 | 11.99 | 15.2 | INFO | ieee80211ax |
| model=nist, mcs=4, modulation=16-QAM, code_rate=0.75 | 16.92 | 15.99 | 5.8 | INFO | ieee80211ax |
| model=nist, mcs=5, modulation=64-QAM, code_rate=0.667 | 21.66 | 19.99 | 8.4 | INFO | ieee80211ax |
| model=nist, mcs=6, modulation=64-QAM, code_rate=0.75 | 22.94 | 20.99 | 9.3 | INFO | ieee80211ax |
| model=nist, mcs=7, modulation=64-QAM, code_rate=0.833 | 24.11 | 21.99 | 9.6 | INFO | ieee80211ax |
| model=nist, mcs=8, modulation=256-QAM, code_rate=0.75 | 28.82 | 26.99 | 6.8 | INFO | ieee80211ax |
| model=nist, mcs=9, modulation=256-QAM, code_rate=0.833 | 30.03 | 28.99 | 3.6 | INFO | ieee80211ax |
| model=nist, mcs=10, modulation=1024-QAM, code_rate=0.75 | 34.69 | 31.99 | 8.4 | INFO | ieee80211ax |
| model=nist, mcs=11, modulation=1024-QAM, code_rate=0.833 | 35.93 | 33.99 | 5.7 | INFO | ieee80211ax |
| model=nist, mcs=12, modulation=4096-QAM, code_rate=0.75 | 40.56 | 36.99 | 9.6 | INFO | ieee80211be |
| model=nist, mcs=13, modulation=4096-QAM, code_rate=0.833 | 41.84 | 38.99 | 7.3 | INFO | ieee80211be |
| model=yans, mcs=0, modulation=BPSK, code_rate=0.5 | 2.68 | 3.99 | -32.9 | INFO | ieee80211ax |
| model=yans, mcs=1, modulation=QPSK, code_rate=0.5 | 5.67 | 6.99 | -18.9 | INFO | ieee80211ax |
| model=yans, mcs=2, modulation=QPSK, code_rate=0.75 | 8.52 | 8.99 | -5.3 | INFO | ieee80211ax |
| model=yans, mcs=3, modulation=16-QAM, code_rate=0.5 | 12.12 | 11.99 | 1.1 | INFO | ieee80211ax |
| model=yans, mcs=4, modulation=16-QAM, code_rate=0.75 | 15.22 | 15.99 | -4.8 | INFO | ieee80211ax |
| model=yans, mcs=5, modulation=64-QAM, code_rate=0.667 | 19.8 | 19.99 | -1.0 | INFO | ieee80211ax |
| model=yans, mcs=6, modulation=64-QAM, code_rate=0.75 | 21.19 | 20.99 | 1.0 | INFO | ieee80211ax |
| model=yans, mcs=7, modulation=64-QAM, code_rate=0.833 | 22.95 | 21.99 | 4.4 | INFO | ieee80211ax |
| model=yans, mcs=8, modulation=256-QAM, code_rate=0.75 | 27.02 | 26.99 | 0.1 | INFO | ieee80211ax |
| model=yans, mcs=9, modulation=256-QAM, code_rate=0.833 | 28.86 | 28.99 | -0.4 | INFO | ieee80211ax |
| model=yans, mcs=10, modulation=1024-QAM, code_rate=0.75 | 32.84 | 31.99 | 2.6 | INFO | ieee80211ax |
| model=yans, mcs=11, modulation=1024-QAM, code_rate=0.833 | 34.75 | 33.99 | 2.2 | INFO | ieee80211ax |
| model=yans, mcs=12, modulation=4096-QAM, code_rate=0.75 | 38.66 | 36.99 | 4.5 | INFO | ieee80211be |
| model=yans, mcs=13, modulation=4096-QAM, code_rate=0.833 | 40.64 | 38.99 | 4.2 | INFO | ieee80211be |

## wifi7-aggregation

A-MPDU up to 15.5 MB and Block Ack window 1024 goodput vs the analytical bound

Command: `./ns3 run --no-build 'wifi-bench-aggregation --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| config=no-aggregation, mcs=13, width_mhz=80, max_ampdu_bytes=0 | 51.41 | 52.175 | -1.5 | PASS | ieee80211-2020 |
| config=ampdu-64KB-ba64, mcs=13, width_mhz=80, max_ampdu_bytes=65535 | 533.09 | 535.679 | -0.5 | PASS | ieee80211-2020 |
| config=ampdu-max-ba64, mcs=13, width_mhz=80, max_ampdu_bytes=6500631 | 575.61 | 577.919 | -0.4 | PASS | ieee80211ax |
| config=ampdu-max-ba256, mcs=13, width_mhz=80, max_ampdu_bytes=6500631 | 654.37 | 655.46 | -0.2 | PASS | ieee80211ax |
| config=amsdu-8KB-ampdu-max-ba256, mcs=13, width_mhz=80, max_ampdu_bytes=6500631 | 665.85 | 666.578 | -0.1 | PASS | ieee80211ax |
| config=ampdu-max-ba1024, mcs=13, width_mhz=80, max_ampdu_bytes=15523200 | 655.91 | 659.217 | -0.5 | PASS | ieee80211be |
| config=amsdu-8KB-ampdu-max-ba1024, mcs=13, width_mhz=80, max_ampdu_bytes=15523200 | 661.55 | 665.144 | -0.5 | PASS | ieee80211be |

## wifi7-edca-bianchi

EDCA saturation throughput of EHT stations vs the Bianchi model

Command: `./ns3 run --no-build 'wifi-bench-edca-bianchi --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| n_stations=5, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 33.809 | 32.56 | 3.8 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 31.791 | 30.414 | 4.5 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 29.742 | 28.103 | 5.8 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 28.27 | 26.678 | 6.0 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=1, mcs=7, width_mhz=20 | 24.635 | 24.763 | -0.5 | PASS | bianchi2000 |
| n_stations=5, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 61.734 | 60.664 | 1.8 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 57.29 | 55.729 | 2.8 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 52.113 | 50.943 | 2.3 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 48.58 | 48.119 | 1.0 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=8, mcs=7, width_mhz=20 | 43.642 | 44.423 | -1.8 | PASS | bianchi2000 |
| n_stations=5, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 68.642 | 67.453 | 1.8 | PASS | bianchi2000 |
| n_stations=10, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 61.612 | 61.73 | -0.2 | PASS | bianchi2000 |
| n_stations=20, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 56.694 | 56.292 | 0.7 | PASS | bianchi2000 |
| n_stations=30, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 53.019 | 53.113 | -0.2 | PASS | bianchi2000 |
| n_stations=50, mpdus_per_ampdu=37, mcs=7, width_mhz=20 | 47.371 | 48.974 | -3.3 | PASS | bianchi2000 |

## wifi7-rate-adaptation

Goodput vs distance with rate adaptation for EHT

Command: `./ns3 run --no-build 'wifi-bench-rate-adaptation --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 655.88 | 659.217 | -0.5 | PASS | patidar2017 |
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 128.65 | 659.217 | -80.5 | INFO | patidar2017 |
| distance_m=1.0, rx_power_dbm=-26.7, primary20_rssi_dbm=-32.7, preamble_detectable=yes | 547.01 | 659.217 | -17.0 | INFO | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 591.38 | 593.527 | -0.4 | PASS | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 64.56 | 593.527 | -89.1 | INFO | patidar2017 |
| distance_m=5.0, rx_power_dbm=-47.6, primary20_rssi_dbm=-53.7, preamble_detectable=yes | 547.21 | 593.527 | -7.8 | INFO | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 437.94 | 440.174 | -0.5 | PASS | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 309.0 | 549.977 | -43.8 | INFO | patidar2017 |
| distance_m=10.0, rx_power_dbm=-56.7, primary20_rssi_dbm=-62.7, preamble_detectable=yes | 437.2 | 549.977 | -20.5 | INFO | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 328.13 | 329.609 | -0.4 | PASS | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 52.35 | 394.224 | -86.7 | INFO | patidar2017 |
| distance_m=20.0, rx_power_dbm=-65.7, primary20_rssi_dbm=-71.7, preamble_detectable=yes | 315.36 | 394.224 | -20.0 | INFO | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 197.17 | 197.58 | -0.2 | PASS | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 55.69 | 295.762 | -81.2 | INFO | patidar2017 |
| distance_m=30.0, rx_power_dbm=-71.0, primary20_rssi_dbm=-77.0, preamble_detectable=yes | 147.89 | 295.762 | -50.0 | INFO | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 131.19 | 131.092 | 0.1 | PASS | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 178.99 | 197.58 | -9.4 | INFO | patidar2017 |
| distance_m=40.0, rx_power_dbm=-74.7, primary20_rssi_dbm=-80.8, preamble_detectable=yes | 131.91 | 197.58 | -33.2 | INFO | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 17.97 | 0.0 | inf | INFO | patidar2017 |
| distance_m=50.0, rx_power_dbm=-77.6, primary20_rssi_dbm=-83.7, preamble_detectable=no | 87.45 | 0.0 | inf | INFO | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 14.96 | 0.0 | inf | INFO | patidar2017 |
| distance_m=60.0, rx_power_dbm=-80.0, primary20_rssi_dbm=-86.0, preamble_detectable=no | 28.79 | 0.0 | inf | INFO | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=80.0, rx_power_dbm=-83.8, primary20_rssi_dbm=-89.8, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | PASS | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |
| distance_m=100.0, rx_power_dbm=-86.7, primary20_rssi_dbm=-92.7, preamble_detectable=no | 0.0 | 0.0 | 0.0 | INFO | patidar2017 |

## wifi7-dl-ofdma

DL OFDMA with EHT at 80 MHz vs SU

Command: `./ns3 run --no-build 'wifi-bench-dl-ofdma --outputDir=wifi-benchmarks-results --standard=be --channelWidth=80'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| scenario=saturated, n_stations=2, mode=SU, mcs=7 | 327.81 | 360.294 | -9.0 | PASS | ieee80211ax |
| scenario=saturated, n_stations=2, mode=DL-OFDMA, mcs=7 | 313.29 | 344.118 | -9.0 | PASS | ieee80211ax |
| scenario=saturated, n_stations=2, mode=gain-OFDMA-vs-SU, mcs=7 | 0.956 | 0.955 | 0.1 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=SU, mcs=7 | 0.264 | 0.264 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=DL-OFDMA, mcs=7 | 0.272 | 0.264 | 2.8 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=2, mode=gain-OFDMA-vs-SU, mcs=7 | 0.973 | 1.0 | -2.7 | INFO | khorov2019 |
| scenario=saturated, n_stations=4, mode=SU, mcs=7 | 328.13 | 360.294 | -8.9 | PASS | ieee80211ax |
| scenario=saturated, n_stations=4, mode=DL-OFDMA, mcs=7 | 312.84 | 344.118 | -9.1 | PASS | ieee80211ax |
| scenario=saturated, n_stations=4, mode=gain-OFDMA-vs-SU, mcs=7 | 0.953 | 0.955 | -0.2 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=SU, mcs=7 | 0.42 | 0.42 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=DL-OFDMA, mcs=7 | 0.31 | 0.42 | -26.1 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=4, mode=gain-OFDMA-vs-SU, mcs=7 | 1.352 | 1.0 | 35.2 | INFO | khorov2019 |
| scenario=saturated, n_stations=8, mode=SU, mcs=7 | 328.17 | 360.294 | -8.9 | PASS | ieee80211ax |
| scenario=saturated, n_stations=8, mode=DL-OFDMA, mcs=7 | 270.57 | 300.0 | -9.8 | PASS | ieee80211ax |
| scenario=saturated, n_stations=8, mode=gain-OFDMA-vs-SU, mcs=7 | 0.824 | 0.833 | -1.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=SU, mcs=7 | 1.044 | 1.044 | 0.0 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=DL-OFDMA, mcs=7 | 0.388 | 1.044 | -62.8 | INFO | khorov2019 |
| scenario=low-load-small-packets, n_stations=8, mode=gain-OFDMA-vs-SU, mcs=7 | 2.688 | 1.0 | 168.8 | INFO | khorov2019 |

## wifi7-ul-ofdma

UL OFDMA with EHT vs EDCA contention

Command: `./ns3 run --no-build 'wifi-bench-ul-ofdma --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| n_stations=4, mode=edca, mcs=7, width_mhz=20 | 72.06 | 69.15 | 4.2 | PASS | bianchi2000 |
| n_stations=4, mode=ofdma, mcs=7, width_mhz=20 | 62.44 | 70.588 | -11.5 | PASS | ieee80211ax |
| n_stations=4, mode=ofdma-bsrp, mcs=7, width_mhz=20 | 64.45 | 68.858 | -6.4 | PASS | ieee80211ax |
| n_stations=4, mode=ofdma-bsrp-muedca, mcs=7, width_mhz=20 | 52.0 | 70.588 | -26.3 | PASS | ieee80211ax |
| n_stations=8, mode=edca, mcs=7, width_mhz=20 | 65.33 | 63.346 | 3.1 | PASS | bianchi2000 |
| n_stations=8, mode=ofdma, mcs=7, width_mhz=20 | 64.86 | 70.588 | -8.1 | PASS | ieee80211ax |
| n_stations=8, mode=ofdma-bsrp, mcs=7, width_mhz=20 | 63.67 | 66.834 | -4.7 | PASS | ieee80211ax |
| n_stations=8, mode=ofdma-bsrp-muedca, mcs=7, width_mhz=20 | 52.03 | 70.588 | -26.3 | PASS | ieee80211ax |

## wifi7-spatial-reuse

OBSS-PD spatial reuse with EHT devices

Command: `./ns3 run --no-build 'wifi-bench-spatial-reuse --outputDir=wifi-benchmarks-results --standard=be'`

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| ap_distance_m=100, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 33.04 | 33.038 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 33.14 | 33.038 | 0.3 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 59.52 | 66.075 | -9.9 | PASS | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 59.58 | 66.075 | -9.8 | PASS | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 31.5 | 33.038 | -4.7 | INFO | wilhelmi2021 |
| ap_distance_m=100, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 31.45 | 33.038 | -4.8 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 33.78 | 33.779 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 33.5 | 33.779 | -0.8 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 59.83 | 67.559 | -11.4 | PASS | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 59.81 | 67.559 | -11.5 | PASS | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 50.62 | 33.779 | 49.9 | INFO | wilhelmi2021 |
| ap_distance_m=150, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 31.47 | 33.779 | -6.8 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=none, mcs=3, width_mhz=20 | 62.09 | 62.089 | 0.0 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-82, mcs=3, width_mhz=20 | 62.18 | 62.089 | 0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-77, mcs=3, width_mhz=20 | 62.15 | 62.089 | 0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-72, mcs=3, width_mhz=20 | 62.05 | 62.089 | -0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-67, mcs=3, width_mhz=20 | 62.17 | 62.089 | 0.1 | INFO | wilhelmi2021 |
| ap_distance_m=300, obss_pd_level_dbm=-62, mcs=3, width_mhz=20 | 62.24 | 62.089 | 0.2 | INFO | wilhelmi2021 |

## wifi7-mlo-throughput

Multi-link operation throughput scaling with 1, 2 and 3 links vs the sum of link bounds

Command: `./ns3 run --no-build 'wifi-bench-mlo-throughput --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| n_links=1, links=5GHz/80MHz, mcs=11, n_stations=1 | 546.69 | 549.977 | -0.6 | PASS | lopezraventos2022 |
| n_links=2, links=5GHz/80MHz+6GHz/80MHz, mcs=11, n_stations=1 | 1095.03 | 1097.311 | -0.2 | PASS | lopezraventos2022 |
| n_links=3, links=5GHz/80MHz+6GHz/80MHz+2.4GHz/40MHz, mcs=11, n_stations=1 | 1352.0 | 1359.415 | -0.5 | PASS | lopezraventos2022 |

## wifi7-mlo-latency

MLO latency under asymmetric contention vs single-link devices

Command: `./ns3 run --no-build 'wifi-bench-mlo-latency --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=sld-contended-link, n_links=1, n_stations=2, load_mbps=20.0 | 22.983 | 22.983 | 0.0 | INFO | carrascosa2023 |
| mode=sld-idle-link, n_links=1, n_stations=2, load_mbps=20.0 | 0.493 | 22.983 | -97.9 | INFO | carrascosa2023 |
| mode=mlo-both-links, n_links=2, n_stations=2, load_mbps=20.0 | 0.479 | 22.983 | -97.9 | PASS | carrascosa2023 |

## wifi7-emlsr

EMLSR single-radio client vs single-link and dual-radio MLD

Command: `./ns3 run --no-build 'wifi-bench-emlsr --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=sld, scenario=saturated-dl, mcs=7, padding_delay_us=32 | 327.82 | 329.609 | -0.5 | PASS | ieee80211be |
| mode=sld, scenario=low-load-dl-obss-on-link0, mcs=7, padding_delay_us=32 | 14.111 | 14.111 | 0.0 | INFO | galati2024 |
| mode=mld, scenario=saturated-dl, mcs=7, padding_delay_us=32 | 656.57 | 327.82 | 100.3 | INFO | chen2022 |
| mode=mld, scenario=low-load-dl-obss-on-link0, mcs=7, padding_delay_us=32 | 0.276 | 14.111 | -98.0 | INFO | galati2024 |
| mode=emlsr, scenario=saturated-dl, mcs=7, padding_delay_us=32 | 318.97 | 327.82 | -2.7 | INFO | chen2022 |
| mode=emlsr, scenario=low-load-dl-obss-on-link0, mcs=7, padding_delay_us=32 | 0.58 | 14.111 | -95.9 | INFO | galati2024 |

## wifi7-tid-to-link

TID-to-link mapping: voice latency isolation from best-effort load

Command: `./ns3 run --no-build 'wifi-bench-tid-to-link --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mapping=default-all-links, n_stations=2, mcs=7, be_throughput_mbps=646.18 | 5.254 | 5.254 | 0.0 | INFO | lopezraventos2022 |
| mapping=VO->link1,others->link0, n_stations=2, mcs=7, be_throughput_mbps=326.74 | 0.073 | 5.254 | -98.6 | PASS | lopezraventos2022 |

## wifi7-dynamic-bw

Dynamic bandwidth operation under partial-band interference vs the puncturing bound

Command: `./ns3 run --no-build 'wifi-bench-dynamic-bw --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| interfered_subchannel=secondary40, duty_cycle=0.0, mcs=7, full_band_bound_mbps=329.61 | 327.83 | 329.609 | -0.5 | PASS | ieee80211be |
| interfered_subchannel=secondary40, duty_cycle=0.25, mcs=7, full_band_bound_mbps=329.61 | 219.35 | 286.606 | -23.5 | INFO | ns3-wifi |
| interfered_subchannel=secondary40, duty_cycle=0.5, mcs=7, full_band_bound_mbps=329.61 | 152.79 | 243.604 | -37.3 | INFO | ns3-wifi |
| interfered_subchannel=secondary40, duty_cycle=0.75, mcs=7, full_band_bound_mbps=329.61 | 135.82 | 200.601 | -32.3 | INFO | ns3-wifi |
| interfered_subchannel=secondary40, duty_cycle=1.0, mcs=7, full_band_bound_mbps=329.61 | 156.79 | 157.599 | -0.5 | INFO | ns3-wifi |

## wifi8-uhr-kpi

UHR KPI baselines (5th-percentile throughput, P95 latency) in a dense co-channel deployment

Command: `./ns3 run --no-build 'wifi-bench-uhr-kpi --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=wifi6, n_bss=4, stations_per_bss=4, width_mhz=80 | 14.31 | 19.676 | -27.3 | INFO | ieee80211bn-par |
| mode=wifi7-mlo, n_bss=4, stations_per_bss=4, width_mhz=80 | 19.676 | 14.31 | 37.5 | PASS | galati2024 |
| mode=wifi7-mlo-sr, n_bss=4, stations_per_bss=4, width_mhz=80 | 19.557 | 19.676 | -0.6 | INFO | ieee80211bn-par |

## wifi8-multi-ap

Multi-AP coordination emulation: uncoordinated vs Co-TDMA vs Co-SR

Command: `./ns3 run --no-build 'wifi-bench-multi-ap --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=uncoordinated, ap_distance_m=25.0, stations_per_bss=2, width_mhz=20 | 29.863 | 29.863 | 0.0 | INFO | ieee80211bn-par |
| mode=co-tdma, ap_distance_m=25.0, stations_per_bss=2, width_mhz=20 | 29.162 | 29.863 | -2.3 | INFO | ieee80211bn-par |
| mode=co-sr, ap_distance_m=25.0, stations_per_bss=2, width_mhz=20 | 29.228 | 29.863 | -2.1 | INFO | ieee80211bn-par |

## wifi8-roaming

BSS transition loss and interruption (mobility objective baseline)

Command: `./ns3 run --no-build 'wifi-bench-roaming --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| trigger=missed-beacons, ap_distance_m=60.0, speed_mps=6.0, max_missed_beacons=10 | 0.0807 | 0.081 | 0.0 | INFO | ieee80211bn-par |
| trigger=forced-at-60pct, ap_distance_m=60.0, speed_mps=6.0, max_missed_beacons=10 | 0.0003 | 0.081 | -99.6 | INFO | ieee80211bn-par |

## wifi8-mlo-reliability

Multi-link reliability under hidden interference (deadline miss ratio)

Command: `./ns3 run --no-build 'wifi-bench-mlo-reliability --outputDir=wifi-benchmarks-results

| Row | Measured | Reference | Deviation (%) | Verdict | Source |
|---|---|---|---|---|---|
| mode=sld-jammed-link, mcs=7, interferer_dbm=0.0, duty_cycle=0.5 | 0.4582 | 0.458 | 0.0 | INFO | galati2024 |
| mode=sld-clean-link, mcs=7, interferer_dbm=0.0, duty_cycle=0.5 | 0.0 | 0.458 | -100.0 | INFO | galati2024 |
| mode=mlo, mcs=7, interferer_dbm=0.0, duty_cycle=0.5 | 0.187 | 0.458 | -59.2 | PASS | galati2024 |
