#! /usr/bin/env python3

# Examples of the uwb module run by test.py. Each tuple contains
# (example_name, do_run, do_valgrind_run). The arguments keep every run short;
# the defaults of each example are what a reader should actually use.
cpp_examples = [
    ("uwb-ranging-comparison --exchanges=20", "True", "False"),
    ("uwb-link-budget --frames=10", "True", "False"),
    ("uwb-positioning --rounds=10", "True", "False"),
    ("uwb-positioning --rounds=10 --anchors=4 --method=SS-TWR --sync=1", "True", "False"),
    ("uwb-tag-capacity --seconds=0.5", "True", "False"),
    ("uwb-wifi-coexistence --seconds=0.3", "True", "False"),
]

python_examples = []
