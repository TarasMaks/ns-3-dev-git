#! /usr/bin/env python3

# Examples of the ble module run by test.py. Each tuple contains
# (example_name, do_run, do_valgrind_run).
cpp_examples = [
    ("ble-connection-throughput --phy=1M --payload=251 --interval=30ms --duration=300ms",
     "True", "False"),
    ("ble-range-phy-modes --distances=10,100", "True", "False"),
    ("ble-advertising-discovery --advertisers=3 --duration=2s", "True", "False"),
    ("ble-piconet --peripherals=3 --duration=1s", "True", "False"),
    ("ble-wifi-coexistence --duration=300ms", "True", "False"),
]

python_examples = []
