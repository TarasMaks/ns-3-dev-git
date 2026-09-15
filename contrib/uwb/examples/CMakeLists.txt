set(uwb_example_libs
    ${libuwb}
    ${libcore}
    ${libnetwork}
    ${libmobility}
    ${libspectrum}
    ${libpropagation}
)

build_lib_example(
  NAME uwb-ranging-comparison
  SOURCE_FILES uwb-ranging-comparison.cc
  LIBRARIES_TO_LINK ${uwb_example_libs}
)

build_lib_example(
  NAME uwb-link-budget
  SOURCE_FILES uwb-link-budget.cc
  LIBRARIES_TO_LINK ${uwb_example_libs}
)

build_lib_example(
  NAME uwb-positioning
  SOURCE_FILES uwb-positioning.cc
  LIBRARIES_TO_LINK ${uwb_example_libs}
)

build_lib_example(
  NAME uwb-tag-capacity
  SOURCE_FILES uwb-tag-capacity.cc
  LIBRARIES_TO_LINK ${uwb_example_libs}
)

build_lib_example(
  NAME uwb-wifi-coexistence
  SOURCE_FILES uwb-wifi-coexistence.cc
  LIBRARIES_TO_LINK
    ${uwb_example_libs}
    ${libwifi}
    ${libinternet}
    ${libapplications}
)
