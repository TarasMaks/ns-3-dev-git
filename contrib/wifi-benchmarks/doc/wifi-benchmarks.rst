
.. highlight:: cpp

Wi-Fi 6/7/8 benchmark suite (wifi-benchmarks)
----------------------------------------------

The ``wifi-benchmarks`` contrib module provides experimental simulation scripts that exercise
the IEEE 802.11ax (Wi-Fi 6), IEEE 802.11be (Wi-Fi 7) and IEEE 802.11bn (Wi-Fi 8) capabilities
modeled by the ns-3 wifi module and compare the results with figures published in the standards
and in the literature. See ``contrib/wifi-benchmarks/README.md`` for the complete description of
the scripts, the references and the coverage table.

Model Description
*****************

The module is made of three parts:

* ``model/wifi-literature-reference``: reference data and analytical models independent from
  the wifi implementation (HE/EHT MCS tables and data rate formula, receiver sensitivity of the
  standard, EDCA parameters, single-user goodput bound, Bianchi saturation model with A-MPDU
  aggregation, objectives of the P802.11bn PAR, citations);
* ``helper/wifi-benchmark-helper``: a BSS builder (single-link and multi-link devices, OFDMA
  scheduler, EMLSR, TID-to-link mapping, BSS color and OBSS-PD, static association), traffic
  sources and flow measurements (throughput and latency percentiles, loss, deadline misses,
  reception gaps), PPDU accounting, energy monitoring and result tables (CSV/JSON);
* ``examples/``: the benchmark scripts, each writing a table of measured values, references,
  deviations and PASS/FAIL/INFO verdicts.

Usage
*****

Run one benchmark with ``./ns3 run "wifi-bench-phy-rates --standard=be"`` or the whole suite with
``python3 contrib/wifi-benchmarks/scripts/run-suite.py --profile full``. All scripts accept
``--quick`` (short runs used by the regression tests), ``--outputDir``, ``--rngRun`` and
``--verbose``.

Benchmarks
==========

Wi-Fi 6 and Wi-Fi 7 (``--standard=ax|be``): ``wifi-bench-phy-rates``, ``wifi-bench-per-vs-snr``,
``wifi-bench-aggregation``, ``wifi-bench-edca-bianchi``, ``wifi-bench-rate-adaptation``,
``wifi-bench-dl-ofdma``, ``wifi-bench-ul-ofdma``, ``wifi-bench-spatial-reuse``,
``wifi-bench-power-save``.

Wi-Fi 7 only: ``wifi-bench-mlo-throughput``, ``wifi-bench-mlo-latency``, ``wifi-bench-emlsr``,
``wifi-bench-tid-to-link``, ``wifi-bench-dynamic-bw``.

Wi-Fi 8 baselines and emulations (802.11bn is not modeled by ns-3): ``wifi-bench-uhr-kpi``,
``wifi-bench-multi-ap``, ``wifi-bench-roaming``, ``wifi-bench-mlo-reliability``.

Validation
**********

The ``wifi-benchmarks`` test suite checks the reference formulas against the values published in
the standard tables and against the ns-3 PHY rate functions, and the Bianchi model port against
the values tabulated in ``src/wifi/examples/wifi-bianchi.cc``. The examples are run in quick mode
by ``test.py``.
