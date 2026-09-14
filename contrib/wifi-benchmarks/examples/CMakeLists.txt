set(wifi_benchmark_libs
    ${libwifi-benchmarks}
    ${libwifi}
    ${libinternet}
    ${libapplications}
    ${libmobility}
    ${libspectrum}
    ${libpropagation}
    ${libenergy}
    ${libinternet-apps}
    ${libflow-monitor}
    ${libcsma}
    ${libbridge}
)

build_lib_example(
  NAME wifi-bench-phy-rates
  SOURCE_FILES wifi-bench-phy-rates.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-per-vs-snr
  SOURCE_FILES wifi-bench-per-vs-snr.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-aggregation
  SOURCE_FILES wifi-bench-aggregation.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-edca-bianchi
  SOURCE_FILES wifi-bench-edca-bianchi.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-rate-adaptation
  SOURCE_FILES wifi-bench-rate-adaptation.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-dl-ofdma
  SOURCE_FILES wifi-bench-dl-ofdma.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-ul-ofdma
  SOURCE_FILES wifi-bench-ul-ofdma.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-spatial-reuse
  SOURCE_FILES wifi-bench-spatial-reuse.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-power-save
  SOURCE_FILES wifi-bench-power-save.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-mlo-throughput
  SOURCE_FILES wifi-bench-mlo-throughput.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-mlo-latency
  SOURCE_FILES wifi-bench-mlo-latency.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-emlsr
  SOURCE_FILES wifi-bench-emlsr.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-tid-to-link
  SOURCE_FILES wifi-bench-tid-to-link.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-dynamic-bw
  SOURCE_FILES wifi-bench-dynamic-bw.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-uhr-kpi
  SOURCE_FILES wifi-bench-uhr-kpi.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-multi-ap
  SOURCE_FILES wifi-bench-multi-ap.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-roaming
  SOURCE_FILES wifi-bench-roaming.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)

build_lib_example(
  NAME wifi-bench-mlo-reliability
  SOURCE_FILES wifi-bench-mlo-reliability.cc
  LIBRARIES_TO_LINK ${wifi_benchmark_libs}
)
