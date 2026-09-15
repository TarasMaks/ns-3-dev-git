set(ble_example_libs
    ${libble}
    ${libcore}
    ${libnetwork}
    ${libmobility}
    ${libspectrum}
    ${libpropagation}
)

build_lib_example(
  NAME ble-connection-throughput
  SOURCE_FILES ble-connection-throughput.cc
  LIBRARIES_TO_LINK ${ble_example_libs}
)

build_lib_example(
  NAME ble-range-phy-modes
  SOURCE_FILES ble-range-phy-modes.cc
  LIBRARIES_TO_LINK ${ble_example_libs}
)

build_lib_example(
  NAME ble-advertising-discovery
  SOURCE_FILES ble-advertising-discovery.cc
  LIBRARIES_TO_LINK ${ble_example_libs}
)

build_lib_example(
  NAME ble-piconet
  SOURCE_FILES ble-piconet.cc
  LIBRARIES_TO_LINK ${ble_example_libs}
)

build_lib_example(
  NAME ble-wifi-coexistence
  SOURCE_FILES ble-wifi-coexistence.cc
  LIBRARIES_TO_LINK
    ${ble_example_libs}
    ${libwifi}
    ${libinternet}
    ${libapplications}
)
