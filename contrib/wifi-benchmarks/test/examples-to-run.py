#! /usr/bin/env python3

# Examples of the wifi-benchmarks module run by test.py in quick mode. Each tuple contains
# (example_name, do_run, do_valgrind_run).
cpp_examples = [
    ("wifi-bench-phy-rates --quick --standard=ax", "True", "False"),
    ("wifi-bench-phy-rates --quick --standard=be", "True", "False"),
    ("wifi-bench-per-vs-snr --quick --standard=ax", "True", "False"),
    ("wifi-bench-per-vs-snr --quick --standard=be", "True", "False"),
    ("wifi-bench-aggregation --quick --standard=ax", "True", "False"),
    ("wifi-bench-aggregation --quick --standard=be", "True", "False"),
    ("wifi-bench-edca-bianchi --quick --standard=ax", "True", "False"),
    ("wifi-bench-edca-bianchi --quick --standard=be", "True", "False"),
    ("wifi-bench-rate-adaptation --quick --standard=ax", "True", "False"),
    ("wifi-bench-rate-adaptation --quick --standard=be", "True", "False"),
    ("wifi-bench-dl-ofdma --quick --standard=ax", "True", "False"),
    ("wifi-bench-dl-ofdma --quick --standard=be", "True", "False"),
    ("wifi-bench-ul-ofdma --quick --standard=ax", "True", "False"),
    ("wifi-bench-ul-ofdma --quick --standard=be", "True", "False"),
    ("wifi-bench-spatial-reuse --quick --standard=ax", "True", "False"),
    ("wifi-bench-spatial-reuse --quick --standard=be", "True", "False"),
    ("wifi-bench-power-save --quick --standard=ax", "True", "False"),
    ("wifi-bench-mlo-throughput --quick", "True", "False"),
    ("wifi-bench-mlo-latency --quick", "True", "False"),
    ("wifi-bench-emlsr --quick", "True", "False"),
    ("wifi-bench-tid-to-link --quick", "True", "False"),
    ("wifi-bench-dynamic-bw --quick", "True", "False"),
    ("wifi-bench-uhr-kpi --quick", "True", "False"),
    ("wifi-bench-multi-ap --quick", "True", "False"),
    ("wifi-bench-roaming --quick", "True", "False"),
    ("wifi-bench-mlo-reliability --quick", "True", "False"),
]

python_examples = []
